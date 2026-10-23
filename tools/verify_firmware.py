"""ELF32 ARM memory and SRAM dependency checks; no third-party packages required."""
from pathlib import Path
import struct,sys
ROOT=Path(__file__).resolve().parents[1]
def inspect(name):
    p=ROOT/'build'/name/(name+'.elf');data=p.read_bytes()
    assert data[:5]==b'\x7fELF\x01' and struct.unpack_from('<H',data,18)[0]==40,'Not ARM ELF32'
    shoff=struct.unpack_from('<I',data,32)[0];ents,num,strings=struct.unpack_from('<HHH',data,46)
    headers=[struct.unpack_from('<10I',data,shoff+i*ents) for i in range(num)]
    strhdr=headers[strings];names=data[strhdr[4]:strhdr[4]+strhdr[5]]
    def string(buf,off):return buf[off:buf.index(0,off)].decode()
    sections={string(names,h[0]):(i,h) for i,h in enumerate(headers)}
    syms=[]
    h=sections['.symtab'][1];strh=headers[h[6]];strs=data[strh[4]:strh[4]+strh[5]]
    for off in range(h[4],h[4]+h[5],h[9]):
        n,v,size,info,other,section=struct.unpack_from('<IIIBBH',data,off)
        syms.append((string(strs,n),v,size,section))
    symbols={s[0]:s[1] for s in syms}
    assert symbols['_ebss']<=symbols['_estack']<=0x20004FC0
    assert symbols.get('_encr',0x20004FC0)<=0x20005000
    if name=='ELC_APP':
        assert symbols['_sidata']+symbols['_edata']-symbols['_sdata']<=0x0800F7FC
        assert symbols['Dcm_ActiveSessionState']==0x20004FC0
        print('APP: Flash/CRC footer, SRAM and retained handover PASS')
        return
    start=symbols['__ram_blob_vma_start'];end=symbols['__ram_blob_vma_end']
    assert start==0x20000000 and end<=0x20004B80
    assert symbols['__ram_blob_load_end']<=0x08003800
    h=sections['.ram_blob'][1];sp,pc=struct.unpack_from('<II',data,h[4])
    assert sp==0x20004F80 and pc&1 and start<=pc<end
    # Every relocation emitted from the copied blob must target that blob or an absolute symbol.
    count=0
    for name2,(i,h) in sections.items():
        if h[1]!=9 or headers[h[7]][3]!=start:continue
        for off in range(h[4],h[4]+h[5],h[9]):
            where,info=struct.unpack_from('<II',data,off);symbol=syms[info>>8];kind=info&255
            if kind==0:continue
            n,v,size,sec=symbol
            assert (sec==0xFFF1 and (v==0 or v>=0x40000000)) or start<=v<end,f'SRAM dependency escapes blob at {where:#x}: {n} -> {v:#x}'
            count+=1
    assert count>20,'No retained relocation evidence'
    print(f'FBL: vectors, VMA/LMA, 1 KiB updater stack margin and {count} SRAM relocations PASS')
if __name__=='__main__':
    for name in sys.argv[1:] or ['ELC_APP','ELC_FBL']:inspect(name)
