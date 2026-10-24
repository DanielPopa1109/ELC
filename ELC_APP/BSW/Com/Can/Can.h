/* Can.h - bxCAN driver (CAN1) for the ELC lab stack.
 *
 * AUTOSAR Can driver shape, polling mode: Can_Write() hands a PDU to one of
 * three TX mailboxes, Can_MainFunction_Write() confirms completed mailboxes to
 * CanIf, Can_MainFunction_Read() drains the RX queue filled by the HAL RX
 * interrupt, Can_MainFunction_BusOff() reports controller state changes.
 * The bxCAN peripheral itself (bit timing, filters, ABOM) is configured by
 * MX_CAN_Init; this module owns every HAL_CAN_AddTxMessage call. */
#ifndef ELC_BSW_CAN_H
#define ELC_BSW_CAN_H

#include "ComStack_Types.h"

#define CAN_BUSY 2u   /* Can_Write(): no mailbox available now, retry later */

typedef uint16 Can_IdType;
typedef uint8  Can_HwHandleType;

typedef struct
{
    PduIdType   swPduHandle;  /* returned to CanIf_TxConfirmation() */
    uint8       length;
    Can_IdType  id;
    const uint8 *sdu;
} Can_PduType;

typedef struct
{
    Can_IdType       CanId;
    Can_HwHandleType Hoh;
    uint8            ControllerId;
} Can_HwType;

typedef enum
{
    CAN_CS_UNINIT = 0u,
    CAN_CS_STOPPED,        /* bus-off / not listening */
    CAN_CS_STARTED,
    CAN_CS_SLEEP
} Can_ControllerStateType;

void Can_Init(void);
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);
Std_ReturnType Can_GetControllerMode(uint8 Controller, Can_ControllerStateType *ControllerModePtr);

void Can_MainFunction_Write(void);   /* TX confirmation polling, 1 ms */
void Can_MainFunction_Read(void);    /* RX queue -> CanIf, 1 ms */
void Can_MainFunction_BusOff(void);  /* controller state edges -> CanIf, 1 ms */

/* Called from the HAL RX interrupt only. */
void Can_IsrRxIndication(Can_IdType id, uint8 dlc, const uint8 *data);

extern volatile uint32 Can_RxOverflow;
extern volatile uint32 Can_TxErrors;

#endif /* ELC_BSW_CAN_H */
