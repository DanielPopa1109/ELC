"""Native host tests: the production BSW/APPL sources are compiled unmodified with the local Zig C
compiler; only the STM32 HAL and FreeRTOS/OS data are mocked (tests/mocks and the test files).
python tools/run_host_tests.py [test names...]   (fee cantp bsw_stack boot_1 boot_2)"""
from pathlib import Path
import os, re, subprocess, sys
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/tests'; OUT.mkdir(parents=True, exist_ok=True)
ZIG = ROOT / '.tools/native/ziglang/zig.exe'
os.environ['ZIG_LOCAL_CACHE_DIR'] = str(OUT / 'zig-local')
os.environ['ZIG_GLOBAL_CACHE_DIR'] = str(OUT / 'zig-global')
BSW = 'ELC_APP/BSW'
# Same header search order as tools/build_firmware.py: mocks and the CubeMX Inc directory first (the
# CubeMX headers can.h/crc.h share their names with BSW Can.h/Crc.h on case-insensitive systems), then
# every BSW/APPL header directory.
_header_dirs = sorted({p.parent.relative_to(ROOT).as_posix() for r in ('ELC_APP/BSW', 'ELC_APP/APPL')
                       for p in (ROOT / r).rglob('*.h')})
INCLUDES = ['-Itests/mocks', '-IELC_APP/Inc', *[f'-I{d}' for d in _header_dirs]]

def run(name, sources, extra=()):
    exe = OUT / (name + '.exe')
    cmd = [str(ZIG), 'cc', '-std=c11', '-O0', '-g', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
           '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-misleading-indentation',
           '-Wno-int-to-pointer-cast', *INCLUDES, *extra, *sources, '-o', str(exe)]
    if subprocess.run(cmd, cwd=ROOT).returncode:
        sys.exit(f'{name}: COMPILE FAILED')
    if subprocess.run([str(exe)], cwd=ROOT).returncode:
        sys.exit(f'{name}: FAILED')
    print(name + ': PASS', flush=True)

def stack_sources():
    com = [str(p.relative_to(ROOT)).replace('\\', '/') for p in (ROOT / BSW / 'Com').rglob('*.c')]
    return com + [f'{BSW}/Diag/Dcm/Dcm.c', f'{BSW}/Diag/Dcm/Dcm_Cfg.c', f'{BSW}/Diag/Dem/Dem.c', f'{BSW}/Diag/Dem/Dem_Cfg.c',
                  f'{BSW}/Mem/Nvm/NvM.c', f'{BSW}/Mem/Nvm/NvM_Cfg.c', f'{BSW}/Mem/Fee/Fee.c', f'{BSW}/Mem/Fls/Fls.c',
                  f'{BSW}/Sys/Crc/Crc.c', 'ELC_APP/APPL/DcmAppl/DcmAppl.c', 'ELC_APP/APPL/SMon/SMon_Calib.c',
                  'build/tests/smon_globals.c', 'tests/bsw_stack_test.c']

TESTS = {
    'fee': lambda: run('fee', ['tests/fee_test.c', f'{BSW}/Mem/Fee/Fee.c', f'{BSW}/Sys/Crc/Crc.c']),
    'cantp': lambda: run('cantp', ['tests/cantp_test.c', f'{BSW}/Com/CanTp/CanTp.c', f'{BSW}/Com/CanTp/CanTp_Cfg.c']),
    'bsw_stack': lambda: run('bsw_stack', stack_sources()),
    'boot_1': lambda: run('boot_1', ['tests/boot_protocol_test.c'], ['-DBOOT_TEST_BLOCK=1']),
    'boot_2': lambda: run('boot_2', ['tests/boot_protocol_test.c'], ['-DBOOT_TEST_BLOCK=2']),
}

if __name__ == '__main__':
    # The hardware mock defines the application RAM symbols declared by SMon.h (excluding SMon_Calib's own).
    smon = (ROOT / 'ELC_APP/APPL/SMon/SMon.h').read_text()
    declarations = re.findall(r'^extern\s+([^;\n]+);', smon, re.M)
    globals_ = '\n'.join(d + ';' for d in declarations if '(' not in d and 'SMon_Calib' not in d)
    (OUT / 'smon_globals.c').write_text('#include "SMon.h"\n' + globals_ + '\n')
    for name in sys.argv[1:] or TESTS:
        TESTS[name]()
