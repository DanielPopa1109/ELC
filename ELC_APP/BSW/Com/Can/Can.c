#include "Can.h"
#include "../../../Inc/can.h"   /* HAL handle `hcan`; not the BSW Can.h (same name on Windows) */
#include "ComStack_Cfg.h"
#include "../CanIf/CanIf.h"
#include <string.h>

#define CAN_NUM_MAILBOXES   3u
#define CAN_RX_QUEUE_LEN    16u
#define CAN_TX_TIMEOUT_MS   100u

typedef struct
{
    Can_IdType  id;
    PduIdType   swPduHandle;
    uint8       busy;
    uint32      started;
} Can_MailboxType;

typedef struct
{
    Can_IdType id;
    uint8      dlc;
    uint8      data[8];
} Can_RxFrameType;

static Can_MailboxType Can_Mailbox[CAN_NUM_MAILBOXES];
static Can_RxFrameType Can_RxQueue[CAN_RX_QUEUE_LEN];
static volatile uint8 Can_RxHead;
static volatile uint8 Can_RxTail;
static uint8 Can_ControllerWasOnline;

volatile uint32 Can_RxOverflow;
volatile uint32 Can_TxErrors;

static uint8 Can_IsControllerOnline(void)
{
    return (uint8)((HAL_CAN_GetState(&hcan) == HAL_CAN_STATE_LISTENING) &&
                   ((hcan.Instance->ESR & CAN_ESR_BOFF) == 0u));
}

void Can_Init(void)
{
    (void)memset(Can_Mailbox, 0, sizeof(Can_Mailbox));
    Can_RxHead = 0u;
    Can_RxTail = 0u;
    Can_RxOverflow = 0u;
    Can_TxErrors = 0u;
    Can_ControllerWasOnline = 1u;
}

Std_ReturnType Can_GetControllerMode(uint8 Controller, Can_ControllerStateType *ControllerModePtr)
{
    if ((Controller != ELC_CAN_CONTROLLER_ID) || (ControllerModePtr == NULL_PTR))
    {
        return E_NOT_OK;
    }
    *ControllerModePtr = (Can_IsControllerOnline() != 0u) ? CAN_CS_STARTED : CAN_CS_STOPPED;
    return E_OK;
}

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo)
{
    CAN_TxHeaderTypeDef header = {0};
    uint32 mailbox;
    uint32 primask;
    Std_ReturnType rc = CAN_BUSY;
    unsigned i;

    (void)Hth; /* one hardware transmit handle: any free mailbox */
    if ((PduInfo == NULL_PTR) || (PduInfo->sdu == NULL_PTR) || (PduInfo->id > 0x7FFu) ||
        (PduInfo->length > 8u) || (Can_IsControllerOnline() == 0u))
    {
        return E_NOT_OK;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    for (i = 0u; i < CAN_NUM_MAILBOXES; ++i)
    {
        /* Never queue a second frame with the same identifier (keeps ordering),
         * and never reuse a mailbox whose completion has not been processed yet. */
        if ((Can_Mailbox[i].busy != 0u) &&
            ((Can_Mailbox[i].id == PduInfo->id) ||
             (HAL_CAN_IsTxMessagePending(&hcan, 1u << i) == 0u)))
        {
            __set_PRIMASK(primask);
            return CAN_BUSY;
        }
    }

    header.StdId = PduInfo->id;
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = PduInfo->length;
    if (HAL_CAN_AddTxMessage(&hcan, &header, (uint8_t *)PduInfo->sdu, &mailbox) == HAL_OK)
    {
        i = (mailbox == CAN_TX_MAILBOX0) ? 0u : (mailbox == CAN_TX_MAILBOX1) ? 1u : 2u;
        Can_Mailbox[i].id = PduInfo->id;
        Can_Mailbox[i].swPduHandle = PduInfo->swPduHandle;
        Can_Mailbox[i].busy = 1u;
        Can_Mailbox[i].started = HAL_GetTick();
        rc = E_OK;
    }
    __set_PRIMASK(primask);
    return rc;
}

void Can_MainFunction_Write(void)
{
    unsigned i;

    for (i = 0u; i < CAN_NUM_MAILBOXES; ++i)
    {
        uint32 primask;
        uint32 shift;
        int pending;
        int success;

        if (Can_Mailbox[i].busy == 0u)
        {
            continue;
        }
        primask = __get_PRIMASK();
        __disable_irq();
        shift = 8u * i;
        pending = HAL_CAN_IsTxMessagePending(&hcan, 1u << i);
        success = (pending == 0) && ((hcan.Instance->TSR & (CAN_TSR_TXOK0 << shift)) != 0u);
        if ((pending == 0) || (Can_IsControllerOnline() == 0u) ||
            ((uint32)(HAL_GetTick() - Can_Mailbox[i].started) >= CAN_TX_TIMEOUT_MS))
        {
            PduIdType handle = Can_Mailbox[i].swPduHandle;
            if (pending != 0)
            {
                (void)HAL_CAN_AbortTxRequest(&hcan, 1u << i);
            }
            if (HAL_CAN_IsTxMessagePending(&hcan, 1u << i) == 0)
            {
                hcan.Instance->TSR = CAN_TSR_RQCP0 << shift;
                Can_Mailbox[i].busy = 0u;
                __set_PRIMASK(primask);
                if (success != 0)
                {
                    CanIf_TxConfirmation(handle);
                }
                else
                {
                    /* AUTOSAR CanIf has no negative confirmation: upper layers
                     * recover through their own timeouts (N_As, COM TX timeout). */
                    Can_TxErrors++;
                }
                continue;
            }
        }
        __set_PRIMASK(primask);
    }
}

void Can_MainFunction_Read(void)
{
    unsigned n;

    for (n = 0u; (n < CAN_RX_QUEUE_LEN) && (Can_RxTail != Can_RxHead); ++n)
    {
        Can_RxFrameType frame;
        Can_HwType mailbox;
        PduInfoType info;
        uint32 primask = __get_PRIMASK();

        __disable_irq();
        frame = Can_RxQueue[Can_RxTail];
        Can_RxTail = (uint8)((Can_RxTail + 1u) % CAN_RX_QUEUE_LEN);
        __set_PRIMASK(primask);

        mailbox.CanId = frame.id;
        mailbox.Hoh = 0u;
        mailbox.ControllerId = ELC_CAN_CONTROLLER_ID;
        info.SduDataPtr = frame.data;
        info.MetaDataPtr = NULL_PTR;
        info.SduLength = frame.dlc;
        CanIf_RxIndication(&mailbox, &info);
    }
}

void Can_MainFunction_BusOff(void)
{
    uint8 online = Can_IsControllerOnline();

    if (online != Can_ControllerWasOnline)
    {
        Can_ControllerWasOnline = online;
        if (online == 0u)
        {
            CanIf_ControllerBusOff(ELC_CAN_CONTROLLER_ID);
        }
        else
        {
            CanIf_ControllerModeIndication(ELC_CAN_CONTROLLER_ID, CAN_CS_STARTED);
        }
    }
}

void Can_IsrRxIndication(Can_IdType id, uint8 dlc, const uint8 *data)
{
    uint32 primask;
    uint8 next;

    if ((data == NULL_PTR) || (dlc > 8u))
    {
        return;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    next = (uint8)((Can_RxHead + 1u) % CAN_RX_QUEUE_LEN);
    if (next == Can_RxTail)
    {
        Can_RxOverflow++;
    }
    else
    {
        Can_RxQueue[Can_RxHead].id = id;
        Can_RxQueue[Can_RxHead].dlc = dlc;
        (void)memcpy(Can_RxQueue[Can_RxHead].data, data, dlc);
        Can_RxHead = next;
    }
    __set_PRIMASK(primask);
}

static void Can_HalReceive(CAN_HandleTypeDef *handle, uint32_t fifo)
{
    CAN_RxHeaderTypeDef header;
    uint8_t data[8];

    while (HAL_CAN_GetRxFifoFillLevel(handle, fifo) != 0u)
    {
        if (HAL_CAN_GetRxMessage(handle, fifo, &header, data) != HAL_OK)
        {
            break;
        }
        if ((header.IDE == CAN_ID_STD) && (header.RTR == CAN_RTR_DATA) && (header.DLC <= 8u))
        {
            Can_IsrRxIndication((Can_IdType)header.StdId, (uint8)header.DLC, data);
        }
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *handle)
{
    Can_HalReceive(handle, CAN_RX_FIFO0);
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *handle)
{
    Can_HalReceive(handle, CAN_RX_FIFO1);
}
