"""Inventory every maintained application/FBL source and enforce the architecture rules of the ELC lab
stack (AUTOSAR-shaped modules called directly, no RTE).
python tools/audit_sources.py   -> docs/modernization/SOURCE_INVENTORY.json"""
from pathlib import Path
import hashlib, json, re, sys
ROOT = Path(__file__).resolve().parents[1]
BSW = ROOT / 'ELC_APP/BSW'

def text(rel):
    return (ROOT / rel).read_text(errors='replace')

def includes(rel):
    return re.findall(r'#include\s+"([^"]+)"', text(rel))

def main():
    roots = ['ELC_APP/BSW', 'ELC_APP/APPL', 'ELC_APP/Src', 'ELC_APP/Inc', 'ELC_FBL/Core']
    files = sorted({p for root in roots for p in (ROOT / root).rglob('*') if p.suffix in ('.c', '.h', '.s')})
    files += list(ROOT.glob('ELC_*/STM32F103C8TX_FLASH.ld'))
    inventory = []
    for p in files:
        t = p.read_text(errors='replace')
        inventory.append({'path': p.relative_to(ROOT).as_posix(), 'lines': len(t.splitlines()),
                          'sha256': hashlib.sha256(p.read_bytes()).hexdigest(),
                          'functions': re.findall(r'^\w[^;\n]*?\b(\w+)\([^;\n]*\)\s*\n?\{', t, re.M)})
    failures = []
    def check(condition, message):
        if not condition:
            failures.append(message)

    # ---- no RTE / DET / MemMap, no legacy APIs --------------------------------------------------------------
    check(not any((BSW / n).exists() for n in ['Rte', 'RTE', 'Det', 'DET', 'MemMap']), 'RTE/DET/MemMap directory present')
    app_sources = [p for p in files if 'ELC_APP' in p.parts and p.suffix in ('.c', '.h') and 'Drivers' not in p.parts]
    legacy = re.compile(r'\b(Dem_SetDtc|Dem_GetDtcStatus|Nvm_[A-Z]\w*|NvmStore_\w+|MemIf_(Read|Erase|Program)\b|CanIf_TransmitXcp|'
                        r'CanIf_TransmitDiagnostic|CanIf_TransmitTelemetry|CanIf_SetCommunicationControl|Com_QueueTelemetry|'
                        r'Com_SetLoadStatus|Dcm_main|Dem_main|EcuM_main|Can_QueueRx)\b')
    for p in app_sources:
        hit = legacy.search(p.read_text(errors='replace'))
        check(hit is None, f'legacy API {hit.group(0) if hit else ""} in {p.relative_to(ROOT).as_posix()}')
    check(not re.search(r'\bRte_|\bDet_ReportError|\bMemMap\.h', ' '.join(p.read_text(errors='replace') for p in app_sources)),
          'RTE/DET/MemMap reference in APP sources')

    # ---- hardware ownership --------------------------------------------------------------------------------------
    def owners(token, subset):
        return sorted(p.relative_to(ROOT).as_posix() for p in subset if p.suffix == '.c' and token in p.read_text(errors='replace'))
    app_c = [p for p in app_sources if p.suffix == '.c']
    check(owners('HAL_CAN_AddTxMessage(', app_c) == ['ELC_APP/BSW/Com/Can/Can.c'], 'only Can.c may submit CAN frames')
    check(owners('HAL_CAN_GetRxMessage(', app_c) == ['ELC_APP/BSW/Com/Can/Can.c'], 'only Can.c may read the RX FIFO')
    check(owners('HAL_FLASHEx_Erase(', app_c) == ['ELC_APP/BSW/Mem/Fls/Fls.c'], 'only Fls.c may erase Flash')
    check(owners('HAL_FLASH_Program(', app_c) == ['ELC_APP/BSW/Mem/Fls/Fls.c'], 'only Fls.c may program Flash')

    # ---- layering: who may include / call whom ------------------------------------------------------------------------
    def forbid(rel, names, why):
        inc = [Path(i).name for i in includes(rel)]
        for n in names:
            check(n not in inc, f'{rel} must not include {n} ({why})')
    forbid('ELC_APP/BSW/Diag/Dcm/Dcm.c', ['SMon.h', 'EcuM.h', 'NvM.h', 'Can.h', 'CanIf.h', 'CanTp.h', 'Com.h', 'Fls.h', 'Fee.h'],
           'Dcm talks to PduR, Dem, ComM and the DcmAppl callouts only')
    forbid('ELC_APP/BSW/Diag/Dem/Dem.c', ['SMon.h', 'EcuM.h', 'Can.h', 'CanIf.h', 'Fls.h', 'Fee.h'], 'Dem uses NvM and Com only')
    forbid('ELC_APP/BSW/Com/CanTp/CanTp.c', ['Dcm.h', 'Can.h', 'Com.h', 'ComM.h'], 'CanTp sits between CanIf and PduR')
    forbid('ELC_APP/BSW/Com/Com/Com.c', ['CanIf.h', 'Can.h', 'CanTp.h', 'Dcm.h'], 'Com reaches the bus through PduR')
    forbid('ELC_APP/BSW/Com/PduR/PduR.c', ['Can.h', 'Fls.h', 'Fee.h', 'NvM.h', 'Dem.h', 'SMon.h'], 'PduR routes between CanIf/CanTp and Com/Dcm')
    forbid('ELC_APP/BSW/Mem/Nvm/NvM.c', ['Dem.h', 'Dcm.h', 'SMon.h', 'Can.h', 'CanIf.h'], 'NvM is configured through NvM_Cfg.c')
    forbid('ELC_APP/BSW/Mem/Fee/Fee.c', ['NvM.h', 'Dem.h', 'SMon.h'], 'Fee sits on Fls')
    forbid('ELC_APP/BSW/Com/CanIf/CanIf.c', ['Com.h', 'Dcm.h', 'Dem.h', 'Fls.h'], 'CanIf dispatches by configuration')
    forbid('ELC_APP/BSW/Com/Can/Can.c', ['Com.h', 'PduR.h', 'CanTp.h', 'Dcm.h'], 'the driver only talks to CanIf')
    check(not re.search(r'\bSMon_\w+|\bEcuM_\w+', re.sub(r'/\*.*?\*/', '', text('ELC_APP/BSW/Diag/Dcm/Dcm.c'), flags=re.S)),
          'Dcm.c references application data (belongs in DcmAppl.c)')
    check('HAL_' not in text('ELC_APP/BSW/Diag/Dcm/Dcm.c') and 'Dem_DTC_Stat[' not in text('ELC_APP/BSW/Diag/Dcm/Dcm.c'),
          'Dcm.c touches hardware or the raw DEM images')
    check('HAL_' not in text('ELC_APP/BSW/Diag/Dem/Dem.c'), 'Dem.c touches hardware')
    # raw CAN identifiers belong to the CanIf configuration only
    for rel in ['ELC_APP/BSW/Com/CanTp/CanTp.c', 'ELC_APP/BSW/Com/PduR/PduR.c', 'ELC_APP/BSW/Com/Com/Com.c',
                'ELC_APP/BSW/Diag/Dcm/Dcm.c', 'ELC_APP/BSW/Diag/Dem/Dem.c', 'ELC_APP/BSW/Com/CanNm/CanNm.c']:
        t = re.sub(r'/\*.*?\*/|//[^\n]*', '', text(rel), flags=re.S)
        check(not re.search(r'\b0x(7[0-9A-Fa-f]{2}|6[EF][0-9A-F]|3[0-9A-F]{2}|05[01])u?\b', t), f'raw CAN identifier in {rel}')
    # every BSW module with a cyclic function uses the *_MainFunction naming
    for p in app_c:
        if 'BSW' in p.parts:
            check(not re.search(r'\bvoid\s+\w+_main\s*\(', p.read_text(errors='replace')), f'non AUTOSAR main function name in {p.name}')

    # ---- boot core stays position independent and dependency free ----------------------------------------------------------
    boot = text('ELC_FBL/Core/Inc/boot_protocol.h')
    check('HAL_' not in boot and '#include <string.h>' not in boot and '#include <stdlib.h>' not in boot, 'boot core has library/HAL dependencies')
    check(not re.search(r'\(\s*\*\s*\w*\s*\)\s*\(', boot), 'boot core contains function pointers')
    check(boot.count('BOOT_CODE') >= 10, 'boot core functions lost their BOOT_CODE placement')
    app_fbl_version = re.search(r'#define\s+DCMAPPL_FBL_SW_VERSION\s+(\d+)u', text('ELC_APP/APPL/DcmAppl/DcmAppl.h'))
    boot_version = re.search(r'#define\s+BOOT_SW_VERSION\s+(\d+)u', boot)
    check(app_fbl_version is not None and boot_version is not None and
          app_fbl_version.group(1) == boot_version.group(1),
          'APP F101 FBL-version bytes drifted from the resident FBL version')

    out = ROOT / 'docs/modernization/SOURCE_INVENTORY.json'
    out.write_text(json.dumps(inventory, indent=2) + '\n')
    if failures:
        print('\n'.join('AUDIT FAIL: ' + f for f in failures))
        sys.exit(1)
    print(f'{len(inventory)} source/linker files inventoried; hardware ownership, layering, raw-identifier, '
          f'legacy-API and boot-core independence rules PASS')

if __name__ == '__main__':
    main()
