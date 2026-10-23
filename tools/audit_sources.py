"""Inventory every maintained application/FBL source and enforce ownership boundaries."""
from pathlib import Path
import hashlib,json,re
ROOT=Path(__file__).resolve().parents[1]
def main():
    roots=['ELC_APP/BSW','ELC_APP/APPL','ELC_APP/Src','ELC_APP/Inc','ELC_FBL/Core']
    files=sorted({p for root in roots for p in (ROOT/root).rglob('*') if p.suffix in ('.c','.h','.s')})
    files+=list(ROOT.glob('ELC_*/STM32F103C8TX_FLASH.ld'))
    inventory=[]
    for p in files:
        text=p.read_text(errors='replace')
        inventory.append({'path':p.relative_to(ROOT).as_posix(),'lines':len(text.splitlines()),
                          'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),
                          'functions':re.findall(r'^\w[^;\n]*?\b(\w+)\([^;\n]*\)\s*\n?\{',text,re.M)})
    dcm=(ROOT/'ELC_APP/BSW/Diag/Dcm/Dcm.c').read_text()
    dem=(ROOT/'ELC_APP/BSW/Diag/Dem/Dem.c').read_text()
    assert not any(s in dcm for s in ['HAL_CAN_','PCI_','Dem_DTC_Stat[','Dem_FF['])
    assert not any(s in dem for s in ['HAL_CAN_','HAL_FLASH_'])
    appcan=[p for p in files if 'ELC_APP' in str(p) and p.suffix=='.c' and 'HAL_CAN_AddTxMessage(' in p.read_text()]
    assert [p.relative_to(ROOT).as_posix() for p in appcan]==['ELC_APP/BSW/Com/Can/Can.c']
    assert not any((ROOT/'ELC_APP/BSW'/name).exists() for name in ['Rte','RTE','Det','DET','MemMap'])
    out=ROOT/'docs/modernization/SOURCE_INVENTORY.json'
    out.write_text(json.dumps(inventory,indent=2)+'\n')
    print(f'{len(inventory)} source/linker files inventoried; CAN ownership, payload-only DCM and DEM boundaries PASS')
if __name__=='__main__':main()
