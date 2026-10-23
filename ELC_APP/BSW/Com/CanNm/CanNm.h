#ifndef ELC_BSW_CANNM_H
#define ELC_BSW_CANNM_H

#include <stdint.h>

/* Gateway NM3 PN1 on CAN 0x3FF; called on the 5 ms BSW cycle. */
typedef enum {
    CANNM_WAIT_NM = 0,
    CANNM_NETWORK_REQUESTED,
    CANNM_NETWORK_RELEASED,
    CANNM_TIMED_OUT
} CanNm_StateType;
void CanNm_Init(void);
void CanNm_RxIndication(const uint8_t *data, uint8_t len);
void CanNm_MainFunction(void);
uint8_t CanNm_GetPn1(void);
uint8_t CanNm_IsNetworkActive(void);
uint16_t CanNm_GetInactiveTicks(void);
CanNm_StateType CanNm_GetState(void);

#endif
