> Source baseline updated 2026-10-19. Historical details below are superseded by [functional closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) and [validation](../modernization/ELC_VALIDATION_RESULTS.md). Verify current source before using older protocol or memory descriptions.

# Boot and handover

## FBL selection

`main()` starts HAL/clock/CAN/CRC, configures falling-edge PVD, clears retained 64-byte SRAM area after POR, then checks reset-counter range 50–52 or retained session state if APP vector word is present. Programming status remains in FBL; otherwise execution calls `FBL_JumpToAppl`. Startup has no image signature or stored CRC validation before jumping.

## APP handover

`FBL_JumpToAppl` disables IRQs, clears pending CAN/RCC/FLASH/PVD IRQs, stops SysTick, sets VTOR to `0x08003C00`, loads MSP from first APP vector word and branches to reset vector. It does not validate MSP range, Thumb bit/reset-vector range, CRC or reset all peripheral state. APP startup must tolerate inherited peripheral clocks/configuration.

## RAM updater handover

`FBL_RunRamUpdater` disables IRQs and SysTick, copies linked blob LMA to RAM VMA, reads initial stack/reset vectors, updates VTOR/MSP and branches. The blob masks NVIC bank 0, clears updater state, then re-enables CPU interrupts (faults can still run) and polls CAN forever. It does not return to resident FBL; successful reset path requests system reset. RAM vector defaults record fault registers at `0x20000100` and spin.

No runtime hardware test confirms either handover. Review vector alignment, Thumb-bit semantics, interrupts, peripheral state, stack/data placement and flash range on the actual board.
