> Source baseline updated 2026-10-24. Historical details below are superseded by [functional closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) and [validation](../modernization/ELC_VALIDATION_RESULTS.md). Verify current source before using older protocol or memory descriptions.

# ELC knowledge bank

Start here. This repository is a STM32F103C8T6 load-control ECU, with independent application and FBL projects plus board files, calibration data, and CAN test artifacts.

## Topics

- [Architecture](ARCHITECTURE.md) — project boundaries, runtime and dependencies.
- [STM32 platform](STM32_PLATFORM.md) — MCU, clocks, pins, interrupt and watchdog evidence.
- [Application](APPLICATION.md) — tasks and ECU modules.
- [Communications](COMMUNICATIONS.md) — CAN, signals and transport.
- [Diagnostics](DIAGNOSTICS.md) — DCM/UDS behavior.
- [Memory and build](MEMORY_AND_BUILD.md) — linker map, persistent state, builds.
- [Tools and tests](TOOLS_AND_TESTS.md) — engineering tools and evidence limitations.
- [FBL knowledge bank](../bootloader/INDEX.md) — updater source-level trace.
- [ZGW/FCD compatibility](../compatibility/INDEX.md) — current integration gap analysis.

Claims are derived from tracked source/configuration. Binary outputs, historical tests and board files do not independently prove the behavior of a currently connected ECU. See each page for unresolved items.

Current results: [AUTOSAR-lab / UDS / ISO-TP alignment](../autosar/AUTOSAR_LAB_ALIGNMENT.md), [functional coverage](../autosar/BSW_FUNCTIONAL_COVERAGE.md), [gap closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md), [executed validation](../modernization/ELC_VALIDATION_RESULTS.md).
