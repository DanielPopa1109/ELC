#ifndef ELC_BSW_CANSM_H
#define ELC_BSW_CANSM_H

#include <stdint.h>

typedef enum { CANSM_RX_ONLY = 0, CANSM_FULL_COMMUNICATION = 1 } CanSM_ModeType;

void CanSM_Init(void);
void CanSM_RequestComMode(CanSM_ModeType mode);
CanSM_ModeType CanSM_GetCurrentComMode(void);
void CanSM_MainFunction(void);

#endif
