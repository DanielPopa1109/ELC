> Source baseline updated 2026-10-19. Historical details below are superseded by [functional closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) and [validation](../modernization/ELC_VALIDATION_RESULTS.md). Verify current source before using older protocol or memory descriptions.

# Tools and tests

Tracked engineering artifacts include:

- `Tool/A2L/A2L_ELC.a2l` for calibration/measurement metadata.
- `Tool/Documentation/Tests.xlsx` and `I2T_Ratings.xlsx` for test and rating data.
- KiCad Rev A schematic, PCB, BOM/CSV and manufacturing outputs under `Tool/Documentation/HW/Rev A/`.
- `ZGW_CAN_3/` DBC/PDBC, TSMaster projects, diagnostic/test scripts, captures and historical binary/configuration files.

These assets describe intended design or test scenarios. A DBC label is not definitive proof of current firmware behavior; a HEX file or stale A2L export is not a reproducible build; spreadsheet entries do not establish a successful test unless result/evidence is recorded. FBL behavior has not been hardware-validated during this analysis.

No tests, builds, flashing or target operations were run. Before asserting release readiness, use a controlled bench with matching board, verified MCU density, current build artifacts, CAN trace, power-fault control and recovery procedure. See [FBL validation plan](../bootloader/VALIDATION_TESTS.md).
