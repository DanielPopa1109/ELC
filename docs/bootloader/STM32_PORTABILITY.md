> Source baseline updated 2026-10-19. Historical details below are superseded by [functional closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) and [validation](../modernization/ELC_VALIDATION_RESULTS.md). Verify current source before using older protocol or memory descriptions.

# STM32 portability

Reusable concepts: protocol state machine, range-checked address/length representation, chunk buffer, image digest interface, boot request record, common diagnostic adapter and test vectors. These can be redesigned/reused independently of MCU.

Must adapt per family: flash base/size/erase unit, unlock/option-byte behavior, programming granularity, alignment, read-while-write banks, cache/prefetch, busy execution rules, voltage range, ECC/errors, watchdog timing, vector table size/alignment, SRAM bounds, boot address and peripheral mux. ELC's 1-KiB pages, raw `0x40022000` flash registers and STM32F1 keys are not generic STM32 APIs.

The RAM blob is a single linked section with direct register access and a hard-coded vector MSP. Reproduction requires a family-specific linker MEMORY map, load-vs-run symbols, copy routine, complete no-flash-call closure (including constants/compiler helpers), RAM vectors, stack, initialized data, IRQ policy, CAN peripheral setup assumptions and reset mechanism. Audit map/disassembly to prove every dependency runs from RAM during flash busy.

Do not copy F93's bootloader or ELC's blob blindly. Read target MCU reference manual/errata and the destination app/FBL linker/build/startup first. See [implementation guide](REUSABLE_IMPLEMENTATION_GUIDE.md).

## F93 comparison

The F93 bootloader knowledge bank describes two distinct STM32F4 flashloaders (CBM and DMU). DMU also uses classic CAN IDs `0x702/0x703`, which is a useful addressing precedent, but F93's documented parser uses a limited single-frame UDS-like layout and does not provide ELC's segmented ISO-TP image stream or RAM self-programmer. Its linker layouts, erase geometry, peripheral setup and recovery behavior are separate. Reuse design questions and validation lessons from [F93 bootloader docs](../../../F93/docs/bootloader/INDEX.md); do not import its code or assume protocol compatibility.
