> Source baseline updated 2026-10-19. Historical details below are superseded by [functional closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) and [validation](../modernization/ELC_VALIDATION_RESULTS.md). Verify current source before using older protocol or memory descriptions.

# ELC compatibility

ELC has an application CAN/UDS stack and a direct-CAN FBL, but checked-in FCD configuration marks ELC as simulated and unselected. Compatibility is therefore a source comparison, not a demonstrated integration.

- [ZGW/FCD gap analysis](ZGW_FCD_GAP_ANALYSIS.md)
- [Migration requirements](MIGRATION_REQUIREMENTS.md)
- [Compatibility test plan](COMPATIBILITY_TESTS.md)

Gateway forwarding, FCD target descriptor, ELC APP DCM, and ELC FBL are separate boundaries. Verify each hop and distinguish source configuration from live deployment.

Current results: [functional coverage](../autosar/BSW_FUNCTIONAL_COVERAGE.md), [gap closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md), [executed validation](../modernization/ELC_VALIDATION_RESULTS.md).
