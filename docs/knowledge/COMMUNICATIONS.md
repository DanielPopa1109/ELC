# Current communication architecture

CAN RX ISRs enqueue bounded standard data frames; protocol and XCP execution run in task context. Can owns all application HAL CAN submissions and correlates three hardware mailbox completions using TXOK, errors and timeout/abort. CanIf maps interface/transport routes, normal communication gates and the retained XCP adapter. PduR routes COM and diagnostic payloads and confirmations. CanTp independently reassembles diagnostics; DCM no longer consumes PCI bytes.

See the current [ZGW CAN contract](../modernization/ZGW_ELC_CAN_CONTRACT.md), [module coverage](../autosar/BSW_FUNCTIONAL_COVERAGE.md) and [validation](../modernization/ELC_VALIDATION_RESULTS.md). Wire diagnostics are extended710/58 RX and710/F0 TX/FC. Application IDs/DLCs and signal meanings remain unchanged. Telemetry comes from a coherent SMon publication, and completion versions protect newer queued samples.

NM3 is3FF/DLC1 with retained ELC bit6 policy and200 x5 ms loss timeout. Normal TX is inhibited without a network request or while bus-off. Diagnostic communication remains available. No unsupported NM transmit frame or NM-only MCU standby was introduced. Physical bus timing/FC correlation remains unverified.
