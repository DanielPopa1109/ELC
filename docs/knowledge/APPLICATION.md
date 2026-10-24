> Source baseline updated 2026-10-24 ([AUTOSAR-lab alignment](../autosar/AUTOSAR_LAB_ALIGNMENT.md)). Verify current source before using older protocol or memory descriptions.

# Application

## Startup and scheduling

`ELC_APP/Src/main.c` calls HAL init, clock setup, GPIO, DMA, CRC, ADC, CAN, timer and FreeRTOS initialization; starts PWM and ADC DMA; checks application/FBL software-version state in the reserved SRAM area; then starts the scheduler. The BSW initialization order is `Can_Init`, `CanIf_Init`, `CanTp_Init`, `PduR_Init`, `Com_Init`, `CanNm_Init`, `CanSM_Init`, `ComM_Init`, `CanH_Init`, `Dcm_Init`, `NvM_Init`, `NvM_ReadAll`, `Dem_Init`, `SMon_CalibInit`. ADC completion computes calibrated voltage, current and temperature values and publishes them to SMon globals.

`ELC_APP/Src/freertos.c` creates CMSIS-RTOS v2 threads. All periodic functions use `vTaskDelayUntil`:

| Thread | Period | Calls |
| --- | --- | --- |
| `QM_BSW` | 5 ms | `EcuM_MainFunction`, `CanNm_MainFunction`, `ComM_MainFunction`, `CanSM_MainFunction`, `Com_MainFunctionRx`, `CanH_MainFunction`, `Com_MainFunctionTx` |
| `QM_APPL` | 5 ms | `SMon_main` |
| `QM_DIAG` | 1 ms, every 5th tick additionally | `Can_MainFunction_Read/Write/BusOff`, `Xcp_MainFunction`; then `CanTp_MainFunction`, `Dcm_MainFunction`, `Dem_MainFunction`, and `NvM_MainFunction` only while `CanTp_IsIdle()` |
| `CPULOAD_OS` | - | CPU-load reporting |

A 5-ms timer and Timer 3 supply run-time statistics. The tick hook restarts ADC DMA while software state allows it. Stack overflow and allocation failure hooks request a reset. This is a FreeRTOS task set, not an AUTOSAR OS.

## Responsibilities

- `BSW/Common`: `Std_ReturnType`, `PduIdType`, `PduInfoType`, `BufReq_ReturnType`, symbolic PDU ids.
- `BSW/Com`: `Can` -> `CanIf` -> `CanTp`/`PduR` -> `Com`/`Dcm`, `CanNm`, `CanSM`, `ComM`, and `CanH` (XCP slave and periodic telemetry on top of the Com signals). Static configuration in `*_Cfg.c`.
- `BSW/Diag/Dcm` and `APPL/DcmAppl`: table-driven UDS engine; DID readers, routines and reset policy are `DcmAppl` callouts.
- `BSW/Diag/Dem`: DTC table/status, UDS status byte, filters, snapshots and clear.
- `BSW/Mem`: `NvM` (blocks) over `Fee` (journal) over `Fls` (Flash driver); `BSW/Sys/Crc` for CRC-32.
- `BSW/Sys/EcuM`: run/post-run state, wake pin, reset bookkeeping and standby.
- `APPL/SMon`: power/load sensing, protection logic, battery-state calculations, control outputs and the calibration table `SMon_Calib.c`.

These are modules in the style of the ZGW lab stack, called directly (no RTE), not evidence of full AUTOSAR conformance. The scope and the open deviations are in [AUTOSAR-lab alignment](../autosar/AUTOSAR_LAB_ALIGNMENT.md); follow code paths rather than module names when migrating.
