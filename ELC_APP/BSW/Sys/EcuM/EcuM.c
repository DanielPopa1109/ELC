#include "EcuM.h"
#include "FreeRTOS.h"
#include "CanH.h"
#include "Nvm.h"
#include "Dem.h"
#include "stm32f1xx.h"
#include "main.h"
#include "cmsis_os2.h"
#include "task.h"
#include "SMon.h"
#include "Dcm.h"

uint8_t EcuM_SWState = 1u;
uint8_t EcuM_WUPLine = 0xFFu;
uint32_t EcuM_RunTimer = 200u;
uint32_t EcuM_PostRunTimer = 200u;
uint32_t EcuM_MainCounter = 0u;
uint8_t EcuM_SWV[4u] __attribute((section(".ncr")));
uint8_t EcuM_ResetReason __attribute((section(".ncr")));
uint8_t EcuM_ResetInfo __attribute((section(".ncr")));
uint32_t EcuM_TimeActive __attribute((section(".ncr")));
uint32_t EcuM_TimeWithoutReset __attribute((section(".ncr")));
uint32_t EcuM_TotalResetCounter __attribute((section(".ncr")));
/* Avoid a flash write just because no NM3 was present after power-up. */
static uint8_t EcuM_PostRunPersisted = 1u;

void EcuM_main();
void EcuM_PerformReset(uint8_t reason, uint8_t info);

void EcuM_main()
{
	SMon_SWState = EcuM_SWState;
	EcuM_TimeActive += 5u;
	EcuM_TimeWithoutReset += 5u;
	EcuM_WUPLine = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

	for(uint8_t i = 0; i < 4u; i++)
	{
		EcuM_SWV[i] = Dcm_SWV[i];
	}

	if(0u == EcuM_MainCounter)
	{
        Dem_SetOperationCycle(1u);
		if(EcuM_ResetReason)
		{
			Dem_SetDtc(0x57, 0x2f);
		}
		else
		{
			if(0x2f == Dem_GetDtcStatus(0x57u))
			{
				Dem_SetDtc(0x57, 0x2e);
			}
			else
			{
				/* Do nothing. */
			}
		}
	}
	else
	{
		/* Do nothing. */
	}

	if(1u == EcuM_WUPLine
			|| FULL_COMMUNICATION == CanH_CommunicationState
			|| 1u == SMon_CalculatedCommand)
	{
		EcuM_SWState = 1u;
		EcuM_RunTimer = 200u;
		EcuM_PostRunTimer = 200u;
		EcuM_PostRunPersisted = 0u;
	}
	else
	{
		if(0u < EcuM_RunTimer)
		{
			EcuM_RunTimer--;

			if(0u == EcuM_RunTimer)
			{
				EcuM_SWState = 2u;
			}
			else
			{
				/* Do nothing. */
			}
		}
		else
		{
			/* Do nothing. */
		}
	}

	if(0u == EcuM_RunTimer)
	{
		if( ( 4u != SMon_ShortToPlusTest )  && 0u == EcuM_RunTimer)
		{
			EcuM_SWState = 2u;
			EcuM_PostRunTimer = 200u;
		}
		else if( ( 4u == SMon_ShortToPlusTest ) && 0u == EcuM_RunTimer)
		{
			if(0u < EcuM_PostRunTimer)
			{
				EcuM_PostRunTimer--;

				if(0u == EcuM_PostRunTimer)
				{
					/* NM3 loss alone must not enter MCU standby: bxCAN cannot
					 * receive the next NM3 while the core is in this mode, and
					 * no verified CAN wake source is configured for this board.
					 * Stay in post-run with diagnostics and CAN RX alive.
					 */
					EcuM_SWState = 2u;

				}
				else
				{
					/* Do nothing. */
				}
			}
			else
			{
				/* Do nothing. */
			}
		}
		else
		{
			/* Do nothing. */
		}
	}
	else
	{
		/* Do nothing. */
	}

    /* Do not mark an asynchronous persistence request successful on submission. */
    if(EcuM_SWState==2u && EcuM_PostRunTimer==0u){
        if(EcuM_PostRunPersisted==0u){
            Dem_SetOperationCycle(0u);
            if(!Nvm_WriteAll())EcuM_PostRunPersisted=2u;
        }else if(EcuM_PostRunPersisted==2u && Nvm_GetErrorStatus(0u)!=NVM_PENDING){
            EcuM_PostRunPersisted=Nvm_GetErrorStatus(0u)==NVM_OK?1u:3u;
        }
    }

	EcuM_MainCounter++;
}

void EcuM_PerformReset(uint8_t reason, uint8_t info)
{
	EcuM_TimeWithoutReset = 0u;

	EcuM_ResetInfo = info;

	EcuM_ResetReason = reason;

	if(reason)
	{
		EcuM_TotalResetCounter++;
	}
	else
	{
		/* Do nothing. */
	}

	NVIC_SystemReset();
}
