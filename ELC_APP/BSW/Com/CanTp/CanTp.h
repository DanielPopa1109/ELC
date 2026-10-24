/* CanTp.h - ISO 15765-2 (ISO-TP) transport layer for the ELC lab stack.
 *
 * AUTOSAR CanTp shape: upper layers (via PduR) use CanTp_Transmit(); received
 * and transmitted payload is exchanged with the upper layer through the TP
 * buffer callbacks (StartOfReception / CopyRxData / RxIndication / CopyTxData /
 * TxConfirmation), so CanTp keeps no message-sized buffer of its own.
 * Classic CAN, extended addressing (one address byte per frame), one channel,
 * half-duplex, called every CANTP_MAIN_PERIOD_MS. */
#ifndef ELC_BSW_CANTP_H
#define ELC_BSW_CANTP_H

#include "ComStack_Cfg.h"

#define CANTP_MAIN_PERIOD_MS   5u
#define CANTP_MS_TO_TICKS(ms)  ((uint16)(((ms) + CANTP_MAIN_PERIOD_MS - 1u) / CANTP_MAIN_PERIOD_MS))
#define CANTP_MAX_SDU_LEN      4095u    /* 12-bit FF_DL */

typedef struct
{
    PduIdType rxNPduId;      /* received N-PDU (id given by CanIf) */
    PduIdType rxNSduId;      /* N-SDU id towards PduR/Dcm */
    PduIdType txNSduId;
    PduIdType txNPduId;      /* CanIf TX PDU id */
    uint8     rxAddress;     /* N_TA expected in received SF/FF/CF (this ECU) */
    uint8     fcAddress;     /* additional N_TA accepted in received FC: ZGW addresses its flow control with
                              * its configured tester address, not with the ECU address */
    uint8     txAddress;     /* N_TA placed in transmitted frames (gateway) */
    uint8     blockSize;     /* BS advertised in FC.CTS */
    uint8     stMin;         /* STmin advertised in FC.CTS (ms) */
    uint8     paddingByte;   /* unused data bytes of every frame */
    uint8     wftMax;        /* tolerated consecutive FC.WAIT from the peer */
    uint16    nAsTicks;      /* own frame not confirmed */
    uint16    nArTicks;
    uint16    nBsTicks;      /* no FC after FF / last CF of a block */
    uint16    nCrTicks;      /* no CF while receiving */
} CanTp_ChannelConfigType;

typedef struct
{
    const CanTp_ChannelConfigType *channels;
    uint8                          numChannels;
} CanTp_ConfigType;

extern const CanTp_ConfigType CanTp_Config;

void CanTp_Init(const CanTp_ConfigType *CfgPtr);
void CanTp_MainFunction(void);
Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
Std_ReturnType CanTp_CancelTransmit(PduIdType TxPduId);
Std_ReturnType CanTp_CancelReceive(PduIdType RxPduId);
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void CanTp_TxConfirmation(PduIdType TxPduId);

/* True when no segmented transfer is in progress (ELC: gates Flash work). */
uint8 CanTp_IsIdle(void);

extern volatile uint8 CanTp_LastRxResult;   /* NotifResultType of the last failed reception */
extern volatile uint8 CanTp_LastTxResult;   /* NotifResultType of the last failed transmission */

#endif /* ELC_BSW_CANTP_H */
