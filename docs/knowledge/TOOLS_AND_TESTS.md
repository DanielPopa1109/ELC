> Source baseline updated 2026-10-24. Historical details below are superseded by [functional closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) and [validation](../modernization/ELC_VALIDATION_RESULTS.md). Verify current source before using older protocol or memory descriptions.

# Tools and tests

Tracked engineering artifacts include:

- `Tool/A2L/A2L_ELC.a2l` for calibration/measurement metadata.
- `Tool/Documentation/Tests.xlsx` and `I2T_Ratings.xlsx` for test and rating data.
- KiCad Rev A schematic, PCB, BOM/CSV and manufacturing outputs under `Tool/Documentation/HW/Rev A/`.
- `ZGW_CAN_3/` DBC/PDBC, TSMaster projects, diagnostic/test scripts, captures and historical binary/configuration files.

These assets describe intended design or test scenarios. A DBC label is not definitive proof of current firmware behavior; a HEX file or stale A2L export is not a reproducible build; spreadsheet entries do not establish a successful test unless result/evidence is recorded. FBL behavior has not been hardware-validated during this analysis.

Local validation tooling now exists (`tools/build_firmware.py`, `verify_firmware.py`, `run_host_tests.py`, `audit_sources.py`; see `tools/README.md`): Cortex-M3 builds of both images, ELF/SRAM-relocation checks, five native assertion binaries (`fee`, `cantp`, `bsw_stack`, `boot_1`, `boot_2`) and an architecture audit. Their results are in [validation](../modernization/ELC_VALIDATION_RESULTS.md). They do not replace hardware tests: no flashing or target operation was run, and the spreadsheets/DBC/captures above remain unverified evidence. Before asserting release readiness, use a controlled bench with matching board, verified MCU density, current build artifacts, CAN trace, power-fault control and recovery procedure. See [FBL validation plan](../bootloader/VALIDATION_TESTS.md).
