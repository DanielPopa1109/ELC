"""Reproducible Cortex-M3 build, using only sources inside ELC.
python tools/build_firmware.py [ELC_APP|ELC_FBL]
Set ARM_GCC_BIN to use an installed GNU Arm compiler.
"""
from pathlib import Path
import os, re, subprocess, sys, concurrent.futures, binascii, struct, hashlib, json
ROOT=Path(__file__).resolve().parents[1]
BIN=Path(os.environ.get('ARM_GCC_BIN',ROOT/'.tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin'))
def run(args):
    p=subprocess.run([str(x) for x in args],cwd=ROOT,capture_output=True,text=True)
    if p.stdout: print(p.stdout,end='')
    if p.stderr: print(p.stderr,end='')
    if p.returncode: raise RuntimeError('Command failed: '+' '.join(str(x) for x in args[:3]))
def build(name):
    project=ROOT/name; out=ROOT/'build'/name;out.mkdir(parents=True,exist_ok=True)
    src=list((project/('Src' if name=='ELC_APP' else 'Core/Src')).glob('*.c'))
    if name=='ELC_APP':
        src+=list((project/'BSW').rglob('*.c'))+list((project/'APPL').rglob('*.c'))
    # HAL selection matches the project's generated build configuration.
    halroot=ROOT/'ELC_APP/Drivers/STM32F1xx_HAL_Driver'
    selection=(ROOT/'ELC_APP/Debug/Drivers/STM32F1xx_HAL_Driver/Src/subdir.mk').read_text()
    for filename in sorted(set(re.findall(r'../Drivers/STM32F1xx_HAL_Driver/Src/(\w+\.c)',selection))):
        src.append(halroot/'Src'/filename)
    inc=[project/('Inc' if name=='ELC_APP' else 'Core/Inc'),halroot/'Inc',halroot/'Inc/Legacy',
         ROOT/'ELC_APP/Drivers/CMSIS/Include',ROOT/'ELC_APP/Drivers/CMSIS/Device/ST/STM32F1xx/Include']
    if name=='ELC_APP':
        inc += sorted({p.parent for p in (project/'BSW').rglob('*.h')}|{p.parent for p in (project/'APPL').rglob('*.h')})
        freertos=project/'Middlewares/Third_Party/FreeRTOS/Source'
        inc += [freertos/'include',freertos/'CMSIS_RTOS_V2',freertos/'portable/GCC/ARM_CM3']
        src += list(freertos.glob('*.c'))+[freertos/'CMSIS_RTOS_V2/cmsis_os2.c',freertos/'portable/GCC/ARM_CM3/port.c',freertos/'portable/MemMang/heap_5.c']
    startup=next((project/'Core/Startup').glob('*.s'))
    src.append(startup)
    common=['-mcpu=cortex-m3','-mthumb','-mfloat-abi=soft','-Os','-g3','-ffunction-sections','-fdata-sections','-fno-strict-aliasing',
            '-Wall','-Wextra','-Wno-unused-parameter','-DUSE_HAL_DRIVER','-DSTM32F103xB','--specs=nano.specs']
    includes=[f'-I{p}' for p in inc]
    objects=[]
    def compile_one(item):
        i,source=item;obj=out/(str(i)+'_'+source.stem+'.o');objects.append(obj)
        flags=['-fno-builtin','-fno-jump-tables','-fno-tree-switch-conversion','-mslow-flash-data'] if source.name=='ram_blob.c' else []
        run([BIN/'arm-none-eabi-gcc.exe',*common,*includes,*flags,'-fstack-usage','-c',source,'-o',obj])
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:list(pool.map(compile_one,enumerate(sorted(set(src)))))
    objects.sort()
    rsp=out/'objects.rsp';rsp.write_text('\n'.join('"'+str(p)+'"' for p in objects))
    elf=out/(name+'.elf')
    run([BIN/'arm-none-eabi-gcc.exe',*common,'@'+str(rsp),'-T'+str(project/'STM32F103C8TX_FLASH.ld'),
         '--specs=nosys.specs','-Wl,--gc-sections','-Wl,--emit-relocs',f'-Wl,-Map={out/name}.map','-Wl,--print-memory-usage',
         '-Wl,--start-group','-lc','-lm','-Wl,--end-group','-o',elf])
    run([BIN/'arm-none-eabi-size.exe',elf])
    for fmt,suffix in [('ihex','hex'),('binary','bin')]:run([BIN/'arm-none-eabi-objcopy.exe','-O',fmt,elf,out/(name+'.'+suffix)])
    p=subprocess.run([str(BIN/'arm-none-eabi-objdump.exe'),'-h','-dr',str(elf)],capture_output=True,text=True,check=True)
    (out/(name+'.dis')).write_text(p.stdout)
    if name=='ELC_APP':
        # A debugger-loadable full image; routed RequestDownload excludes the CRC word.
        raw=(out/(name+'.bin')).read_bytes()
        assert len(raw)<=0xBBFC
        padded=raw+b'\xff'*(0xBBFC-len(raw))
        (out/(name+'.program.bin')).write_bytes(padded)
        (out/(name+'.bootable.bin')).write_bytes(padded+struct.pack('<I',binascii.crc32(padded)&0xFFFFFFFF))
    manifest={'image':name,'compiler':subprocess.check_output([str(BIN/'arm-none-eabi-gcc.exe'),'--version'],text=True).splitlines()[0],
              'sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob(name+'.*') if p.suffix in ('.elf','.bin','.hex')}}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'{name}: Cortex-M3 build and link PASS')
if __name__=='__main__':
    for name in sys.argv[1:] or ['ELC_APP','ELC_FBL']:build(name)
