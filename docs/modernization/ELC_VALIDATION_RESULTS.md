# ELC validation results - 2026-10-24

**Result: the configured ELC subset builds as two Cortex-M3 images; five assertion-enabled native binaries, SRAM dependency checks, and source layering/ownership checks pass. This is not complete AUTOSAR/UDS coverage. Hardware-verified: NO. FCD programming readiness remains conditional on the dependencies below.**

This record replaces the 2026-10-19 numbers after the [AUTOSAR-lab, UDS and ISO-TP alignment](../autosar/AUTOSAR_LAB_ALIGNMENT.md). The memory map, journal format, calibration layout and programming contract are unchanged from 2026-10-19 (see the [closure record](ELC_FUNCTIONAL_GAP_CLOSURE.md)).

## Executed commands and artifacts

```
python tools/build_firmware.py
python tools/verify_firmware.py
python tools/run_host_tests.py
python tools/audit_sources.py
git diff --check
```

Cortex-M3/Thumb/soft-float, local xPack GNU Arm Embedded GCC 15.2.1 (20251203), -Os, function/data section garbage collection, local ELC HAL/CMSIS/FreeRTOS. A local compiler archive and Zig 0.16.0 native package live under the ignored `.tools`; no system toolchain installation occurred. Windows native tests use -O0 -g -UNDEBUG, -Wall/-Wextra/-Werror plus bounds instrumentation. Generated Debug makefiles with obsolete absolute paths were not used as build evidence. Each command above ran to completion in this checkout after the last source and line-ending change; the audit was run last so the source inventory matches the sources.

`git diff --check` passed (line-ending conversion warnings only). Nothing was committed or pushed. Line endings follow each file's previous style (CRLF for the existing APP/FBL/docs/tools files, LF only for the `tests/mocks` headers), so a review diff shows only the real changes for edited files and whole-file diffs for rewritten ones.

Outputs: build/ELC_APP and build/ELC_FBL contain ELF/MAP/HEX/BIN/disassembly/stack-usage data and manifest.json. Logs are build/firmware_build.log and build/host_tests.log. See tools/README.md for the diagnostic and recovery packaging distinction.

| Linked image | Flash used / allocation | Runtime SRAM used / allocation | Retained SRAM |
| --- | --- | --- | --- |
| APP | 48,104 / 48,124 bytes (99.96%) | 19,760 / 20,352 bytes (97.09%) | 22 / 64 bytes; session at 20004FC0 |
| FBL incl. SRAM blob load image | 14,288 / 14,336 bytes (99.67%) | 5,712 / 20,416 bytes (27.98%) | None allocated by resident image |

Compared with 2026-10-19 the APP uses 468 more bytes of Flash (20 left) and 224 fewer bytes of normal SRAM (592 left); the FBL uses 644 more bytes of Flash (48 left). This is a link result, not a task-stack high-water measurement; the stack and heap reservations are unchanged. No partition was enlarged. A single FBL ELF read/write/execute load-segment warning is expected for the intentionally copied SRAM code/data blob; no C compiler warnings remain in the final firmware build.

Actual NvM payload is unchanged: block1 = 72 bytes (`Dem_DTC_Stat`), block2 = 432 bytes (`Dem_FF`), block3 = 140 bytes (`SMon_Battery`); total 644 bytes. Each committed bank fits one 1 KiB page including its header/commit. APP CRC footer and calibration/NvM pages are excluded from executable Flash. Retained data is asserted below 20004FE0 so it cannot overlap the updater fault record.

APP.program.bin is padded to 48,124 bytes, excluding the CRC word. APP.bootable.bin appends the full-span CRC in little-endian form for an independently loaded boot image. RequestDownload must exclude that footer; the resident writes it after verification. No artifact was flashed.

## Native execution evidence

| Binary / actual sources | Passed assertions and fault cases |
| --- | --- |
| fee.exe - production Fee.c and Crc.c over a mocked Fls | Missing bank/defaults, known CRC-32 vector, 90 restart-cut positions, old/new snapshot recovery, alternating compaction generations, erase failure, program failure, corrupt readback, invalid CRC, torn commit, unchanged-write suppression and the `MEMIF_MIGRATION_REQUIRED` refusal when no page may be erased. |
| cantp.exe - production CanTp.c/CanTp_Cfg.c with mocked PduR and CanIf | SF/FF/CF/FC in both directions; invalid SF_DL (0 and 7), FF_DL <= 6 and short frames ignored; 256-byte reception with BS 8 and SN wrap; STmin values including reserved codes (127 ms); FC.WAIT with the N_WFTmax counter reset by CTS; FC overtaking our own TX confirmation, FC from N_TA 0x41, FC without a transfer or with too few bytes ignored; CF while the own FC confirmation is pending; N_As, N_Bs, N_Cr timeouts and an FC that CanIf cannot send; CanIf busy retry of SF/FF/CF. |
| bsw_stack.exe - production Can/CanIf/CanTp/PduR/Com/CanNm/CanSM/ComM/CanH/Dcm/DcmAppl/Dem/NvM/Fee/Fls/Crc | LoadRequest valid/corrupt CRC and legal repeated alive; expiry; NM invalid DLC/request/release/timeout/restoration; bus-off; mailbox success/error/100 ms abort/duplicate ownership; LoadStatus retry; I-PDU group gating through `28`; XCP retry and flush; multi-frame requests and responses over the real CanTp; sessions, suppressPosRspMsgIndicationBit on every sub-function service, NRC order, P2/P2* values, NRC 0x78 timing (first at 40 ms, then 4.5 s) and S3; `22` multi-DID and overflow; `19` 01/02/04/06 with filters, snapshot and occurrence extended data; F101 six-byte APP/FBL version with F001 reset counter; `31` 0040/0042 (seconds unit); `85`; DTC status/occurrence/cycle; unchanged-event endurance; persistence across a simulated reset; legacy-journal import, safe migration and the occupied-two-page refusal with byte-for-byte preservation (`NvM_WriteAll` returns `NVM_REQ_MIGRATION_REQUIRED`); clear durable success and Flash-program failure; clear/reset/programming-session requests during a running Dem write; idle-only write conditions; calibration slot table (72 slots, defaults, rejection); reset only after the final response confirmation. |
| boot_1.exe - resident instance of production boot_protocol.h | Download range/alignment/length checks; BSC progression, identical replay, changed replay, wrap through 00; incomplete exit; CRC mismatch/success; erase/program/readback/finalize failure; programming data persistence in simulated Flash; extended-address filtering, FF/CF/FC, FF_DL <= 6 ignored, flow control from N_TA 0x58 and 0x41, sequence and N_Cr/N_Bs timeouts; NRC 0x11 for unknown services in every session; suppress bit on `10`, `11`, `31`; S3Server expiry and TesterPresent reload; FCD 28/85 preamble, six-byte F101 version, block selection and updater-entry callback. |
| boot_2.exe - SRAM instance of production boot_protocol.h | Same state-machine/error/transport/diagnostic assertions with FBL range and updater block 2 rules; verification-before-reset path. |

The stack tests compile the real modules and real SMon type declarations; only peripheral/Flash/OS/application input ports are mocked. The programming tests instantiate the production header used in both firmware images. The opt-in `ECUM_STANDBY_ENABLED=1` branch of `EcuM.c` also compiled separately for Cortex-M3; that is syntax/code-generation evidence, not a sleep/wake execution test. The tests do not emulate Cortex-M instructions, Flash voltage/timing, MSP/VTOR, interrupts or a physical reset. Physical monitor thresholds/polarities and SMon algorithm decisions were preserved; SMon hardware/protection behavior was not validated by these mocks. The expected values in the transport and diagnostic assertions come from ISO 15765-2, ISO 14229-1 and the ZGW sources, not from a recorded trace.

## ELF, source and regression evidence

- Both ELF files are ARM ELF32. APP data load end, CRC footer, normal/retained SRAM and session word placement are checked (`verify_firmware.py`).
- FBL updater VMA starts 20000000, vector MSP 20004F80, Thumb reset points inside the blob, LMA remains below 08003800 and blob end remains below 20004B80. At least 1 KiB updater stack margin is enforced. All 44 emitted relocations originating in the SRAM blob target that blob or legitimate absolute hardware symbols; Flash call/data dependencies fail verification. Objdump disassembly is saved alongside the ELF.
- 97 maintained APP/FBL source/header/startup/linker files are inventoried with SHA-256 in `SOURCE_INVENTORY.json`. The audit also fails on: RTE/DET/MemMap, legacy API names, a CAN submit/RX read outside `Can.c`, a Flash erase/program outside `Fls.c`, forbidden upward includes (Dcm, Dem, CanTp, Com, PduR, NvM, Fee, CanIf, Can), SMon/EcuM references in `Dcm.c`, raw CAN identifiers outside `CanIf_Cfg.c`, `*_main` function names, library/HAL/function-pointer use in the boot core, and APP F101's FBL version drifting from the FBL's own version constant.
- CubeIDE (`ELC_APP/.cproject`) lists the new `Common`, `Mem/*`, `Sys/Crc`, `Com/Com`, `Com/ComM` and `APPL/DcmAppl` folders; CubeIDE GUI builds were not executed.
- ZGW/F93/G26/FCD were read-only references.

## Remaining dependencies and residual limitations

1. Hardware: confirm actual MCU Flash density, bxCAN arbitration/errors/ABOM, gateway FC (including N_TA 0x41), response correlation, P2/P2* and ISO-TP timing, absolute message deadlines, task/ISR/ADC/protection timing, stack/heap high-water, idle-window Flash stalls, physical CRC boot and reset/MSP/VTOR handover. STM32 Standby remains disabled by default; PA0 wake, output state, persistence and restart must be checked on the board before enabling it. Failed NvM persistence now prevents optional Standby.
2. FBL S3Server (`BOOT_S3_MS`, 5000 ms after the tester's first request) has not been run against FCD. If FCD pauses longer than 5 s between requests without TesterPresent, set it to 0.
3. Persistent storage: real power-cut/endurance/failure tests are still needed. A legacy journal whose recoverable records span both pages cannot be converted transactionally without independent backup/storage. It returns MIGRATION_REQUIRED rather than destroying the last records. Legacy calibration has no original CRC and remains explicitly marked unverified unless a separately verified CAL2 image is supplied.
4. Resident FBL: replacement is single-copy; power loss needs independent SWD/ROM recovery. A contiguous image per erase is required to avoid repeated programming of the APP boot CRC word. No autonomous power-loss recovery is claimed.
5. External FCD profile: currently Simulated ELC, incorrect APP base 08000000 and a stale HEX path under the missing VSC directory. F101 is now the six-byte APP/FBL version in APP and FBL; F001 retains the reset counter. Use a real profile, 08003C00 APP base/current artifacts, and correct payload extent. ZGW routes and the upstream 711 setting still require a real end-to-end trace; synthetic acknowledgements are insufficient.
6. Scope of the standards claims: no SecurityAccess, WriteDataByIdentifier, functional addressing, NRC 0x21 or FC.WAIT; Dem has no debounce counters, aging or additional extended-data records beyond occurrence record 0x01; see the alignment record for the full list.

There is no software deployment, CM5 access/modification/reboot, physical ECU erase/program/reset, option-byte change, commit or push in this task. The detailed [module coverage](../autosar/BSW_FUNCTIONAL_COVERAGE.md), [alignment review](../autosar/AUTOSAR_LAB_ALIGNMENT.md), [closure record](ELC_FUNCTIONAL_GAP_CLOSURE.md) and [external dependencies](../compatibility/ZGW_FCD_GAP_ANALYSIS.md) accompany this evidence.

## Final artifact SHA-256

### ELC_APP

- ELC_APP.bin: `a079047a9697d6ff85475588588f8dee3c8a15cb8efd3f45cb0e3de2400c310d`
- ELC_APP.bootable.bin: `355fbe2f0bac45dd5934199a976c4bed2859b7d52f9c6454a83f5d49b88403da`
- ELC_APP.elf: `ba87a253b90b548892a3cf89e2ad59633f21e53f769fa229d7aef4476c5ace5e`
- ELC_APP.hex: `9ffa1e55af223d052014184b4ddf8c407e4a57142c2c29d6e8cb484a4e31c6ce`
- ELC_APP.program.bin: `2e87ea95d4361c0731bc7872ce44a265c45db3f6ec850c578bc9f64becc7b013`

### ELC_FBL

- ELC_FBL.bin: `e5136713f141c5c57cdaccc5f421460079aa989fbbd54b10408391528aaef2e0`
- ELC_FBL.elf: `5e5cdca04197a7d3e259c3527f925c206b32cbc26c3e2d139f407c67145e940b`
- ELC_FBL.hex: `b9dfa821c4d39f0763502570d242ca0d51eed83c4eca2fadf2ad23698d191b2c`
