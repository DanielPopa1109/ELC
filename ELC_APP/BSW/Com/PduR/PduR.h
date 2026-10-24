/* PduR.h - PDU Router for the ELC lab stack.
 *
 * Static routing paths (PduR_Cfg.c):
 *   IF  : Com  <-> CanIf   (LoadRequest/VehicleState/LoadStatus/telemetry/DTC broadcast)
 *   TP  : Dcm  <-> CanTp   (UDS requests and responses, TP buffer callbacks)
 * Every other CanIf user (CanNm, CanTp N-PDUs, XCP) is addressed by CanIf directly. */
#ifndef ELC_BSW_PDUR_H
#define ELC_BSW_PDUR_H

#include "ComStack_Cfg.h"

typedef struct
{
    PduIdType srcPduId;
    PduIdType destPduId;
} PduR_RoutingPathType;

typedef struct
{
    const PduR_RoutingPathType *ifTx;   uint8 numIfTx;   /* Com TX I-PDU    -> CanIf TX PDU  */
    const PduR_RoutingPathType *ifRx;   uint8 numIfRx;   /* CanIf RX PDU    -> Com RX I-PDU  */
    const PduR_RoutingPathType *tpTx;   uint8 numTpTx;   /* Dcm TX PDU      -> CanTp TX SDU  */
    const PduR_RoutingPathType *tpRx;   uint8 numTpRx;   /* CanTp RX SDU    -> Dcm RX PDU    */
} PduR_ConfigType;

extern const PduR_ConfigType PduR_Config;

void PduR_Init(const PduR_ConfigType *ConfigPtr);

/* Upper layer interface (called by Com / Dcm). */
Std_ReturnType PduR_ComTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/* Lower layer interface, IF (called by CanIf). */
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void PduR_CanIfTxConfirmation(PduIdType TxPduId);

/* Lower layer interface, TP (called by CanTp). */
BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType id, const PduInfoType *info,
                                             PduLengthType TpSduLength, PduLengthType *bufferSizePtr);
BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType id, const PduInfoType *info, PduLengthType *bufferSizePtr);
void PduR_CanTpRxIndication(PduIdType id, Std_ReturnType result);
BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                       PduLengthType *availableDataPtr);
void PduR_CanTpTxConfirmation(PduIdType id, Std_ReturnType result);

#endif /* ELC_BSW_PDUR_H */
