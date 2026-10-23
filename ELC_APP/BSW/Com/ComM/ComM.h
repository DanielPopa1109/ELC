#ifndef ELC_BSW_COMM_H
#define ELC_BSW_COMM_H

#include <stdint.h>

void ComM_Init(void);
void ComM_MainFunction(void);
uint8_t ComM_IsApplicationCommunicationAllowed(void);

#endif
