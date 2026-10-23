# Current ELC programming protocol

Source: `Core/Inc/boot_protocol.h`, instantiated by `Core/Src/main.c` and `Core/Src/ram_blob.c`. Execution architecture remains a resident APP writer and an independent SRAM FBL writer. [Closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) documents changes; [validation](../modernization/ELC_VALIDATION_RESULTS.md) separates native/ELF evidence from hardware proof.

| Item | Current target contract |
| --- | --- |
| CAN/address | Standard710 both directions; RX N_TA58, TX/FC N_TAF0 |
| Addressing | Extended only; no silent702/703 fallback |
| RX transport | SF/FF/CF, 240-byte UDS maximum, BS8/STmin5, sequence checks and1 s timeout |
| Response transport | SF up to6 bytes; seven-byte version response uses FF/FC/CF; physical TX success is checked |
| Session | 10 02; F186/F17E report it; TesterPresent supports suppress bit |
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
