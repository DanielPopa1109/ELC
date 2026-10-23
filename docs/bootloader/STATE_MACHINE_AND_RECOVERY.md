> Source baseline updated 2026-10-19. Historical details below are superseded by [functional closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) and [validation](../modernization/ELC_VALIDATION_RESULTS.md). Verify current source before using older protocol or memory descriptions.

# State machine and recovery

The resident FBL has a small state enum: programming or hard-reset/default. `10 02` sets programming; `.ncr` retained SRAM at `0x20004FC0` holds session status across reset, and a separate reset counter is checked for 50–52. POR clears 64 bytes. FBL programming mode is partly inferred by non-erased APP first word plus retained state. The RAM updater writes state 2 before reset.

This is not a transactional A/B design. APP is erased before replacement and programmed in place. No alternate slot, monotonic version, signed manifest, authenticated session, explicit image-valid marker or boot-time CRC verification is shown. Power loss during APP erase/write can leave an invalid APP; FBL should remain bootable if its own flash is intact, but that recovery path is not hardware-verified. Power loss during RAM self-update can corrupt the sole FBL and strand the unit; no backup/recovery image is shown.

Transfer handling lacks block sequence validation, expected-size enforcement and durable progress checkpointing. CRC detects data mismatch only when routine completes; it is not authenticity/security. Callers discard flash operation statuses. Reset or interrupted programming behavior must be treated as unknown until fault-injection tests are performed safely on sacrificial hardware.
