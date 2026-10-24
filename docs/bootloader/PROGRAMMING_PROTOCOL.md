# Current ELC programming protocol

Source: `Core/Inc/boot_protocol.h`, instantiated by `Core/Src/main.c` and `Core/Src/ram_blob.c`. Execution architecture remains a resident APP writer and an independent SRAM FBL writer. [Closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) documents changes; [validation](../modernization/ELC_VALIDATION_RESULTS.md) separates native/ELF evidence from hardware proof.

| Item | Current target contract |
| --- | --- |
| CAN/address | Standard710 both directions; RX N_TA58, TX N_TAF0; flow control from the tester side is accepted with N_TA58 or41 (`BOOT_FC_ADDRESS`, the address ZGW places in its FC frames; derived from ZGW `CanTp.c`, not bus-verified) |
| Layers | `BootTp_*` (ISO 15765-2 transport: `BootTp_RxIndication`, `BootTp_Transmit`) and `BootDcm_*` (ISO 14229-1: `BootDcm_Process`, `BootDcm_Respond`, `BootDcm_Nrc`) over the `BootPort_*` hardware ports; `Boot_Init` / `Boot_MainFunction` drive N_Cr, N_Bs and S3Server. The same header is compiled into the resident FBL (block 1) and the SRAM updater (block 2). |
| Addressing | Extended only; no silent702/703 fallback |
| RX transport | SF/FF/CF, 240-byte UDS maximum, BS8/STmin5, sequence checks and1 s N_Cr timeout; FF_DL <= 6 is invalid and ignored (no FC) |
| Response transport | SF up to6 bytes; seven-byte version response uses FF/FC/CF; physical TX success is checked |
| Session | `10 01` / `10 02` answer `50 sub 00 32 01 F4` (P2 50 ms, P2* 5 s); F186/F17E report it; sub-function 0x80 (suppressPosRspMsgIndicationBit) is honored on `10`, `11`, `31` and `3E`, negative responses are never suppressed |
| FCD software version | `22 F1 01` returns APP major/minor/patch plus FBL major/minor/patch (six bytes); `22 F1 80` retains the four-byte ELC version. The APP uses the same F101 six-byte layout. |
| Unsupported services | NRC 0x11 in every session for services the FBL does not implement (anything but 10, 11, 22, 28, 31, 34, 36, 37, 3E, 85); implemented programming services outside the programming session answer 0x7F. The FCD preamble's 28 01 03 and 85 02 are accepted; the FBL has no application COM or DEM state to switch. |
| S3Server | `BOOT_S3_MS` = 5000 ms. Armed by the tester's first request; every request (including `3E`) restarts it; expiry returns to the default session (programming services refuse with 0x7F until `10 02` is sent again). Before the first request the FBL waits in the programming session indefinitely. Set `BOOT_S3_MS` to 0 to disable. Not yet verified with FCD. |
| Select writer | 31 01 02 00 [01 APP /02 FBL], with acknowledged resident-to-RAM handover |
| RAM entry | 31 01 01 55; legacy0007 retained |
| Erase | 31 01 00 01 address[4] size[4], BE and bounded to current writer; legacy0004 APP/0006 FBL and0005 calibration retained |
| Download | 34 00 44 address[4] size[4], BE; response74 10 F0 (240-byte total request max) |
| Transfer | 36 BSC payload; exact previous request replay acknowledged without rewriting; changed/future BSC gives73; wraps through00 |
| Exit | 37 or37 CRC32[4] BE; complete byte count and final padded word required |
| Verify | 31 01 00 02 address[4] size[4] CRC32[4], BE; legacy0000 uses LE CRC |
| CRC | Reflected IEEE CRC32, seedFFFFFFFF/final inversion; pending78 precedes potentially long CRC/erase work |
| APP boot footer | Resident writes full-span CRC of08003C00..0800F7FB to0800F7FC; boot requires vectors and footer CRC |
| Reset | 11 01 after verified SRAM replacement; APP reset only if valid application; physical response confirmation precedes reset |

APP RequestDownload excludes the CRC footer and parameter/NvM pages. RAM downloads start08000000 and remain below08003800; vectors must lie inside the received image. A single contiguous APP image per erase is supported; package disjoint segments before transfer rather than overwriting a programmed footer repeatedly. ELF evidence verifies SRAM independence but not power-loss recovery. Physical flashing was not performed.
