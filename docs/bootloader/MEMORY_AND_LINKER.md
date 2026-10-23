> Source baseline updated 2026-10-19. Historical details below are superseded by [functional closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) and [validation](../modernization/ELC_VALIDATION_RESULTS.md). Verify current source before using older protocol or memory descriptions.

# FBL memory and linker contract

| Region | Address range | Source use |
|---|---:|---|
| FBL flash | `0x08000000..0x080037FF` | 14 KiB; resident FBL and RAM blob load image |
| Parameter page | `0x08003800..0x08003BFF` | 1 KiB; separate erase RID |
| APP | `0x08003C00..0x0800F7FF` | 48,128 bytes addressable by FBL erase constants |
| APP CRC word | `0x0800F7FC..0x0800F7FF` | final word reserved by convention |
| Retained SRAM | `0x20004FC0..0x20004FFF` | 64-byte `.ncr` retained region |

APP linker length 48,125 bytes maps through `0x0800F7FC` inclusive. The FBL reserves the four-byte CRC at `0x0800F7FC..0x0800F7FF`, so the APP linker region appears to overlap the first CRC byte; inspect the actual map/HEX and fix the boundary before release. FBL linker makes `.ram_blob` VMA in RAM and LMA in FLASH, exports load/VMA symbols and places vector table first. RAM updater hardcodes vector MSP `0x20004F80`; this leaves a 64-byte gap below the `.ncr` region at `0x20004FC0`.

The map assumes 64-KiB flash and 20-KiB SRAM. Yet the Cube compile define is `STM32F103xB`; verify actual part density and ELF/map before programming. A changed blob can overflow reserved FBL space or RAM; add linker assertions and inspect copy length/stack overlap in a future implementation.
