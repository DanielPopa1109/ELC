/* CanNm.h - receive-only NM3 observer (gateway NM3, CAN 0x3FF, DLC 1).
 *
 * ZGW defines the sender; no source defines subordinate transmit or sleep
 * handshake semantics, so ELC does not transmit NM. Bit 6 of the PN1 byte is the
 * ELC application communication request. The network is reported to ComM as
 * "network mode" on request and as "bus sleep" on release or after 200 ticks
 * without a frame (this is an indication only; the MCU never enters standby). */
#ifndef ELC_BSW_CANNM_H
#define ELC_BSW_CANNM_H

#include "ComStack_Cfg.h"

#define CANNM_TIMEOUT_TICKS       200u
#define CANNM_ELC_REQUEST_MASK    0x40u

typedef enum
{
    CANNM_WAIT_NM = 0,
    CANNM_NETWORK_REQUESTED,
    CANNM_NETWORK_RELEASED,
    CANNM_TIMED_OUT
} CanNm_StateType;

void CanNm_Init(void);
void CanNm_MainFunction(void);                                           /* 5 ms */
void CanNm_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void CanNm_EnableRx(boolean Enabled);       /* UDS CommunicationControl, NM reception */

uint8 CanNm_GetPn1(void);
uint16 CanNm_GetInactiveTicks(void);
CanNm_StateType CanNm_GetState(void);

#endif /* ELC_BSW_CANNM_H */
