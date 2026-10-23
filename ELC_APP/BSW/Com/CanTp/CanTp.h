#ifndef ELC_CANTP_H
#define ELC_CANTP_H
#include <stdint.h>
#define CANTP_RX_ID 0x710u
#define CANTP_TX_ID 0x710u
#define CANTP_RX_ADDRESS 0x58u
#define CANTP_TX_ADDRESS 0xF0u
#define CANTP_RX_MAX 256u
void CanTp_Init(void);
void CanTp_MainFunction(void);
void CanTp_RxIndication(const uint8_t *data,uint8_t len);
int CanTp_Transmit(const uint8_t *data,uint16_t len);
void CanTp_TxConfirmation(int result);
void CanTp_Cancel(void);
int CanTp_IsIdle(void);
#endif
