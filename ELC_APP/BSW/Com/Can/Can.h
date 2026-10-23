#ifndef ELC_BSW_CAN_H
#define ELC_BSW_CAN_H

#include <stdint.h>

/* bxCAN is initialized by MX_CAN_Init. This module owns BSW TX submission. */
int Can_Write(uint16_t canId, const uint8_t *data, uint8_t dlc);
uint8_t Can_IsControllerOnline(void);
void Can_Init(void);
void Can_MainFunction(void);
void Can_QueueRx(uint16_t id, const uint8_t *data, uint8_t len);
void CanH_RxIndication(uint16_t id, const uint8_t *data, uint8_t len);

#endif
