# ELC AUTOSAR-lab, UDS and ISO-TP alignment review - 2026-10-24

This is a source, host-test and Cortex-M-build assessment of ELC_APP and ELC_FBL. It supersedes the module-level wording of the 2026-10-19 records where they differ; those records remain the history of the memory map, journal format and programming contract. No hardware, gateway, FCD or CM5 was touched, nothing was flashed or reset, and nothing was committed.

**Continuation on 2026-10-24:** F101 now returns the six-byte APP/FBL version expected by the inspected FCD and ZGW sources; the legacy reset counter remains at F001. APP ReadDTCInformation 0x19 0x06 returns extended-data record 0x01 (two-byte occurrence counter), and the FBL accepts the FCD preamble services 0x28/0x85. The host regression assertions cover these paths. EcuM Standby now requires a successful post-run NvM write; a failed write keeps the RAM copy awake. The open-decision table below records the earlier review state and is superseded for these items by this paragraph and the current source.

## Goal and yardstick

The goal is an AUTOSAR-Classic-like laboratory stack in the style of ZGW (`BSW/Com`, `BSW/Diag`, `BSW/Mem`): modules with AUTOSAR-shaped APIs and static configuration that call each other directly, **no RTE**, and behavior that follows ISO 14229-1 (UDS) and ISO 15765-2 (ISO-TP) as the standards describe it. It is not a claim of conformance to one AUTOSAR release, not every AUTOSAR module is wanted, and there is no percentage: mandatory-requirement applicability has not been classified requirement by requirement (the SWS PDFs in `D:/GitHub_Repos` were not traced). ZGW is the architectural example, not the standards authority; where ZGW is stricter than the standard (reserved ISO-TP STmin aborts the transfer in ZGW) ELC follows the standard.

The yardstick is the following set of rules. `tools/audit_sources.py` enforces the structural ones on every run.

| Rule | Checked by |
| --- | --- |
| R1 No RTE, DET or MemMap; modules call each other's public API | audit (directory and reference scan) |
| R2 Common types: `Std_ReturnType`, `PduIdType`, `PduInfoType`, `BufReq_ReturnType`; module APIs use AUTOSAR names and shapes | audit (legacy API names must not exist), review |
| R3 PDU routing and CAN identifiers live in static tables (`CanIf_Cfg.c`, `PduR_Cfg.c`, `CanTp_Cfg.c`, `ComStack_Cfg.h`); no raw CAN identifier in CanTp, PduR, Com, Dcm, Dem or CanNm | audit |
| R4 Strict layering: only `Can.c` touches the CAN peripheral, only `Fls.c` the Flash HAL, Dcm talks to PduR/Dem/ComM/DcmAppl only, Com reaches the bus only through PduR | audit (include and call rules) |
| R5 Transport semantics follow ISO 15765-2, payload moves through the TP buffer API (StartOfReception, CopyRxData, RxIndication, CopyTxData, TxConfirmation); lower-layer failure is a timeout, not a negative confirmation | `tests/cantp_test.c` |
| R6 UDS semantics follow ISO 14229-1: sub-function and suppressPosRspMsgIndicationBit, NRC selection/order, P2/P2*/S3, response pending | `tests/bsw_stack_test.c`, `tests/boot_protocol_test.c` |
| R7 Application knowledge (data identifiers, routines, reset policy) is a callout (`APPL/DcmAppl`), not part of the Dcm | audit |
| R8 ELC-specific behavior that has no standard counterpart is named as such (documented extension), never hidden | this document |

## What the review found (before this change)

The previous state worked and was well tested, but it was an ELC-specific stack with AUTOSAR module names:

| Area | Finding | Disposition |
| --- | --- | --- |
| Architecture | Interfaces used `int` results and raw CAN identifiers (`CanIf_TxConfirmation(0x051, result)`, `PduR_ComTransmitTelemetry(0x6EE..)`); IDs, DLCs and routes were hard coded in function bodies; no common types | Fixed: common types, PDU ids, static tables |
| Architecture | CanTp reassembled into its own 256-byte buffer and Dcm copied it again; Dcm used one 120-line `switch` mixing generic UDS with `SMon`, `EcuM`, `Nvm` calls and held the hard-coded DID list | Fixed: TP buffer callbacks into the Dcm buffer (-256 B RAM); service/DID/routine tables; `DcmAppl` callouts |
| Architecture | `Nvm.c` mixed block management, Flash HAL access, legacy-journal import and the 70-field SMon calibration parser (2.4 KB of straight-line code) | Fixed: `Fls` / `Fee` / `NvM` layers; calibration is a 72-slot table in `APPL/SMon/SMon_Calib.c` |
| Architecture | DEM API was `Dem_SetDtc(0x53, 0x2F)` with magic status bytes; communication-control state was duplicated between Dcm (`Dcm_CC`), CanIf gates and CanH | Fixed: event ids and `Dem_SetEventStatus`, UDS status byte, DTC filter, snapshot and clear API; ComM owns the gating |
| ISO-TP | A first frame with FF_DL <= 6 was answered with FC.OVFLW; it is an invalid frame and must be ignored (APP and FBL) | Fixed |
| ISO-TP | Reserved STmin values (0x80..0xF0, 0xFA..0xFF) aborted the transmission; ISO 15765-2 requires the longest value (127 ms) | Fixed |
| ISO-TP | The FC.WAIT counter was never reset by FC.CTS, so N_WFTmax applied per message rather than per series of consecutive waits | Fixed |
| ISO-TP | A consecutive frame that arrived after our FC reached the CAN driver but before its TX confirmation was processed aborted the reception (race with a tester that sends STmin 0) | Fixed |
| ISO-TP | No supervision of our own FC (N_Ar); transmission failure was signalled by a negative confirmation that AUTOSAR CanIf does not have | Fixed: N_Ar/N_As/N_Bs/N_Cr timers; the CAN driver only confirms success |
| ISO-TP | Flow control from the gateway was accepted only with N_TA 0x58. ZGW `CanTp_SetTxAddress()` places its configured tester address 0x41 into frames it sends outside a forwarded-request transmission, which is the case for the FC answering an ECU response. F93's review reaches the same conclusion. Without this a multi-frame ECU response would stall at N_Bs. **Derived from source, not from a bus trace** | Fixed: FC accepted with 0x58 or 0x41 (APP CanTp and FBL boot core) |
| UDS (APP) | `10 02` answered `50 02` (2 bytes) instead of `50 02 P2 P2*` (6 bytes) | Fixed |
| UDS (APP) | `10 02` outside the extended session returned NRC 0x7F (service) instead of 0x7E (sub-function) | Fixed |
| UDS (APP) | suppressPosRspMsgIndicationBit honored only for `3E`; `10`, `11`, `19`, `28`, `31`, `85` rejected or ignored it | Fixed (generic in the Dcm engine) |
| UDS (APP) | NRC 0x78 was sent immediately for asynchronous work instead of when P2 is about to expire; renewal at P2* unchanged | Fixed: first at 40 ms, then every 4.5 s |
| UDS (APP) | `22` response overflow returned NRC 0x31 instead of 0x14; `28` rejected communicationType 2 and applied type 1 to NM reception | Fixed |
| UDS (APP) | A ClearDTC, ECU reset or programming-session request that arrived while the DEM's own 50 ms coalesced write was running failed with NRC 0x22 | Fixed: the request waits for the running write |
| UDS (APP) | Routine `0x0042` multiplied the duration by 1000 but was decremented every 5 ms: the forced load state lasted 5x the requested seconds | Fixed (unit bug, behavior change) |
| UDS (FBL) | Unknown services answered NRC 0x7F in the default session instead of 0x11; sub-function services did not accept the suppress bit (`10 82` was NRC 0x12); no S3 timer, so the FBL never left the programming session | Fixed |

## What changed

Paths are relative to `ELC_APP/BSW` unless noted.

| Module | Files | Shape now |
| --- | --- | --- |
| Common | `Common/Std_Types.h`, `ComStack_Types.h`, `ComStack_Cfg.h` | `Std_ReturnType` (E_OK 0), `PduIdType`, `PduInfoType`, `BufReq_ReturnType`, `RetryInfoType`; symbolic PDU ids for CanIf/CanTp/PduR/Com/Dcm |
| Can | `Com/Can/Can.c,h` | `Can_Write(Hth, Can_PduType*)` returning E_OK/E_NOT_OK/CAN_BUSY, `Can_MainFunction_Write/Read/BusOff`, `Can_IsrRxIndication`; HAL RX callbacks moved here; confirms success to `CanIf_TxConfirmation(PduId)`, reports bus-off through `CanIf_ControllerBusOff` / `CanIf_ControllerModeIndication` |
| CanIf | `Com/CanIf/CanIf.c,h`, `CanIf_Cfg.c` | `CanIf_Transmit(TxPduId, PduInfoType*)`, `CanIf_RxIndication(Can_HwType*, PduInfoType*)`, `CanIf_SetPduMode/GetPduMode`; 6 RX and 14 TX PDUs in tables with CAN id, DLC contract and upper layer; diagnostics are `ALWAYS_ONLINE`, XCP is variable-DLC |
| CanTp | `Com/CanTp/CanTp.c,h`, `CanTp_Cfg.c` | AUTOSAR TP API (`CanTp_Transmit(id, PduInfoType*)`, `CanTp_RxIndication`, `CanTp_TxConfirmation`, Cancel*), channel configuration (extended addressing, BS 8, STmin 5 ms, N_As/N_Ar/N_Bs/N_Cr 1 s, N_WFTmax 3); no message buffer of its own |
| PduR | `Com/PduR/PduR.c,h`, `PduR_Cfg.c` | Routing tables Com<->CanIf (12 TX, 2 RX) and Dcm<->CanTp; TP callbacks `PduR_CanTpStartOfReception/CopyRxData/RxIndication/CopyTxData/TxConfirmation` |
| Com | `Com/Com/Com.c,h` | `Com_SendSignal` / `Com_ReceiveSignal` / `Com_GetRxStatus`, `Com_RxIndication(PduIdType, PduInfoType*)`, `Com_TxConfirmation(PduIdType)`, I-PDU group gates `Com_SetRxAllowed/SetTxAllowed`; lost confirmations re-arm after 30 ticks |
| CanNm, CanSM, ComM | `Com/CanNm`, `CanSM`, `ComM` | Events instead of polling: CanNm -> `ComM_Nm_NetworkMode/BusSleepMode`; CanIf -> `CanSM_ControllerBusOff/ModeIndication`; CanSM -> `CanIf_SetPduMode` and `ComM_BusSM_ModeIndication`; ComM owns the UDS `28` gates (`ComM_CommunicationControl`, `ComM_IsTxAllowed/IsRxAllowed`) |
| Dcm | `Diag/Dcm/Dcm.c,h`, `Dcm_Cfg.c,h`; `APPL/DcmAppl/DcmAppl.c,h` | Service table (session mask, minimum length, sub-function flag, handler); handlers return an NRC, 0 or `DCM_E_PENDING`; DID table with mask `0xFEFF` (F0xx/F1xx aliases), routine table; engine owns sub-function suppression, NRC order, P2/P2*/S3, 0x78, post-response reset |
| Dem | `Diag/Dem/Dem.c,h`, `Dem_Types.h`, `Dem_Cfg.c,h` | `Dem_SetEventStatus/GetEventFailed/GetEventUdsStatus/GetDTCOfEvent`, `Dem_SetOperationCycleState`, `Dem_SetDTCFilter` + `GetNumberOfFilteredDTC/GetNextFilteredDTC`, `Dem_GetSnapshotDataByDTC`, `Dem_ClearDTC` with AUTOSAR repeat-call result (`DEM_CLEAR_PENDING` ... `OK`/`MEMORY_ERROR`); event ids 0..17 = DTC 0x50..0x61; freeze frame DID layout (F300..F30B) is configuration |
| NvM / Fee / Fls | `Mem/Nvm/NvM.c,h`, `NvM_Cfg.c,h`; `Mem/Fee`; `Mem/Fls`; `Mem/MemIf/MemIf_Types.h` | `NvM_ReadAll/WriteAll/WriteBlock/ReadBlock/GetErrorStatus/MainFunction`; block descriptors with init callbacks (the 644 bytes of zero/default tables are gone from Flash); Fee keeps the unchanged 1 KiB two-page journal; Fls is the only Flash HAL user |
| Crc | `Sys/Crc/Crc.c,h` | `Crc_CalculateCRC32(ptr, len, start, isFirst)` replaces three private CRC loops |
| EcuM / tasks | `Sys/EcuM`, `Src/freertos.c`, `Src/main.c` | `*_MainFunction` naming; BSW task: EcuM, CanNm, ComM, CanSM, Com RX, CanH, Com TX (5 ms); diag task: Can read/write/bus-off and XCP (1 ms), CanTp/Dcm/Dem/NvM (5 ms) |
| SMon | `APPL/SMon/SMon_Calib.c` | Calibration page layout, defaults and validation as one table generated mechanically from the previous loader; the 72 slots match (checked by script and by `bsw_stack_test.c`) |
| FBL | `ELC_FBL/Core/Inc/boot_protocol.h` | Explicit `BootTp_*` (ISO 15765-2) and `BootDcm_*` (ISO 14229-1) layers over the `BootPort_*` ports; still one dependency-free header instantiated by the resident FBL and the SRAM updater |

Wire contract unchanged: CAN identifiers and DLCs, signal packing and cycles, DID values, RID numbers, the 292-byte calibration layout, the NvM journal layout and the memory map. `verify_firmware.py` still pins the retained session word at `0x20004FC0` and the 44 SRAM relocations.

## Behavior changes that testers and tools can see

1. `10 02` is answered `50 02 00 32 01 F4`; `10 02` from a non-extended session is NRC 0x7E (was a 2-byte response and NRC 0x7F).
2. NRC 0x78 appears about 40 ms after a request that is still being processed (clear, reset, programming-session entry) instead of immediately; renewal every 4.5 s.
3. Every sub-function service honors bit 7 (suppressPosRspMsgIndicationBit). Negative responses, including 0x78, are never suppressed. A suppressed `11 81` or `10 82` still performs the reset.
4. `28`: controlType 0..3 with communicationType 1 (normal), 2 (NM) or 3 (both) is accepted; reception and transmission of normal messages are gated independently as the control type says.
5. `22`: an oversized answer is NRC 0x14.
6. ClearDTC, `11 01` and `10 02` wait for a write that is already running instead of failing with NRC 0x22. NRC 0x22 remains for "load output active", "vehicle state not 6" and "Flash work not allowed".
7. Routine `31 01 00 42 st secs` forces the load state for `secs` real seconds (was 5x).
8. ISO-TP: FF_DL <= 6 ignored, reserved STmin waits 127 ms, flow control accepted with N_TA 0x58 and 0x41, and a lost transmit confirmation ends a transfer after N_As (1 s) instead of immediately.
9. FBL: unknown services are NRC 0x11 in every session; `10 81/82`, `11 81`, `31 81 ..` are accepted; after the tester has sent its first request the programming session ends after 5 s without a request (S3Server, `BOOT_S3_MS`, 0 disables). Before the first request the FBL stays in the programming session indefinitely, as before.

## Distance to the goal

| Area | State now | Not done (and why) |
| --- | --- | --- |
| APP communication stack | AUTOSAR-shaped Can/CanIf/CanTp/PduR/Com with static configuration and the TP buffer API; CanNm/CanSM/ComM exchange the standard indications; layering enforced by the audit | Functional (broadcast) addressing and CAN FD are not offered (physical, extended-address, classic CAN only). CanTp answers a busy upper layer by dropping the frame instead of sending FC.WAIT. One channel, one controller. NM is receive-only and ComM has no user arbitration because no source defines more. Timer granularity is the 5 ms main function |
| APP diagnostics | Table-driven Dcm, UDS sub-function/NRC/P2/P2*/S3/0x78 behavior covered by assertions; Dem with the UDS status byte and AUTOSAR-shaped API | Services offered: 10, 11, 14, 19 (01/02/04/06), 22, 28, 31, 3E, 85. No SecurityAccess, WriteDataByIdentifier, IO control or functional addressing. A request that arrives while another one is being processed is dropped (no NRC 0x21). Dem has no debounce counters (PREPASSED/PREFAILED rejected), no aging threshold, one occurrence extended-data record and one freeze frame record; `19 04` with DTC 0xFFFFFF is an ELC extension (strict ISO answers NRC 0x31) |
| APP storage | `Fls` / `Fee` / `NvM` layers; journal format and the "never erase unrecovered legacy data" rule unchanged | `NvM_ReadAll` is synchronous at start-up and a block write stores the whole snapshot (by design: DEM status, freeze frames and battery model commit atomically); the MemIf layer is reduced to a result type |
| APP system | `*_MainFunction` naming, direct calls, EcuM/ComM/CanSM wiring without CanH in the middle | The scheduler is FreeRTOS/CMSIS tasks, not an AUTOSAR OS; EcuM is the existing run/post-run state machine, not the AUTOSAR EcuM state set |
| FBL | Transport and diagnostic layers named and separated; ISO-TP and UDS deviations fixed; both instances covered by the same assertions | Not an AUTOSAR module set (the boot core must stay one dependency-free, function-pointer-free header so it can be copied to SRAM); no SecurityAccess; single-copy resident FBL is not power-loss recoverable |

## Open decisions and risks

| Item | Owner / evidence | Needed |
| --- | --- | --- |
| FBL S3Server | ELC (this change). ZGW's own FBL has no S3; FCD behavior is unknown | Confirm FCD sends TesterPresent (or finishes each programming step within 5 s). If not, set `BOOT_S3_MS` to 0 in `boot_protocol.h` |
| FC address 0x41 | ZGW source (`CanTp.c` `CanTp_SetTxAddress`, config rows 57-61, `Dcm_Cfg.h` `DCM_EXT_ADDR_TESTER`); F93 docs agree | Bus trace of a multi-frame ECU response through the real gateway |
| APP DID F101 | Resolved in the 2026-10-24 continuation: F101 is the six-byte APP/FBL version; F001 retains the reset counter | Check existing consumers of the former F101 counter before using the new image on hardware |
| `19 04` FFFFFF | ELC extension, retained | Decide whether to return NRC 0x31 like strict ISO |
| Programming gate | Vehicle state 6, extended session, load output off (ELC policy) | Unchanged |
| Hardware | none of the above ran on a board | Bench measurement of P2/P2*, ISO-TP timing, Flash stall, stack high-water, FCD end-to-end |

## Evidence

Executed from this checkout (see [validation](../modernization/ELC_VALIDATION_RESULTS.md) for sizes and hashes): `python tools/build_firmware.py`, `verify_firmware.py`, `run_host_tests.py` (fee, cantp, bsw_stack, boot_1, boot_2), `audit_sources.py`. The host tests compile the production sources unmodified and mock only the HAL and OS data; they do not execute Cortex-M code, interrupts, Flash timing or a bus. `git diff --check` passed in the continuation; nothing was committed.
