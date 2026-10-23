#ifndef ELC_BSW_CANIF_H
#define ELC_BSW_CANIF_H

#include <stdint.h>

enum { CANIF_TX_LOADSTATUS = 0u, CANIF_RX_LOADREQUEST = 0u,
       CANIF_RX_VEHICLESTATE = 1u };

void CanIf_Init(void);
void CanIf_SetApplicationTxEnabled(uint8_t enabled);
int CanIf_Transmit(uint8_t txPduId, const uint8_t *data, uint8_t len);
int CanIf_TransmitTelemetry(uint16_t canId, const uint8_t *data, uint8_t len);
void CanIf_RxIndication(uint16_t canId, const uint8_t *data, uint8_t len);
int CanIf_TransmitDiagnostic(const uint8_t *data,uint8_t len);
int CanIf_TransmitFault(const uint8_t *data);
int CanIf_TransmitXcp(const uint8_t *data,uint8_t len);
void CanIf_TxConfirmation(uint16_t id,int result);
void CanIf_SetCommunicationControl(uint8_t control,uint8_t communicationType);
void CanIf_MainFunction(void);

#endif
