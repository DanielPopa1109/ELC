/* Com.h - signal layer of the ELC lab stack (ELC subset of the ZGW COM configuration).
 *
 * RX I-PDUs: LoadRequest (0x050, CRC-protected command, 1 s deadline) and
 * VehicleState (0x3A0). TX I-PDUs: LoadStatus (0x051, on change + 500 ms cycle),
 * ten telemetry frames (0x6EE..0x6F7) and the DTC broadcast (0x7FE). Transmission
 * is confirmed only by the physical TX confirmation; a lost confirmation re-arms
 * the PDU after COM_TX_TIMEOUT_TICKS. */
#ifndef ELC_BSW_COM_H
#define ELC_BSW_COM_H

#include "ComStack_Cfg.h"

typedef uint8 Com_SignalIdType;

#define COM_TX_TIMEOUT_TICKS      30u    /* 150 ms: > Can driver mailbox abort (100 ms) */
#define COM_LOADSTATUS_PERIOD     100u   /* 500 ms at 5 ms */
#define COM_RX_DEADLINE_TICKS     200u   /* 1 s at 5 ms */

/* Com_GetRxStatus() bit mask. */
#define COM_RXSTATUS_TIMEOUT      0x01u  /* no valid frame within the deadline (also before the first frame) */
#define COM_RXSTATUS_INVALID      0x02u  /* the latest frame failed validation */

void Com_Init(void);
void Com_MainFunctionRx(void);   /* 5 ms */
void Com_MainFunctionTx(void);   /* 5 ms */

void Com_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void Com_TxConfirmation(PduIdType TxPduId);

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void *SignalDataPtr);
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalDataPtr);
uint8 Com_GetRxStatus(Com_SignalIdType SignalId);
uint32 Com_GetE2eErrorCount(void);   /* consecutive invalid LoadRequest frames */

/* I-PDU group control, driven by ComM (communication state and UDS CommunicationControl). */
void Com_SetRxAllowed(boolean Allowed);
void Com_SetTxAllowed(boolean Allowed);

#endif /* ELC_BSW_COM_H */
