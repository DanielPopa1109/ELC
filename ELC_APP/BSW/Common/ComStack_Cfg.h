/* ComStack_Cfg.h - static PDU configuration shared by the ELC communication stack.
 *
 * One CAN controller (bxCAN, 500 kbit/s), classic CAN, 11-bit identifiers.
 * Symbolic PDU ids replace the raw CAN identifiers that the previous ELC
 * modules passed between layers. The CAN identifiers themselves live in the
 * CanIf configuration (CanIf_Cfg.c) and are the contract with ZGW; see
 * docs/modernization/ZGW_ELC_CAN_CONTRACT.md. */
#ifndef ELC_COMSTACK_CFG_H
#define ELC_COMSTACK_CFG_H

#include "ComStack_Types.h"

/* ---- Controller / network ------------------------------------------------ */
#define ELC_CAN_CONTROLLER_ID        0u
#define ELC_CAN_NETWORK_ID           0u

/* ---- CanIf RX PDUs (CAN identifier -> upper layer) ------------------------ */
#define CANIF_RXPDU_DIAG_REQ         0u   /* 0x710, extended addressing N_TA 0x58 -> CanTp */
#define CANIF_RXPDU_LOADREQUEST      1u   /* 0x050 DLC 2 -> PduR -> Com */
#define CANIF_RXPDU_VEHICLESTATE     2u   /* 0x3A0 DLC 1 -> PduR -> Com */
#define CANIF_RXPDU_NM3              3u   /* 0x3FF DLC 1 -> CanNm */
#define CANIF_RXPDU_XCP_CMD          4u   /* 0x7C0 -> XCP slave (CanH) */
#define CANIF_RXPDU_TIMEDATE         5u   /* 0x202 -> application time stamp (CanH) */
#define CANIF_NUM_RX_PDUS            6u

/* ---- CanIf TX PDUs (upper layer -> CAN identifier) ------------------------ */
#define CANIF_TXPDU_DIAG_RESP        0u   /* 0x710, extended addressing N_TA 0xF0 <- CanTp */
#define CANIF_TXPDU_LOADSTATUS       1u   /* 0x051 DLC 1 <- Com */
#define CANIF_TXPDU_TELEMETRY_FIRST  2u   /* 0x6EE .. 0x6F7 (10 PDUs) <- Com */
#define CANIF_TXPDU_DTC_BROADCAST    12u  /* 0x7FE DLC 4 <- Com */
#define CANIF_TXPDU_XCP_RES          13u  /* 0x7C1 <- XCP slave (CanH) */
#define CANIF_NUM_TX_PDUS            14u

/* ---- CanTp N-PDU / N-SDU ids ---------------------------------------------- */
#define CANTP_RXNPDU_DIAG            0u   /* CanIf upper id of CANIF_RXPDU_DIAG_REQ */
#define CANTP_RXNSDU_DIAG            0u   /* PduR/Dcm side */
#define CANTP_TXNSDU_DIAG            0u
#define CANTP_TXNPDU_DIAG            0u   /* CanIf TX PDU id of CANIF_TXPDU_DIAG_RESP */

/* ---- Dcm PDU ids ------------------------------------------------------------ */
#define DCM_RXPDU_DIAG               0u
#define DCM_TXPDU_DIAG               0u

/* ---- Com I-PDU / signal ids ------------------------------------------------- */
#define COM_RXPDU_LOADREQUEST        0u
#define COM_RXPDU_VEHICLESTATE       1u
#define COM_NUM_RX_PDUS              2u

#define COM_TXPDU_LOADSTATUS         0u
#define COM_TXPDU_TELEMETRY_FIRST    1u   /* 0x6EE .. 0x6F7 */
#define COM_TXPDU_TELEMETRY_COUNT    10u
#define COM_TXPDU_DTC_BROADCAST      11u
#define COM_NUM_TX_PDUS              12u

/* One signal per I-PDU: the signal id equals the I-PDU id of its direction. */
#define COM_SIGNAL_LOADREQUEST       COM_RXPDU_LOADREQUEST
#define COM_SIGNAL_VEHICLESTATE      COM_RXPDU_VEHICLESTATE
#define COM_SIGNAL_LOADSTATUS        COM_TXPDU_LOADSTATUS
#define COM_SIGNAL_TELEMETRY_FIRST   COM_TXPDU_TELEMETRY_FIRST
#define COM_SIGNAL_DTC_BROADCAST     COM_TXPDU_DTC_BROADCAST

#endif /* ELC_COMSTACK_CFG_H */
