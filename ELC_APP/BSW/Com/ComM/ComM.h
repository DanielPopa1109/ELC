/* ComM.h - Communication Manager (single CAN channel).
 *
 * Full communication follows the remote network request seen by CanNm and is
 * granted by CanSM while the controller is online. ComM also applies UDS
 * CommunicationControl (0x28) to the application I-PDU group (Com) and to NM
 * reception, which is what a BswM rule does in a full AUTOSAR stack. */
#ifndef ELC_BSW_COMM_H
#define ELC_BSW_COMM_H

#include "ComM_Types.h"

#define COMM_CHANNEL_CAN 0u

void ComM_Init(void);
void ComM_MainFunction(void);

Std_ReturnType ComM_GetCurrentComMode(NetworkHandleType Channel, ComM_ModeType *ComMode);
uint8 ComM_IsTxAllowed(NetworkHandleType Channel);
uint8 ComM_IsRxAllowed(NetworkHandleType Channel);

/* UDS 0x28: controlType 0..3 (enable/disable RX/TX), communicationType 1 normal, 2 NM, 3 both. */
Std_ReturnType ComM_CommunicationControl(NetworkHandleType Channel, uint8 ControlType, uint8 CommunicationType);

/* Notifications from CanSM and CanNm. */
void ComM_BusSM_ModeIndication(NetworkHandleType Channel, ComM_ModeType ComMode);
void ComM_Nm_NetworkMode(NetworkHandleType Channel);
void ComM_Nm_BusSleepMode(NetworkHandleType Channel);

#endif /* ELC_BSW_COMM_H */
