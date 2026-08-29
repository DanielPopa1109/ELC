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

extern uint8_t EcuM_SWState;
extern uint8_t EcuM_WUPLine;
extern uint32_t EcuM_RunTimer;
extern uint32_t EcuM_PostRunTimer;
extern uint32_t EcuM_MainCounter;
extern uint8_t EcuM_SWV[4u];
extern uint8_t EcuM_ResetReason;
extern uint8_t EcuM_ResetInfo;
extern uint32_t EcuM_TimeActive;
extern uint32_t EcuM_TimeWithoutReset;
extern uint32_t EcuM_TotalResetCounter;

extern void EcuM_main(void);
extern void EcuM_PerformReset(uint8_t reason, uint8_t info);

#endif /* ECUM_H */
