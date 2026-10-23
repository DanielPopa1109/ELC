#ifndef ELC_BSW_PDUR_H
#define ELC_BSW_PDUR_H

#include <stdint.h>

enum { PDUR_TX_LOADSTATUS = 0u, PDUR_RX_LOADREQUEST = 0u,
       PDUR_RX_VEHICLESTATE = 1u };

void PduR_Init(void);
int PduR_ComTransmit(uint8_t txPduId, const uint8_t *data, uint8_t len);
int PduR_ComTransmitTelemetry(uint16_t canId, const uint8_t *data, uint8_t len);
void PduR_CanIfRxIndication(uint8_t rxPduId, const uint8_t *data, uint8_t len);
int PduR_DcmTransmit(const uint8_t *data,uint16_t len);
void PduR_CanTpRxIndication(const uint8_t *data,uint16_t len,int result);
void PduR_CanTpTxConfirmation(int result);
void PduR_CanIfTxConfirmation(uint16_t id,int result);
int PduR_ComTransmitFault(const uint8_t *data);

#endif
