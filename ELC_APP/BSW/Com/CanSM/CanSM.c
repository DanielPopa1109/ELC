#include "CanSM.h"
#include "../CanIf/CanIf.h"
#include "../ComM/ComM.h"
#include "main.h"

static ComM_ModeType CanSM_Requested;
static ComM_ModeType CanSM_Current;
static uint8 CanSM_ControllerOnline;

static void CanSM_Evaluate(void)
{
    ComM_ModeType available;
    uint32 primask = __get_PRIMASK();

    __disable_irq();
    available = (CanSM_ControllerOnline != 0u) ? CanSM_Requested : COMM_SILENT_COMMUNICATION;
    if (available == COMM_NO_COMMUNICATION)
    {
        available = COMM_SILENT_COMMUNICATION;   /* no bus sleep on this ECU */
    }
    if (available != CanSM_Current)
    {
        CanSM_Current = available;
        (void)CanIf_SetPduMode(ELC_CAN_CONTROLLER_ID,
                               (available == COMM_FULL_COMMUNICATION) ? CANIF_SET_ONLINE : CANIF_SET_TX_OFFLINE);
        __set_PRIMASK(primask);
        ComM_BusSM_ModeIndication(COMM_CHANNEL_CAN, available);
        return;
    }
    __set_PRIMASK(primask);
}

void CanSM_Init(void)
{
    Can_ControllerStateType mode;

    CanSM_Requested = COMM_SILENT_COMMUNICATION;
    CanSM_Current = COMM_SILENT_COMMUNICATION;
    CanSM_ControllerOnline = (uint8)((CanIf_GetControllerMode(ELC_CAN_CONTROLLER_ID, &mode) != E_OK) ||
                                     (mode == CAN_CS_STARTED));
    (void)CanIf_SetPduMode(ELC_CAN_CONTROLLER_ID, CANIF_SET_TX_OFFLINE);
}

Std_ReturnType CanSM_RequestComMode(NetworkHandleType Network, ComM_ModeType ComM_Mode)
{
    if ((Network != ELC_CAN_NETWORK_ID) || (ComM_Mode > COMM_FULL_COMMUNICATION))
    {
        return E_NOT_OK;
    }
    CanSM_Requested = ComM_Mode;
    CanSM_Evaluate();
    return E_OK;
}

Std_ReturnType CanSM_GetCurrentComMode(NetworkHandleType Network, ComM_ModeType *ComM_ModePtr)
{
    if ((Network != ELC_CAN_NETWORK_ID) || (ComM_ModePtr == NULL_PTR))
    {
        return E_NOT_OK;
    }
    *ComM_ModePtr = CanSM_Current;
    return E_OK;
}

void CanSM_ControllerBusOff(uint8 ControllerId)
{
    if (ControllerId == ELC_CAN_CONTROLLER_ID)
    {
        CanSM_ControllerOnline = 0u;
        CanSM_Evaluate();
    }
}

void CanSM_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode)
{
    if (ControllerId == ELC_CAN_CONTROLLER_ID)
    {
        CanSM_ControllerOnline = (uint8)(ControllerMode == CAN_CS_STARTED);
        CanSM_Evaluate();
    }
}

void CanSM_MainFunction(void)
{
    CanSM_Evaluate();
}
