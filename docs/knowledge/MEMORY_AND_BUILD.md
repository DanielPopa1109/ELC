# Current memory and build

Both images are now linked for STM32F103 Cortex-M3 with GNU Arm15.2.1; [validation](../modernization/ELC_VALIDATION_RESULTS.md) records actual sizes, hashes and commands. Generated legacy Debug makefiles still contain obsolete paths and are not the validation build. `tools/build_firmware.py` compiles local sources, including every BSW source, and emits ELF/MAP/disassembly/HEX/BIN plus SHA-256 manifests. CubeIDE FBL links/includes use the ELC APP HAL/CMSIS tree.

Flash: FBL0000..37FF, calibration3800..3BFF, APP3C00..F7FB, CRCF7FC..F7FF, NvMF800..FFFF, all relative to08000000. SRAM retention is20004FC0..20004FFF; application session word is fixed at20004FC0. `verify_firmware.py` checks boundaries and all RAM-blob relocations. Linker assertions reserve the updater stack margin. See the full [memory map and journal format](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md).

APP is close to capacity. No Flash partition or hardware setting was expanded. Native tests and successful linking do not establish RTOS stack high-water, IRQ deadlines or hardware reliability.
