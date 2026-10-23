from pathlib import Path
import os,re,subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/tests';OUT.mkdir(parents=True,exist_ok=True)
ZIG=ROOT/'.tools/native/ziglang/zig.exe'
os.environ['ZIG_LOCAL_CACHE_DIR']=str(OUT/'zig-local')
os.environ['ZIG_GLOBAL_CACHE_DIR']=str(OUT/'zig-global')
def run(name,sources,extra=()):
    exe=OUT/(name+'.exe')
    cmd=[str(ZIG),'cc','-std=c11','-O0','-g','-UNDEBUG','-Wall','-Wextra','-Werror',
         '-Wno-unused-function','-Wno-unused-parameter','-Wno-misleading-indentation',
         '-Wno-int-to-pointer-cast', '-Itests/mocks','-IELC_APP/Inc',
         '-IELC_APP/BSW/Nvm','-IELC_APP/BSW/Diag/Dcm','-IELC_APP/BSW/Diag/Dem',
         '-IELC_APP/BSW/Sys/EcuM','-IELC_APP/APPL/SMon',*extra,*sources,'-o',str(exe)]
    subprocess.run(cmd,cwd=ROOT,check=True)
    subprocess.run([str(exe)],cwd=ROOT,check=True)
    print(name+': PASS',flush=True)
if __name__=='__main__':
    # The hardware mock defines existing application RAM symbols, using the real header types.
    smon=(ROOT/'ELC_APP/APPL/SMon/SMon.h').read_text()
    declarations=re.findall(r'^extern\s+([^;\n]+);',smon,re.M)
    globals='\n'.join(d+';' for d in declarations if '(' not in d)
    (OUT/'smon_globals.c').write_text('#include "SMon.h"\n'+globals+'\n')
    run('nvm_store',['tests/nvm_store_test.c','ELC_APP/BSW/Nvm/NvmStore.c'])
    sources=[str(p.relative_to(ROOT)) for p in (ROOT/'ELC_APP/BSW/Com').rglob('*.c') if p.parent.name!='CanH']
    sources+=['ELC_APP/BSW/Diag/Dcm/Dcm.c','ELC_APP/BSW/Diag/Dem/Dem.c','ELC_APP/BSW/Nvm/Nvm.c',
              'ELC_APP/BSW/Nvm/NvmStore.c','build/tests/smon_globals.c','tests/comm_integration_test.c']
    run('bsw_stack',sources)
    for block in (1,2):run('boot_'+str(block),['tests/boot_protocol_test.c'],['-DBOOT_TEST_BLOCK='+str(block)])
