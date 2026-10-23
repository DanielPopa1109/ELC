#include "CanNm.h"

#define CANNM_TIMEOUT_TICKS 200u
#define CANNM_ELC_REQUEST_MASK 0x40u

static volatile uint8_t pn1;
static volatile uint16_t age;
static volatile CanNm_StateType state;

void CanNm_Init(void)
{
    pn1 = 0u;
    age = CANNM_TIMEOUT_TICKS;
    state = CANNM_WAIT_NM;
}

void CanNm_RxIndication(const uint8_t *data, uint8_t len)
{
    if ((data == 0) || (len != 1u)) return;
    pn1 = data[0];
    age = 0u;
    state = ((pn1 & CANNM_ELC_REQUEST_MASK) != 0u) ?
            CANNM_NETWORK_REQUESTED : CANNM_NETWORK_RELEASED;
}

void CanNm_MainFunction(void)
{
    if (age < CANNM_TIMEOUT_TICKS) ++age;
    if ((age >= CANNM_TIMEOUT_TICKS) && (state != CANNM_WAIT_NM))
    {
        pn1 = 0u;
        state = CANNM_TIMED_OUT;
    }
}

uint8_t CanNm_GetPn1(void) { return pn1; }
uint8_t CanNm_IsNetworkActive(void)
{
    return (state == CANNM_NETWORK_REQUESTED) ? 1u : 0u;
}
uint16_t CanNm_GetInactiveTicks(void) { return age; }
CanNm_StateType CanNm_GetState(void) { return state; }
