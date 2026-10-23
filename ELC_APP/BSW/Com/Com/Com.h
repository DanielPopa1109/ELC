#ifndef ELC_BSW_COM_H
#define ELC_BSW_COM_H

#include <stdint.h>

enum { COM_RX_LOADREQUEST = 0u, COM_RX_VEHICLESTATE = 1u };

void Com_Init(void);
void Com_RxIndication(uint8_t rxPduId, const uint8_t *data, uint8_t len);
void Com_MainFunctionRx(void); /* 5 ms */
void Com_MainFunctionTx(void); /* 5 ms */
void Com_SetLoadStatus(uint8_t status, uint8_t txEnabled);
int Com_QueueTelemetry(uint16_t canId, const uint8_t *data, uint8_t len);
int Com_GetLoadRequest(uint8_t *request);
int Com_GetVehicleStatus(uint8_t *status);
uint8_t Com_LoadRequestTimedOut(void);
uint8_t Com_VehicleStatusTimedOut(void);
uint8_t Com_HasInvalidVehicleStatus(void);
uint32_t Com_GetInvalidFrameCount(void);
int Com_QueueFault(const uint8_t *data);
void Com_TxConfirmation(uint16_t id,int result);

#endif
