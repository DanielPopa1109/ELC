#ifndef ECUM_H
#define ECUM_H

#include <stdint.h>
#include "crc.h"
#include "tim.h"
#include "dma.h"
#include "gpio.h"
#include "can.h"
#include "adc.h"
#include <string.h>
#include "EcuM_Cfg.h"

extern uint8_t EcuM_SWState;
extern uint8_t EcuM_WUPLine;
extern uint32_t EcuM_RunTimer;
extern uint32_t EcuM_PostRunTimer;
extern uint32_t EcuM_MainCounter;
extern uint8_t EcuM_SWV[4u];
extern const uint8_t EcuM_SoftwareVersion[4u];
extern uint8_t EcuM_ResetReason;
extern uint8_t EcuM_ResetInfo;
extern uint32_t EcuM_TimeActive;
extern uint32_t EcuM_TimeWithoutReset;
extern uint32_t EcuM_TotalResetCounter;

/* EcuM_Init(): call once, before anything reads the retained RAM window. Evaluates and clears the Standby/wake-up
 * flags; EcuM_GetWakeupReason() != ECUM_WKSOURCE_NONE means the retained window is undefined and must be cleared. */
extern void EcuM_Init(void);
extern void EcuM_MainFunction(void);                    /* 5 ms: RUN -> POST_RUN -> persistence -> (SLEEP) */
extern uint8_t EcuM_GetState(void);                     /* ECUM_STATE_RUN / POST_RUN / SLEEP */
extern uint8_t EcuM_GetWakeupReason(void);              /* ECUM_WKSOURCE_* */
extern void EcuM_PerformReset(uint8_t reason, uint8_t info);

#endif /* ECUM_H */
