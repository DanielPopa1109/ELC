> Source baseline updated 2026-10-19. Historical details below are superseded by [functional closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) and [validation](../modernization/ELC_VALIDATION_RESULTS.md). Verify current source before using older protocol or memory descriptions.

# ELC FBL knowledge bank

Highest priority: the updater consists of a flash-resident application programmer and a RAM-resident FBL self-programmer. They have similar CAN/UDS vocabulary but different implementations, limits and error handling.

- [Architecture](FBL_ARCHITECTURE.md)
- [Updater protocol and source trace](FBL_UPDATER.md)
- [Flash operations](FLASH_DRIVER.md)
- [Memory/linker](MEMORY_AND_LINKER.md)
- [Boot and handover](BOOT_AND_HANDOVER.md)
- [UDS/ISO-TP contract](PROGRAMMING_PROTOCOL.md)
- [State and recovery](STATE_MACHINE_AND_RECOVERY.md)
- [Portability](STM32_PORTABILITY.md)
- [Reusable implementation guide](REUSABLE_IMPLEMENTATION_GUIDE.md)
- [Validation plan](VALIDATION_TESTS.md)

Source of truth: `ELC_FBL/Core/Src/main.c`, `ram_blob.c`, `can.c` and `STM32F103C8TX_FLASH.ld`. Claims below describe source, not proven ECU behavior. Neither power-loss recovery nor live FCD compatibility is established.

Current results: [functional coverage](../autosar/BSW_FUNCTIONAL_COVERAGE.md), [gap closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md), [executed validation](../modernization/ELC_VALIDATION_RESULTS.md).
