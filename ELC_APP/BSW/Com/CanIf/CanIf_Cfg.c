/* CanIf_Cfg.c - static CanIf configuration (ZGW <-> ELC classic CAN contract).
 * Identifiers and DLCs: docs/modernization/ZGW_ELC_CAN_CONTRACT.md. */
#include "CanIf.h"

/* Index == CANIF_RXPDU_* id. */
static const CanIf_RxPduConfigType CanIf_RxPdus[CANIF_NUM_RX_PDUS] =
{
    /* canId  minDlc flags                   upper               upperPduId */
    { 0x710u, 2u,    CANIF_PDU_ALWAYS_ONLINE, CANIF_UPPER_CANTP, CANTP_RXNPDU_DIAG },        /* diagnostic request (N_TA 0x58) */
    { 0x050u, 0u,    0u,                      CANIF_UPPER_PDUR,  CANIF_RXPDU_LOADREQUEST },  /* COM validates DLC 2 (counts as E2E error) */
    { 0x3A0u, 0u,    0u,                      CANIF_UPPER_PDUR,  CANIF_RXPDU_VEHICLESTATE }, /* COM validates DLC 1 */
    { 0x3FFu, 0u,    0u,                      CANIF_UPPER_CANNM, 0u },                       /* NM3, CanNm validates DLC 1 */
    { 0x7C0u, 0u,    0u,                      CANIF_UPPER_APPL,  CANIF_RXPDU_XCP_CMD },
    { 0x202u, 0u,    0u,                      CANIF_UPPER_APPL,  CANIF_RXPDU_TIMEDATE }
};

/* Index == CANIF_TXPDU_* id. For PduR destinations the upper id is the CanIf TX PDU id itself. */
static const CanIf_TxPduConfigType CanIf_TxPdus[CANIF_NUM_TX_PDUS] =
{
    /* canId  dlc flags                   upper               upperPduId */
    { 0x710u, 8u, CANIF_PDU_ALWAYS_ONLINE, CANIF_UPPER_CANTP, CANTP_TXNPDU_DIAG },        /* diagnostic response (N_TA 0xF0) */
    { 0x051u, 1u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_LOADSTATUS },
    { 0x6EEu, 8u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_TELEMETRY_FIRST + 0u },
    { 0x6EFu, 8u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_TELEMETRY_FIRST + 1u },
    { 0x6F0u, 4u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_TELEMETRY_FIRST + 2u },
    { 0x6F1u, 8u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_TELEMETRY_FIRST + 3u },
    { 0x6F2u, 8u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_TELEMETRY_FIRST + 4u },
    { 0x6F3u, 8u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_TELEMETRY_FIRST + 5u },
    { 0x6F4u, 8u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_TELEMETRY_FIRST + 6u },
    { 0x6F5u, 8u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_TELEMETRY_FIRST + 7u },
    { 0x6F6u, 8u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_TELEMETRY_FIRST + 8u },
    { 0x6F7u, 1u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_TELEMETRY_FIRST + 9u },
    { 0x7FEu, 4u, 0u,                      CANIF_UPPER_PDUR,  CANIF_TXPDU_DTC_BROADCAST },
    { 0x7C1u, 8u, CANIF_PDU_VARIABLE_DLC,  CANIF_UPPER_APPL,  CANIF_TXPDU_XCP_RES }       /* XCP CTO/DTO, DLC 1..8 */
};

const CanIf_ConfigType CanIf_Config =
{
    CanIf_RxPdus, CANIF_NUM_RX_PDUS,
    CanIf_TxPdus, CANIF_NUM_TX_PDUS
};
