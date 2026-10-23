# ELC repository guidance

ELC is an STM32 ECU reference implementation. ZGW defines current gateway integration interfaces and FCD is the diagnostic/flashing reference tool; F93 supplies additional STM32 bootloader examples.

Before cross-project changes, inspect the target and the relevant ZGW/FCD/F93 source. Treat the knowledge bank under `docs/` as a map, then verify implementation and configuration because documentation, generated artifacts, and simulated target profiles can drift.

For firmware updates, start with [ELC FBL knowledge bank](docs/bootloader/INDEX.md), especially its distinct application writer and RAM self-updater. Separate gateway routing, ECU application behavior, MCU/hardware-specific flash/boot requirements, reusable protocol/state-machine ideas, and new development. Never copy MCU-specific flash, linker, vector or startup code blindly.

Update relevant docs when source, linker maps, diagnostic contracts, FCD target behavior or gateway routes change. Mark simulated/unverified behavior clearly; cite source paths/functions and exact configuration values. No behavior should be considered hardware validated solely from checked-in config, HEX, DBC, test scripts or docs.


## Current BSW validation workflow

Modify only ELC. Keep ZGW/F93/G26/FCD read-only; no ECU flashing/reset, CM5 deployment, commits or pushes without authorization. Credentials must never enter source/reports; `codex_credentials.env` is ignored.

Current baseline: [functional closure](docs/modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md), [module coverage](docs/autosar/BSW_FUNCTIONAL_COVERAGE.md), [validation](docs/modernization/ELC_VALIDATION_RESULTS.md).

Build with `python tools/build_firmware.py`, then `python tools/verify_firmware.py`, `python tools/run_host_tests.py`, `python tools/audit_sources.py`. The standalone build uses ELC's local HAL/CMSIS/FreeRTOS sources and a Cortex-M3 GNU Arm compiler; do not use the stale absolute paths in checked-in generated Debug makefiles. Keep the RAM blob flags and linker assertions. Native tests need a Windows native C compiler (the runner uses the local Zig package).

Never infer hardware readiness from host assertions or ELF checks. Retain the 64 KiB F103 geometry, both 1 KiB NvM banks, separate calibration page and CRC footer. A fully occupied legacy journal must be backed up and converted independently; never erase its active records to force migration. Single-copy resident FBL update is not power-loss recoverable. Source changes must regenerate the coverage/validation record and source inventory.
