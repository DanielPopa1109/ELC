> Source baseline updated 2026-10-19. Historical details below are superseded by [functional closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) and [validation](../modernization/ELC_VALIDATION_RESULTS.md). Verify current source before using older protocol or memory descriptions.

# Validation plan

No validation run is claimed by this knowledge bank. Suggested staged tests on controlled bench hardware:

1. **Artifact/map**: build both configurations; inspect ELF/map/HEX address ranges, RAM blob load/VMA/size, stack, vectors, application CRC reservation and exact MCU density.
2. **Protocol**: replay valid/malformed ISO-TP SF/FF/CF/FC; wrong CF SN; oversized lengths; BSC duplicate/gap/wrap; wrong ALFI; boundary/overflow addresses; mismatched and truncated lengths; NRC expectations.
3. **Flash**: verify every erase/program return and readback; test alignment, erased-state checks, CRC known vectors and image boundary, while recording CAN response latency.
4. **Handover**: APP good/bad vectors, CRC mismatch, retained state, POR/software reset, stale peripherals and RAM blob fault path.
5. **Recovery**: controlled interruption at each erase/write/verify boundary using sacrificial boards; verify APP failure leaves FBL serviceable and define FBL self-update recovery.
6. **Integration**: ZGW routed diagnostic path and FCD real-target descriptor, compare CAN trace to exact service/RID/address/CRC contract; confirm timeouts and response routing.
7. **Transport and session timing (added 2026-10-24)**: through the real gateway, capture the flow-control N_TA the gateway uses for a multi-frame FBL response (the boot core accepts 58 and 41), confirm FCD tolerates the 5 s S3Server (`BOOT_S3_MS` in `boot_protocol.h`) between its requests or sends TesterPresent, and check that `10 82`, `11 81` and unsupported service numbers behave as in [PROGRAMMING_PROTOCOL.md](PROGRAMMING_PROTOCOL.md). The host assertions in `tests/boot_protocol_test.c` cover the logic only.

Do not use real vehicle/production hardware for interruption testing. Tests require explicit separate authorization for flashing or destructive fault injection.
