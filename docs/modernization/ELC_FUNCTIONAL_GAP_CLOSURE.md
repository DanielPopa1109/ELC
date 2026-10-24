# ELC functional gap closure - 2026-10-19

**Revision 2026-10-24:** M9 below records the AUTOSAR-lab / UDS / ISO-TP alignment pass ([review and behavior changes](../autosar/AUTOSAR_LAB_ALIGNMENT.md)). It renamed and re-layered the APP modules; M1-M8 keep the 2026-10-19 contracts (memory map, journal format, programming protocol) and are annotated where a statement was superseded. Module names in M2-M6 that read `Nvm`, `NvmStore`, `MemIf`, `CanIf_Transmit*` or `Dem_SetDtc` are now `NvM`/`Fee`/`Fls`, `CanIf_Transmit(PduIdType, ...)` and `Dem_SetEventStatus`.

The M10 continuation below supersedes M9's historical DID, extended-data and size statements where they differ.

Software is connected, Cortex-M3 compiled and assertion-host-tested. Hardware integration is unverified. The pre-existing dirty working tree was extended in place. No reference repository, CM5, ECU Flash, reset state, option byte, commit or push was changed.

## M1 - Starting point and memory map

Initial work already supplied communication modules and some APP/FBL hardening, but still contained the destructive synchronous NvM journal, live-RAM defaults, direct DEM writes/broadcasts, raw-PCI DCM and normal-address diagnostics. The inventory in `SOURCE_INVENTORY.json` hashed 74 maintained APP/FBL source/header/startup/linker files at that time (96 after M9). HAL/CMSIS and FreeRTOS were compiled from ELC. No F4 Flash/startup implementation was copied.

| Region | Address and size | Evidence |
| --- | --- | --- |
| FBL plus updater load image | 08000000..080037FF / 14 KiB | FBL linker and ELF load addresses |
| Calibration | 08003800..08003BFF / 1 KiB | NvM constants, Flash ports |
| APP executable/data load | 08003C00..0800F7FB / 48,124 bytes | APP linker and ELF |
| APP CRC word | 0800F7FC..0800F7FF / 4 bytes | Resident finalize and boot check |
| Diagnostic/battery NvM | 0800F800..0800FFFF / 2 KiB | NvM MemIf range checks |
| APP normal SRAM | 20000000..20004F7F / 20,352 bytes | APP linker |
| FBL normal SRAM | 20000000..20004FBF / 20,416 bytes | FBL linker |
| Retained SRAM | 20004FC0..20004FFF / 64 bytes | Both linkers; APP session fixed at20004FC0 |
| RAM updater MSP / fault record | 20004F80 / 20004FE0..20004FF3 | RAM vectors and fault handler |

Nominal target is STM32F103C8Tx, 64 KiB Flash, 20 KiB SRAM, 1 KiB Flash pages, Cortex-M3. No physical density read was performed. APP's existing 64-byte gap below retention was preserved. GPIO, PWM polarity/period, ADC channels, protection thresholds and SMon decisions were preserved. Peak-current magnitude now uses fabsf instead of truncating a float through integer abs.

## M2 - NvM, memory port and CRC

Layering since M9: `Fls` (only Flash HAL user) -> `Fee` (journal below) -> `NvM` (blocks, static `NvM_Cfg.c`), with `Crc_CalculateCRC32` in `Sys/Crc`; the on-Flash format is unchanged. Block IDs1/2/3 preserve the RAM layouts for 18 32-bit statuses, 18 packed freeze frames and SMon_BatteryData_t. They form one fixed-size snapshot, statically asserted to fit a page. ReadBlock/ReadAll and WriteBlock/WriteAll are connected. Each logical write commits all three blocks; the job result is shared, intentionally making status/snapshot clear atomic within 2 KiB.

New bank: ELC2 magic at+0, generation+4, payload size+8, payload CRC32+12, header CRC32+16, payload+32, commit A55A69C3 at+1020. Erase the inactive page, program four words per worker call, verify in bounded batches, then program/read back the commit word. Keep the old committed page. Boot selects the newest valid generation. Immutable defaults remain RAM-only. Identical snapshots are skipped; failed jobs stop instead of retrying forever. The actual F103 memory port validates addresses, checks HAL failures/readback, locks Flash and restores the caller's PRIMASK.

Legacy journal import verifies STM32 word-oriented CRC and retains the latest valid records. The legacy page-zero marker must survive migration. Page1 is used only if the marker and every latest record stay recoverable in page0. Latest records touching both pages produce NVM_MIGRATION_REQUIRED without erasing data. Independent backup/export and deliberate offline conversion are required for that geometry. New-format writes are not backward-readable by old firmware.

Calibration is never erased/rewritten on startup. Existing 292-byte encoding is preserved; used floats must be finite and required integer divisors nonzero. Nvm_ParameterIntegrity=1 explicitly identifies accepted legacy data without a checksum. A read-only CAL2 envelope uses the two unused words: payload CRC at0, magic324C4143 at4, CRC over bytes8..291. Valid CAL2 reports2; invalid/defaults reports0. No automatic conversion is attempted. Software cannot infer the missing original checksum of legacy calibration.

Single-bank F103 erase stalls the CPU. Background Flash work therefore requires L1 status/command off and no active CLS pulse. ClearDTC/reset/programming requests fail with conditionsNotCorrect while this condition is false. The diagnostic task advances NvM only while CanTp is idle, after transmitting pending responses. Active faults remain in RAM until an idle persistence window. This is not a guarantee of uninterrupted 5 ms execution during erase: real erase latency, interrupt/tick behavior, watchdog implications and loaded-bus deadlines still need bench measurements.

## M3 - DEM

SMon/CanH retain physical diagnosis and debounce. DEM handles qualified FAILED/PASSED reports from the existing call sites, now `Dem_SetEventStatus(eventId, DEM_EVENT_STATUS_FAILED/PASSED)` (event 0..17 = DTC 0x50..0x61; PREPASSED/PREFAILED are rejected because the monitors debounce). Repeated failures neither increment occurrence nor rewrite Flash. UDS availability is7F; confirmation follows one qualified failure, preserving immediate-confirm behavior. No calibrated aging threshold exists, so confirmed history remains until clear. EcuM starts the power-on cycle and ends it before post-run persistence; the public cycle API supports restart.

Clear accepts an exact configured DTC or FFFFFF. DEM clears RAM, freezes a complete NvM snapshot, then waits for actual commit/failure. Reports during clear are deferred and re-applied afterward. DCM returns54 only on durable success, otherwise7F1472. On failure RAM may be cleared while the previous durable image remains valid; the negative response and old image recovered after reset are intentional. Broadcast7FE goes through COM/PduR/CanIf. DCM uses DEM public accessors.

## M4-M6 - Communication, diagnostics and system integration

APP path: hardware ISR -> Can RX queue -> CanIf -> CanTp -> PduR -> Dcm, with reverse responses and physical TX confirmations. COM uses parallel interface routes. Only Can owns HAL_CAN_AddTxMessage. XCP runs in task context through its CanIf response/retry adapter.

LoadRequest050/DLC2 validates data-ID7 and low CRC32 byte over [alive, request, dataId]. Bad traffic does not refresh validity and forces command invalidity; expiry remains200 x5 ms with safe command0. Alive is CRC-protected but not required to advance by one: GatewaySwc increments during generation while COM schedules/repeats independently. Identical valid frames are legal. LoadStatus051 retains one byte/bit0, change-triggered plus500 ms transmission and physical-failure retry. Telemetry IDs/packing and existing5 ms fast /200 ms battery production remain. SMon publishes coherent measurements; COM versions avoid clearing newer samples on old confirmations. DBC period discrepancies are listed in the current CAN contract.

Receive-only NM3 is3FF/DLC1. Existing ELC bit6 permits normal communication; release or1 s loss restricts it. Bus-off also restricts normal TX. CAN/diagnostics remain online and missing NM does not put the MCU in standby. No invented subordinate NM transmit handshake was added.

DCM accepts payloads only. Services10,11,14,19(01/02/04/06),22,28(0..3,type1/3),31(0040 heal/0042 timed load),3E and85 have length/subfunction/session checks and NRCs. Sessions use5 s S3 and advertise P2=50 ms/P2*=5 s (`10 02` answers `50 02 00 32 01 F4`). Since M9 the first NRC 0x78 for asynchronous work is sent after 40 ms (it was immediate) and renewed every 4.5 s; every sub-function service honors suppressPosRspMsgIndicationBit. Reset/APP-to-FBL waits for persistence plus physical final-response confirmation. Programming requires extended session and valid vehicle state6. No security service was fabricated.

Legacy DID meanings are retained under F0xx/F1xx for low bytes03 active time,04 time without reset,05 reset data,06 CPU load,1D load status,80 software version,7E session and7F active block. F100/F186 alias active block/session. APP F101 now returns the six-byte APP/FBL version required by the inspected FCD/ZGW sources; F001 retains the reset counter and F180 retains the four-byte ELC version. Snapshot DIDs F300..F30B retain big-endian integer values and raw little-endian floats.

BSW/APPL tasks now use delay-until5 ms, CAN polling1 ms, TP/DCM/DEM/NvM5 ms. The suspend/resume lost-wake window is removed from these tasks. Post-run tracks asynchronous persistence completion/failure and stays awake on NM loss. Physical RTOS/ADC/protection/stack timing remains unverified.

## M7 - Resident FBL and independent SRAM updater

Resident startup, the two Flash writers, copied vector image, memory geometry, SRAM execution and reset/handover architecture remain. Duplicate legacy parsers were replaced by two compiled instances of Core/Inc/boot_protocol.h to align contracts and test real programming behavior without Flash dependencies in the SRAM instance. SRAM functions use section attributes and no builtin helper/switch-table generation. All live updater state is copied with the blob. ELF verification checks vectors, VMA/LMA, stack separation, and all44 SRAM-origin relocations; disassembly is emitted.

All three target images use standard CAN710 in both directions, RX address58 and TX addressF0 (the gateway's configured receive address). Since M9 the APP CanTp and the FBL boot core also accept flow control addressed with N_TA 0x41, the tester address ZGW uses outside a forwarded-request transmission (ZGW `CanTp.c` `CanTp_SetTxAddress`); this is source-derived and has not been seen on a bus. FCD's711 setting is not the target-side response wire ID. Legacy702/703 is not a normal-operation fallback.

Canonical RIDs:0200 block select (APP writer1/FBL writer2),0155 updater entry,0001 erase with BE address/size,0002 CRC with BE address/size/CRC. Existing0004 APP erase,0005 parameter erase,0007 updater entry,0006 SRAM FBL erase and0000 legacy LE-CRC remain explicit aliases. RequestDownload is34 00 44 plus BE address/size. Request maximum240 includes SID/BSC. Exact BSC replay is acknowledged without rewriting, changed replay/future BSC returns73, and sequence wraps through00. Flash errors abort the active download. TransferExit flushes a padded last word and rejects incomplete data. 37 plus BE CRC validates reflected CRC32; plain37 still requires subsequent0002 verification.

After verified programming, the resident writes the CRC of the entire APP span toF7FC. Startup checks that CRC and valid vectors. The SRAM updater requires a contiguous image starting08000000 and validates vectors against the received extent before verified reset. Single complete APP download per erase is supported. Package disjoint segments into one contiguous image rather than repeatedly attempting to replace the already-programmed CRC footer.

FBL remains single-copy: interruption during replacement requires independent SWD/ROM recovery. No physical programming or automatic power-loss recovery is claimed.

## M8 - Evidence and remaining dependencies

See [validation](ELC_VALIDATION_RESULTS.md) and [per-module coverage](../autosar/BSW_FUNCTIONAL_COVERAGE.md). Both Cortex-M3 images fit unchanged partitions. Native tests run actual modules and both boot-core instances with hardware mocks. CubeIDE FBL references now point to ELC's HAL/CMSIS; the standalone build avoids stale generated absolute-path makefiles.

Remaining acceptance dependencies:

- Real bus traces for gateway routing, FC/response correlation, telemetry/LoadStatus deadlines, NM and bus-off restoration.
- Bench RTOS/ADC/protection, stack/heap measurements, Flash latency and physical reset/handover.
- Real power-cut/endurance tests, independent FBL recovery, and backup before offline migration of a fully occupied legacy journal.
- External FCD profile currently marks ELC Simulated and APP base08000000 rather than08003C00. Its version queryF101 now matches ELC APP/FBL, but programming payloads must exclude the APP CRC footer. Those external files were not changed.

No CM5 access/deployment/reboot, ECU flashing/erase/reset, commits or pushes occurred.

## M9 - AUTOSAR-lab, UDS and ISO-TP alignment (2026-10-24)

Goal: behave like the standards say, without an RTE, in the shape of the ZGW lab stack. Full findings, rule set, per-module changes and open decisions are in [AUTOSAR_LAB_ALIGNMENT.md](../autosar/AUTOSAR_LAB_ALIGNMENT.md); the per-module table is [BSW_FUNCTIONAL_COVERAGE.md](../autosar/BSW_FUNCTIONAL_COVERAGE.md).

- **Structure:** `Common/` types and PDU ids; `Can_Write`/`CanIf_Transmit`/`CanTp_Transmit` with `PduInfoType`; static CanIf, PduR and CanTp tables; CanTp uses the PduR TP buffer API (the 256-byte CanTp message buffer is gone); Com signals plus I-PDU group gating; CanNm -> ComM -> CanSM -> CanIf events; table-driven Dcm with the application knowledge in `APPL/DcmAppl`; Dem with event ids, UDS status byte, filter, snapshot and clear API; Fls/Fee/NvM/Crc layering; calibration as a 72-slot table; `*_MainFunction` naming.
- **ISO 15765-2 fixes:** FF_DL <= 6 ignored; reserved STmin = 127 ms; FC.WAIT counter reset by CTS; CF accepted while own FC confirmation is pending; N_Ar/N_As supervision, failed transmissions end by timeout; FC accepted from N_TA 0x58 or 0x41.
- **ISO 14229-1 fixes (APP):** `10 02` positive response length and the 0x7E vs 0x7F choice; generic suppressPosRspMsgIndicationBit; NRC 0x78 timing; `22` overflow = 0x14; `28` communicationType 2/3; clear/reset/programming no longer fail with 0x22 while the Dem write is in flight; routine 0x0042 time unit corrected (was 5x).
- **FBL:** `BootTp_*`/`BootDcm_*` layers in the same dependency-free header; NRC 0x11 for unknown services in every session; sub-function suppress bit; S3Server 5 s after the tester's first request (`BOOT_S3_MS`, 0 disables).
- **M9 checkpoint budget (superseded by M10):** APP 47,808 / 48,124 bytes Flash (316 spare) and 19,752 / 20,352 bytes RAM (600 spare) against 47,636 / 19,984 before; FBL 14,028 / 14,336 bytes (308 spare) against 13,644. Partitions, NvM geometry, journal format and calibration layout are unchanged.
- **Not changed:** wire identifiers, signal packing, DID values, RIDs, programming flow, the 64 KiB geometry and the rule that a fully occupied legacy journal is never erased.
- **Open (see alignment record):** FBL S3 vs FCD, FC N_TA 0x41 on a real bus, `19 04 FFFFFF`, no SecurityAccess / functional addressing / NRC 0x21, all hardware validation.

## M10 - FCD/DTC regression and persistence-safe Standby (2026-10-24)

The current APP `Dcm_Cfg.c` and `DcmAppl.c` answer FCD's F101 software-version query with six APP/FBL version bytes. F001 retains the old little-endian reset counter; F180 retains the ELC version. `Dcm.c`/`Dem.c` also implement UDS `19 06` for record 0x01, a two-byte occurrence counter. The stack test now exercises both DIDs, a multi-DID response whose length changed, positive and negative `19 06` cases, and the resident/SRAM boot tests cover F101 plus the FCD preamble.

`EcuM.c` now permits optional STM32 Standby only after successful post-run NvM persistence. A failed or migration-blocked write leaves the RAM state awake. Standby remains compile-time disabled (`ECUM_STANDBY_ENABLED=0`) pending board-level wake and Flash timing evidence. Current linked sizes, validation results and remaining external FCD profile blockers are in [validation](ELC_VALIDATION_RESULTS.md) and [compatibility](../compatibility/ZGW_FCD_GAP_ANALYSIS.md).
