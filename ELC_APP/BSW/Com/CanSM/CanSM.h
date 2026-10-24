/* CanSM.h - CAN State Manager (single channel, single controller).
 *
 * Grants COMM_FULL_COMMUNICATION only while the controller is online (not bus-off).
 * Full communication sets the CanIf PDU mode online; every other state keeps the
 * controller started and RX online but takes application PDUs TX offline
 * (hardware ABOM recovers bus-off). Diagnostic PDUs are unaffected. */
#ifndef ELC_BSW_CANSM_H
#define ELC_BSW_CANSM_H

#include "../ComM/ComM_Types.h"
#include "../Can/Can.h"

void CanSM_Init(void);
void CanSM_MainFunction(void);

Std_ReturnType CanSM_RequestComMode(NetworkHandleType Network, ComM_ModeType ComM_Mode);
Std_ReturnType CanSM_GetCurrentComMode(NetworkHandleType Network, ComM_ModeType *ComM_ModePtr);

/* Notifications from CanIf. */
void CanSM_ControllerBusOff(uint8 ControllerId);
void CanSM_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode);

#endif /* ELC_BSW_CANSM_H */
