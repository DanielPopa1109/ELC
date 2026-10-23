#include "ComM.h"
#include "../CanNm/CanNm.h"
#include "../CanSM/CanSM.h"

static uint8_t applicationAllowed;

void ComM_Init(void)
{
    applicationAllowed = 0u;
    CanSM_RequestComMode(CANSM_RX_ONLY);
}

void ComM_MainFunction(void)
{
    applicationAllowed = CanNm_IsNetworkActive();
    CanSM_RequestComMode(applicationAllowed ? CANSM_FULL_COMMUNICATION : CANSM_RX_ONLY);
}

uint8_t ComM_IsApplicationCommunicationAllowed(void)
{
    return applicationAllowed;
}
