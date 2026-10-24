#include "PduR.h"
#include "../CanIf/CanIf.h"
#include "../CanTp/CanTp.h"
#include "../Com/Com.h"
#include "../../Diag/Dcm/Dcm.h"

static const PduR_ConfigType *PduR_ConfigPtr;

/* Returns the destination of `srcPduId`, or 0xFF when the PDU is not routed. */
static PduIdType PduR_Route(const PduR_RoutingPathType *paths, uint8 count, PduIdType srcPduId)
{
    uint8 i;

    for (i = 0u; i < count; ++i)
    {
        if (paths[i].srcPduId == srcPduId)
        {
            return paths[i].destPduId;
        }
    }
    return 0xFFu;
}

void PduR_Init(const PduR_ConfigType *ConfigPtr)
{
    PduR_ConfigPtr = (ConfigPtr != NULL_PTR) ? ConfigPtr : &PduR_Config;
}

Std_ReturnType PduR_ComTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
    PduIdType dest;

    if (PduR_ConfigPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }
    dest = PduR_Route(PduR_ConfigPtr->ifTx, PduR_ConfigPtr->numIfTx, TxPduId);
    return (dest != 0xFFu) ? CanIf_Transmit(dest, PduInfoPtr) : E_NOT_OK;
}

Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
    PduIdType dest;

    if (PduR_ConfigPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }
    dest = PduR_Route(PduR_ConfigPtr->tpTx, PduR_ConfigPtr->numTpTx, TxPduId);
    return (dest != 0xFFu) ? CanTp_Transmit(dest, PduInfoPtr) : E_NOT_OK;
}

void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    PduIdType dest;

    if (PduR_ConfigPtr == NULL_PTR)
    {
        return;
    }
    dest = PduR_Route(PduR_ConfigPtr->ifRx, PduR_ConfigPtr->numIfRx, RxPduId);
    if (dest != 0xFFu)
    {
        Com_RxIndication(dest, PduInfoPtr);
    }
}

void PduR_CanIfTxConfirmation(PduIdType TxPduId)
{
    uint8 i;

    if (PduR_ConfigPtr == NULL_PTR)
    {
        return;
    }
    for (i = 0u; i < PduR_ConfigPtr->numIfTx; ++i)
    {
        if (PduR_ConfigPtr->ifTx[i].destPduId == TxPduId)
        {
            Com_TxConfirmation(PduR_ConfigPtr->ifTx[i].srcPduId);
            return;
        }
    }
}

BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType id, const PduInfoType *info,
                                             PduLengthType TpSduLength, PduLengthType *bufferSizePtr)
{
    PduIdType dest;

    if (PduR_ConfigPtr == NULL_PTR)
    {
        return BUFREQ_E_NOT_OK;
    }
    dest = PduR_Route(PduR_ConfigPtr->tpRx, PduR_ConfigPtr->numTpRx, id);
    return (dest != 0xFFu) ? Dcm_StartOfReception(dest, info, TpSduLength, bufferSizePtr) : BUFREQ_E_NOT_OK;
}

BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType id, const PduInfoType *info, PduLengthType *bufferSizePtr)
{
    PduIdType dest;

    if (PduR_ConfigPtr == NULL_PTR)
    {
        return BUFREQ_E_NOT_OK;
    }
    dest = PduR_Route(PduR_ConfigPtr->tpRx, PduR_ConfigPtr->numTpRx, id);
    return (dest != 0xFFu) ? Dcm_CopyRxData(dest, info, bufferSizePtr) : BUFREQ_E_NOT_OK;
}

void PduR_CanTpRxIndication(PduIdType id, Std_ReturnType result)
{
    PduIdType dest;

    if (PduR_ConfigPtr == NULL_PTR)
    {
        return;
    }
    dest = PduR_Route(PduR_ConfigPtr->tpRx, PduR_ConfigPtr->numTpRx, id);
    if (dest != 0xFFu)
    {
        Dcm_TpRxIndication(dest, result);
    }
}

BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                       PduLengthType *availableDataPtr)
{
    uint8 i;

    if (PduR_ConfigPtr == NULL_PTR)
    {
        return BUFREQ_E_NOT_OK;
    }
    for (i = 0u; i < PduR_ConfigPtr->numTpTx; ++i)
    {
        if (PduR_ConfigPtr->tpTx[i].destPduId == id)
        {
            return Dcm_CopyTxData(PduR_ConfigPtr->tpTx[i].srcPduId, info, retry, availableDataPtr);
        }
    }
    return BUFREQ_E_NOT_OK;
}

void PduR_CanTpTxConfirmation(PduIdType id, Std_ReturnType result)
{
    uint8 i;

    if (PduR_ConfigPtr == NULL_PTR)
    {
        return;
    }
    for (i = 0u; i < PduR_ConfigPtr->numTpTx; ++i)
    {
        if (PduR_ConfigPtr->tpTx[i].destPduId == id)
        {
            Dcm_TpTxConfirmation(PduR_ConfigPtr->tpTx[i].srcPduId, result);
            return;
        }
    }
}
