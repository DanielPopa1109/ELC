#include "CanNm.h"
#include "../ComM/ComM.h"

static volatile uint8 CanNm_Pn1;
static volatile uint16 CanNm_Age;
static volatile CanNm_StateType CanNm_State;
static boolean CanNm_RxEnabled;

static void CanNm_SetState(CanNm_StateType state)
{
    CanNm_StateType previous = CanNm_State;

    CanNm_State = state;
    if ((state == CANNM_NETWORK_REQUESTED) && (previous != CANNM_NETWORK_REQUESTED))
    {
        ComM_Nm_NetworkMode(COMM_CHANNEL_CAN);
    }
    else if ((state != CANNM_NETWORK_REQUESTED) && (previous == CANNM_NETWORK_REQUESTED))
    {
        ComM_Nm_BusSleepMode(COMM_CHANNEL_CAN);
    }
}

void CanNm_Init(void)
{
    CanNm_Pn1 = 0u;
    CanNm_Age = CANNM_TIMEOUT_TICKS;
    CanNm_State = CANNM_WAIT_NM;
    CanNm_RxEnabled = TRUE;
}

void CanNm_EnableRx(boolean Enabled)
{
    CanNm_RxEnabled = (Enabled != 0u) ? TRUE : FALSE;
}

void CanNm_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    (void)RxPduId;
    if ((CanNm_RxEnabled == FALSE) || (PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR) ||
        (PduInfoPtr->SduLength != 1u))
    {
        return;
    }
    CanNm_Pn1 = PduInfoPtr->SduDataPtr[0];
    CanNm_Age = 0u;
    CanNm_SetState(((CanNm_Pn1 & CANNM_ELC_REQUEST_MASK) != 0u) ? CANNM_NETWORK_REQUESTED : CANNM_NETWORK_RELEASED);
}

void CanNm_MainFunction(void)
{
    if (CanNm_Age < CANNM_TIMEOUT_TICKS)
    {
        ++CanNm_Age;
    }
    if ((CanNm_Age >= CANNM_TIMEOUT_TICKS) && (CanNm_State != CANNM_WAIT_NM) && (CanNm_State != CANNM_TIMED_OUT))
    {
        CanNm_Pn1 = 0u;
        CanNm_SetState(CANNM_TIMED_OUT);
    }
}

uint8 CanNm_GetPn1(void) { return CanNm_Pn1; }
uint16 CanNm_GetInactiveTicks(void) { return CanNm_Age; }
CanNm_StateType CanNm_GetState(void) { return CanNm_State; }
