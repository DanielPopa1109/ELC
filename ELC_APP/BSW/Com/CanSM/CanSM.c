#include "CanSM.h"
#include "../CanIf/CanIf.h"
#include "../Can/Can.h"

static CanSM_ModeType requestedMode;
static CanSM_ModeType currentMode;

void CanSM_Init(void)
{
    requestedMode = CANSM_RX_ONLY;
    currentMode = CANSM_RX_ONLY;
    CanIf_SetApplicationTxEnabled(0u);
}

void CanSM_RequestComMode(CanSM_ModeType mode)
{
    if (mode <= CANSM_FULL_COMMUNICATION) requestedMode = mode;
}

void CanSM_MainFunction(void)
{
    /* bxCAN remains started and RX online. HAL automatic bus-off recovery is
     * configured in MX_CAN_Init; never advertise FullCom while BOFF is set.
     */
    CanSM_ModeType availableMode = (Can_IsControllerOnline() != 0u) ?
            requestedMode : CANSM_RX_ONLY;
    if (currentMode != availableMode)
    {
        CanIf_SetApplicationTxEnabled(availableMode == CANSM_FULL_COMMUNICATION);
        currentMode = availableMode;
    }
}

CanSM_ModeType CanSM_GetCurrentComMode(void) { return currentMode; }
