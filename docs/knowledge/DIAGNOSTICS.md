# Current ELC diagnostics

The application now uses `Can -> CanIf -> CanTp -> PduR -> Dcm`, with complete service payloads. DCM contains no CAN PCI handling. See [functional coverage](../autosar/BSW_FUNCTIONAL_COVERAGE.md) and [closure](../modernization/ELC_FUNCTIONAL_GAP_CLOSURE.md) for supported service/DID/RID details and exact validation status.

All target images receive extended-address CAN710/58 and respond on710/F0, matching ZGW's routed channel. Normal702/703 is retired. Application sessions are default/extended with a vehicle-state6 programming gate, S3=5 s, P2=50 ms and P2*=5 s. ClearDTC, reset and programming handover wait for NvM completion; response confirmation precedes reset. Failed Flash programming cannot yield a positive durable clear response.

DTC services use DEM public interfaces. Qualified monitor failures/passes retain identifiers50..61, occurrence and the existing freeze-frame fields. Confirmation is one qualified failure; debounce remains with application monitors; aging is unconfigured. ReadDTCInformation01/02/04 supports masks and records1/FF, and exact orFFFFFF snapshot selection. Availability mask is7F.

APP F101 preserves its legacy reset-count meaning, conflicting with FCD's generic version query. Use F180 for APP software version; F100/F186 alias active block/session. Existing raw-float snapshot encoding is retained. No security-access service was added.

Resident and SRAM programming use two compiled instances of `ELC_FBL/Core/Inc/boot_protocol.h`; hardware Flash ports and execution environments remain distinct. See [current programming contract](../bootloader/PROGRAMMING_PROTOCOL.md). No physical session, reset or Flash programming was executed.
