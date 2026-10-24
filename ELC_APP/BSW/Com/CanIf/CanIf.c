#include "CanIf.h"
#include "../PduR/PduR.h"
#include "../CanTp/CanTp.h"
#include "../CanNm/CanNm.h"
#include "../CanSM/CanSM.h"

/* Application (non-AUTOSAR-module) users of CanIf: XCP slave and time stamp in CanH. */
extern void CanH_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
extern void CanH_TxConfirmation(PduIdType TxPduId);

#define CANIF_MODE_RX 0x01u
#define CANIF_MODE_TX 0x02u

static const CanIf_ConfigType *CanIf_ConfigPtr;
static uint8 CanIf_Mode = CANIF_MODE_RX | CANIF_MODE_TX;

void CanIf_Init(const CanIf_ConfigType *ConfigPtr)
{
    CanIf_ConfigPtr = (ConfigPtr != NULL_PTR) ? ConfigPtr : &CanIf_Config;
    CanIf_Mode = CANIF_MODE_RX | CANIF_MODE_TX;
}

Std_ReturnType CanIf_SetPduMode(uint8 ControllerId, CanIf_PduSetModeType PduModeRequest)
{
    if (ControllerId != ELC_CAN_CONTROLLER_ID)
    {
        return E_NOT_OK;
    }
    switch (PduModeRequest)
    {
    case CANIF_SET_OFFLINE:    CanIf_Mode = 0u; break;
    case CANIF_SET_RX_OFFLINE: CanIf_Mode &= (uint8)~CANIF_MODE_RX; break;
    case CANIF_SET_RX_ONLINE:  CanIf_Mode |= CANIF_MODE_RX; break;
    case CANIF_SET_TX_OFFLINE: CanIf_Mode &= (uint8)~CANIF_MODE_TX; break;
    case CANIF_SET_TX_ONLINE:  CanIf_Mode |= CANIF_MODE_TX; break;
    case CANIF_SET_ONLINE:     CanIf_Mode = CANIF_MODE_RX | CANIF_MODE_TX; break;
    default:                   return E_NOT_OK;
    }
    return E_OK;
}

Std_ReturnType CanIf_GetPduMode(uint8 ControllerId, CanIf_PduModeType *PduModePtr)
{
    if ((ControllerId != ELC_CAN_CONTROLLER_ID) || (PduModePtr == NULL_PTR))
    {
        return E_NOT_OK;
    }
    *PduModePtr = ((CanIf_Mode & CANIF_MODE_TX) != 0u) ? CANIF_ONLINE : CANIF_TX_OFFLINE;
    if ((CanIf_Mode & CANIF_MODE_RX) == 0u)
    {
        *PduModePtr = CANIF_OFFLINE;
    }
    return E_OK;
}

Std_ReturnType CanIf_GetControllerMode(uint8 ControllerId, Can_ControllerStateType *ControllerModePtr)
{
    return Can_GetControllerMode(ControllerId, ControllerModePtr);
}

Std_ReturnType CanIf_Transmit(PduIdType CanIfTxSduId, const PduInfoType *PduInfoPtr)
{
    const CanIf_TxPduConfigType *pdu;
    Can_PduType canPdu;

    if ((CanIf_ConfigPtr == NULL_PTR) || (PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR) ||
        (CanIfTxSduId >= CanIf_ConfigPtr->numTxPdus))
    {
        return E_NOT_OK;
    }
    pdu = &CanIf_ConfigPtr->txPdus[CanIfTxSduId];
    if ((((pdu->flags & CANIF_PDU_VARIABLE_DLC) != 0u) ?
         ((PduInfoPtr->SduLength == 0u) || (PduInfoPtr->SduLength > pdu->dlc)) :
         (PduInfoPtr->SduLength != pdu->dlc)) ||
        (((pdu->flags & CANIF_PDU_ALWAYS_ONLINE) == 0u) && ((CanIf_Mode & CANIF_MODE_TX) == 0u)))
    {
        return E_NOT_OK;
    }
    canPdu.swPduHandle = CanIfTxSduId;
    canPdu.length = (uint8)PduInfoPtr->SduLength;
    canPdu.id = pdu->canId;
    canPdu.sdu = PduInfoPtr->SduDataPtr;
    return (Can_Write(0u, &canPdu) == E_OK) ? E_OK : E_NOT_OK;
}

void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr)
{
    uint8 i;

    if ((CanIf_ConfigPtr == NULL_PTR) || (Mailbox == NULL_PTR) || (PduInfoPtr == NULL_PTR))
    {
        return;
    }
    for (i = 0u; i < CanIf_ConfigPtr->numRxPdus; ++i)
    {
        const CanIf_RxPduConfigType *pdu = &CanIf_ConfigPtr->rxPdus[i];

        if (pdu->canId != Mailbox->CanId)
        {
            continue;
        }
        if ((PduInfoPtr->SduLength < pdu->minDlc) ||
            (((pdu->flags & CANIF_PDU_ALWAYS_ONLINE) == 0u) && ((CanIf_Mode & CANIF_MODE_RX) == 0u)))
        {
            return;
        }
        switch (pdu->upper)
        {
        case CANIF_UPPER_CANTP: CanTp_RxIndication(pdu->upperPduId, PduInfoPtr); break;
        case CANIF_UPPER_PDUR:  PduR_CanIfRxIndication(pdu->upperPduId, PduInfoPtr); break;
        case CANIF_UPPER_CANNM: CanNm_RxIndication(pdu->upperPduId, PduInfoPtr); break;
        default:                CanH_RxIndication(pdu->upperPduId, PduInfoPtr); break;
        }
        return;
    }
}

void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    const CanIf_TxPduConfigType *pdu;

    if ((CanIf_ConfigPtr == NULL_PTR) || (CanTxPduId >= CanIf_ConfigPtr->numTxPdus))
    {
        return;
    }
    pdu = &CanIf_ConfigPtr->txPdus[CanTxPduId];
    switch (pdu->upper)
    {
    case CANIF_UPPER_CANTP: CanTp_TxConfirmation(pdu->upperPduId); break;
    case CANIF_UPPER_PDUR:  PduR_CanIfTxConfirmation(pdu->upperPduId); break;
    case CANIF_UPPER_CANNM: break;
    default:                CanH_TxConfirmation(pdu->upperPduId); break;
    }
}

void CanIf_ControllerBusOff(uint8 ControllerId)
{
    CanSM_ControllerBusOff(ControllerId);
}

void CanIf_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    CanSM_ControllerModeIndication(ControllerId, ControllerMode);
}
