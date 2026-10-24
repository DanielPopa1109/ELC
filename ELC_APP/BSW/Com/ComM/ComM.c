#include "ComM.h"
#include "../CanNm/CanNm.h"
#include "../CanSM/CanSM.h"
#include "../Com/Com.h"

static ComM_ModeType ComM_RequestedMode;   /* from the network (CanNm) */
static ComM_ModeType ComM_CurrentMode;     /* as granted by CanSM */
static uint8 ComM_RxDisabled;              /* UDS 0x28: normal communication */
static uint8 ComM_TxDisabled;
static uint8 ComM_NmRxDisabled;

static void ComM_ApplyGates(void)
{
    Com_SetRxAllowed((ComM_RxDisabled == 0u) ? TRUE : FALSE);
    Com_SetTxAllowed(((ComM_CurrentMode == COMM_FULL_COMMUNICATION) && (ComM_TxDisabled == 0u)) ? TRUE : FALSE);
    CanNm_EnableRx((ComM_NmRxDisabled == 0u) ? TRUE : FALSE);
}

void ComM_Init(void)
{
    ComM_RequestedMode = COMM_SILENT_COMMUNICATION;
    ComM_CurrentMode = COMM_SILENT_COMMUNICATION;
    ComM_RxDisabled = 0u;
    ComM_TxDisabled = 0u;
    ComM_NmRxDisabled = 0u;
    (void)CanSM_RequestComMode(COMM_CHANNEL_CAN, COMM_SILENT_COMMUNICATION);
    ComM_ApplyGates();
}

void ComM_MainFunction(void)
{
    (void)CanSM_RequestComMode(COMM_CHANNEL_CAN, ComM_RequestedMode);
    ComM_ApplyGates();
}

Std_ReturnType ComM_GetCurrentComMode(NetworkHandleType Channel, ComM_ModeType *ComMode)
{
    if ((Channel != COMM_CHANNEL_CAN) || (ComMode == NULL_PTR))
    {
        return E_NOT_OK;
    }
    *ComMode = ComM_CurrentMode;
    return E_OK;
}

uint8 ComM_IsTxAllowed(NetworkHandleType Channel)
{
    return (uint8)((Channel == COMM_CHANNEL_CAN) && (ComM_CurrentMode == COMM_FULL_COMMUNICATION) &&
                   (ComM_TxDisabled == 0u));
}

uint8 ComM_IsRxAllowed(NetworkHandleType Channel)
{
    return (uint8)((Channel == COMM_CHANNEL_CAN) && (ComM_RxDisabled == 0u));
}

Std_ReturnType ComM_CommunicationControl(NetworkHandleType Channel, uint8 ControlType, uint8 CommunicationType)
{
    if ((Channel != COMM_CHANNEL_CAN) || (ControlType > 3u) || (CommunicationType < 1u) || (CommunicationType > 3u))
    {
        return E_NOT_OK;
    }
    if ((CommunicationType & 1u) != 0u)
    {
        ComM_RxDisabled = (uint8)(ControlType >= 2u);
        ComM_TxDisabled = (uint8)(ControlType & 1u);
    }
    if ((CommunicationType & 2u) != 0u)
    {
        ComM_NmRxDisabled = (uint8)(ControlType >= 2u);
    }
    ComM_ApplyGates();
    return E_OK;
}

void ComM_BusSM_ModeIndication(NetworkHandleType Channel, ComM_ModeType ComMode)
{
    if (Channel == COMM_CHANNEL_CAN)
    {
        ComM_CurrentMode = ComMode;
        ComM_ApplyGates();
    }
}

void ComM_Nm_NetworkMode(NetworkHandleType Channel)
{
    if (Channel == COMM_CHANNEL_CAN)
    {
        ComM_RequestedMode = COMM_FULL_COMMUNICATION;
    }
}

void ComM_Nm_BusSleepMode(NetworkHandleType Channel)
{
    if (Channel == COMM_CHANNEL_CAN)
    {
        ComM_RequestedMode = COMM_SILENT_COMMUNICATION;
    }
}
