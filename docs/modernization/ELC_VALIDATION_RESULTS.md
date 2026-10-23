# ELC validation results - 2026-10-19

**Result: software implemented, both Cortex-M3 images built/linked, four assertion-enabled native binaries passed, SRAM dependency and source ownership checks passed. Hardware-verified: NO. Deployment/programming readiness remains conditional on the dependencies below.**

## Executed commands and artifacts

```
python tools/build_firmware.py
python tools/verify_firmware.py
python tools/run_host_tests.py
python tools/audit_sources.py
git diff --check
```

Cortex-M3/Thumb/soft-float, local xPack GNU Arm Embedded GCC15.2.1, -Os, function/data section garbage collection, local ELC HAL/CMSIS/FreeRTOS. Installed STM32CubeIDE/bundled Cortex-M compilers were searched but not found. A local compiler archive and Zig0.16.0 native package were placed under ignored .tools; no system toolchain installation occurred. Windows native tests use -O0 -g -UNDEBUG, -Wall/-Wextra/-Werror plus bounds instrumentation. Zig required execution outside the desktop process restriction because it could not resolve its own executable path inside that restriction. No approval action was rejected.

Outputs: build/ELC_APP and build/ELC_FBL contain ELF/MAP/HEX/BIN/disassembly/stack-usage data and manifest.json. Logs are build/firmware_build.log and build/host_tests.log. A complete diagnostic and recovery packaging distinction is documented in tools/README.md. Generated Debug makefiles with obsolete absolute paths were not used as build evidence.

| Linked image | Flash used / allocation | Runtime SRAM used / allocation | Retained SRAM |
| --- | --- | --- | --- |
| APP | 47,636 /48,124 bytes (98.99%) | 19,984 /20,352 bytes (98.19%) | 22 /64 bytes; session at20004FC0 |
| FBL incl. SRAM blob load image | 13,644 /14,336 bytes (95.17%) | 5,392 /20,416 bytes (26.41%) | None allocated by resident image |

APP has488 bytes Flash and368 bytes normal SRAM allocation margin, in addition to its configured heap/stack reservations. This is a link result, not a task-stack high-water measurement. No partition was enlarged. A single FBL ELF read/write/execute load-segment warning is expected for the intentionally copied SRAM code/data blob; no C compiler warnings remain in the final firmware build.

Actual NvM payload: block1=72 bytes, block2=432 bytes, block3=140 bytes; total644 bytes. Each committed bank fits one1 KiB page including its header/commit. APP CRC footer and calibration/NvM pages are excluded from executable Flash. Retained data is asserted below20004FE0 so it cannot overlap the updater fault record.

APP.program.bin is padded to48,124 bytes, excluding the CRC word. APP.bootable.bin appends the full-span CRC in little-endian form for an independently loaded boot image. RequestDownload must exclude that footer; the resident writes it after verification. No artifact was flashed.

## Native execution evidence

| Binary / actual sources | Passed assertions and fault cases |
| --- | --- |
| nvm_store.exe - production NvmStore.c,644-byte payload | Missing bank/defaults, known CRC vector,90 restart-cut positions, old/new snapshot recovery, alternating compaction generations, erase failure, program failure, corrupt readback, invalid CRC, torn commit and unchanged-write suppression. |
| bsw_stack.exe - production Can/CanIf/CanTp/PduR/Com/CanNm/CanSM/ComM/DCM/DEM/NvM adapter/store | LoadRequest valid/corrupt CRC and legal repeated alive; expiry; NM invalid DLC/request/release/timeout/restoration; bus-off; mailbox success/error/100 ms timeout/duplicate ownership; simultaneous app and diagnostic slots; LoadStatus retry; CommunicationControl and XCP retry; SF/FF/CF/FC, invalid SN, overflow, BS1/STmin10/WAIT/timeouts; multi-DID requests/responses; session/NRC/S3; DTC status/snapshot/occurrence/cycle; unchanged-event endurance; persistence across simulated reset; P2*78 renewal during paused NvM; clear durable success and Flash-program failure; idle-only write conditions; legacy and CAL2 parameter checks; safe and interrupted legacy migration; occupied-two-page legacy migration refusal with byte-for-byte preservation. |
| boot_1.exe - resident instance of production boot_protocol.h | Download range/alignment/length checks; BSC progression, identical replay, changed replay, wrap through00; incomplete exit; CRC mismatch/success; erase/program/readback/finalize failure; programming data persistence in simulated Flash; extended-address filtering, FF/CF and sequence/timeout behavior, seven-byte response FC/CF, block selection and updater-entry callback. |
| boot_2.exe - SRAM instance of production boot_protocol.h | Same state-machine/error/transport assertions with FBL range and updater block2 rules; verification-before-reset path. |

The stack tests compile the real modules and real SMon type declarations; only peripheral/Flash/OS/application input ports are mocked. The programming tests instantiate the production header used in both firmware images. They do not emulate Cortex-M instructions, Flash voltage/timing, MSP/VTOR or a physical reset. Physical monitor thresholds/polarities and SMon algorithm decisions were preserved; SMon hardware/protection behavior was not validated by these mocks.

## ELF, source and regression evidence

- Both ELF files are ARM ELF32. APP data load end, CRC footer, normal/retained SRAM and session word placement are checked.
- FBL updater VMA starts20000000, vector MSP20004F80, Thumb reset points inside the blob, LMA remains below08003800 and blob end remains below20004B80. At least1 KiB updater stack margin is enforced. All44 emitted relocations originating in the SRAM blob target that blob or legitimate absolute hardware symbols; Flash call/data dependencies fail verification. Objdump disassembly is saved alongside the ELF.
- All74 maintained APP/FBL source/header/startup/linker files are inventoried with SHA-256. APP HAL CAN submissions occur only in Can.c. DCM has no raw CAN/PCI or direct DEM array access. DEM has no HAL CAN/Flash calls. No RTE/DET/MemMap was introduced.
- CubeIDE FBL linked resources/includes now resolve within ELC. Its SRAM file flags prevent builtin helpers and switch tables. Standalone source enumeration includes every new module. CubeIDE GUI builds were not executed.
- git diff --check passed. Pre-existing worktree edits and ELC.zip were preserved. ZGW/F93/G26/FCD were read-only references.

## Remaining dependencies and residual limitations

1. Hardware: confirm actual MCU Flash density, bxCAN arbitration/errors/ABOM, gateway FC and response correlation, absolute message deadlines, task/ISR/ADC/protection timing, stack/heap high-water, idle-window Flash stalls, physical CRC boot and reset/MSP/VTOR handover.
2. Persistent storage: real power-cut/endurance/failure tests are still needed. A legacy journal whose recoverable records span both pages cannot be converted transactionally without independent backup/storage. It returns MIGRATION_REQUIRED rather than destroying the last records. Legacy calibration has no original CRC and remains explicitly marked unverified unless a separately verified CAL2 image is supplied.
3. Resident FBL: replacement is single-copy; power loss needs independent SWD/ROM recovery. A contiguous image per erase is required to avoid repeated programming of the APP boot CRC word. No autonomous power-loss recovery is claimed.
4. External FCD profile: currently Simulated ELC, incorrect APP base08000000, and generic version DIDF101 conflicting with ELC APP legacy reset counter. Use a real profile,08003C00 APP base/current artifacts, F180 version query, and correct payload extent. ZGW routes and the upstream711 setting still require a real end-to-end trace; synthetic acknowledgements are insufficient.

There is no software deployment, CM5 access/modification/reboot, physical ECU erase/program/reset, option-byte change, commit or push in this task. The detailed [module coverage](../autosar/BSW_FUNCTIONAL_COVERAGE.md), [closure record](ELC_FUNCTIONAL_GAP_CLOSURE.md) and [external dependencies](../compatibility/ZGW_FCD_GAP_ANALYSIS.md) accompany this evidence.

## Final artifact SHA-256

### ELC_APP

- ELC_APP.bin: `41d1d539a6751f70ea1d31438ed925ef7d508f75193a873bfbc8493084759a16`
- ELC_APP.bootable.bin: `341127320e9cbfaf704a4d1e0981bf97aa83f34ad2aeb2568ca5c2ac5070bff8`
- ELC_APP.elf: `cf486b462bd6e6752d254e5ff39ad704101c4115ddcb25ceff1345fe7ecdef60`
- ELC_APP.hex: `42d817a32a74012023d270035e1f8025dabd71ceaee2c60637f9aa3a622106a7`
- ELC_APP.program.bin: `270add3016bdbc97a278647ba2bf8a07dcc9be0c5e492a9fa704803ce0a65e5a`

### ELC_FBL

- ELC_FBL.bin: `60fdeb5977e0079040b1458dab4cfb1716eb6c554848975cb0544c34213d0462`
- ELC_FBL.elf: `a61cc017e85e773bb0ffab86754660e83f6bd9a8492e84ccc11c128163c0aeac`
- ELC_FBL.hex: `7e568c56d2c0f9f6dfee93cf3f24c853e4f78d5aa401a4426b2af98b7d0f5f57`

