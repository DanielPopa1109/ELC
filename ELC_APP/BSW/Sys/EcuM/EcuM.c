#include "EcuM.h"
#include "NvM.h"
#include "Dem.h"
#include "ComM.h"
#include "stm32f1xx.h"
#include "main.h"
#include "SMon.h"
#if ECUM_STANDBY_ENABLED
#include "Dcm.h"
#include "CanTp.h"
#endif

/* Software version stamp; a change makes main() clear the retained RAM window. */
const uint8_t EcuM_SoftwareVersion[4u] = {33u, 33u, 33u, 0xFFu};

uint8_t EcuM_SWState = ECUM_STATE_RUN;
uint8_t EcuM_WUPLine = 0xFFu;
uint32_t EcuM_RunTimer = ECUM_RUN_TIMEOUT_TICKS;
uint32_t EcuM_PostRunTimer = ECUM_POSTRUN_TIMEOUT_TICKS;
uint32_t EcuM_MainCounter = 0u;
uint8_t EcuM_SWV[4u] __attribute((section(".ncr")));
uint8_t EcuM_ResetReason __attribute((section(".ncr")));
uint8_t EcuM_ResetInfo __attribute((section(".ncr")));
uint32_t EcuM_TimeActive __attribute((section(".ncr")));
uint32_t EcuM_TimeWithoutReset __attribute((section(".ncr")));
uint32_t EcuM_TotalResetCounter __attribute((section(".ncr")));
/* Post-run persistence: 0 = write due, 2 = write running, 1 = done, 3 = failed.
 * Starts as "done" to avoid a flash write just because no NM3 was present after power-up. */
static uint8_t EcuM_PostRunPersisted = 1u;
static uint8_t EcuM_WakeupReason = ECUM_WKSOURCE_NONE;

void EcuM_Init(void)
{
	if(0u != (PWR->CSR & PWR_CSR_SBF))
	{
		/* Woke up from Standby (the WAKE line on PA0 is the only wake-up source): the SRAM was lost. */
		EcuM_WakeupReason = ECUM_WKSOURCE_WKUP_PIN;
		PWR->CR |= (PWR_CR_CSBF | PWR_CR_CWUF);
	}
}

uint8_t EcuM_GetState(void)
{
	return EcuM_SWState;
}

uint8_t EcuM_GetWakeupReason(void)
{
	return EcuM_WakeupReason;
}

#if ECUM_STANDBY_ENABLED
static uint8_t EcuM_PersistenceFinished(void)
{
	NvM_RequestResultType nvmResult = NVM_REQ_PENDING;

	(void)NvM_GetErrorStatus(NVM_BLOCK_ID_ALL, &nvmResult);
	return (uint8_t)((NVM_REQ_PENDING != nvmResult) && (FALSE == Dem_IsPersistencePending()));
}

static uint8_t EcuM_StandbyAllowed(ComM_ModeType comMode)
{
	return (uint8_t)((0u == EcuM_WUPLine)
			&& (COMM_FULL_COMMUNICATION != comMode)
			&& (0u == SMon_CalculatedCommand)
			&& (0u == SMon_L1ST)
			&& (DCM_SESSION_DEFAULT == Dcm_ActiveSessionState)
			&& (0u != CanTp_IsIdle())
			&& (0u != EcuM_PersistenceFinished()));
}

/* Enters STM32 Standby. Only returns if the WAKE line is high or a job started in the meantime. */
static void EcuM_EnterStandby(void)
{
	EcuM_SWState = ECUM_STATE_SLEEP;                         /* the OS tick hook stops restarting the ADC */
	PWR->CSR |= PWR_CSR_EWUP;                                /* WAKE line (PA0) rising edge wakes the MCU */
	PWR->CR |= PWR_CR_CWUF;                                  /* an edge after this point sets WUF and wakes at once */
	__disable_irq();                                         /* no task switch between the checks and the WFI */

	if((0u == HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)) && (0u != EcuM_PersistenceFinished()))
	{
		SysTick->CTRL = 0u;                                  /* a pending interrupt would end WFI without Standby */
		NVIC->ICER[0u] = 0xFFFFFFFFu;
		NVIC->ICER[1u] = 0xFFFFFFFFu;
		NVIC->ICPR[0u] = 0xFFFFFFFFu;
		NVIC->ICPR[1u] = 0xFFFFFFFFu;
		SCB->ICSR = (SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk);
		PWR->CR |= PWR_CR_PDDS;
		SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
		__DSB();
		__WFI();
		NVIC_SystemReset();                                  /* Standby was not entered: restart instead of running without interrupts */
	}

	__enable_irq();
	PWR->CSR &= ~PWR_CSR_EWUP;
	EcuM_SWState = ECUM_STATE_POST_RUN;
}
#endif

void EcuM_MainFunction(void)
{
	ComM_ModeType comMode = COMM_SILENT_COMMUNICATION;

	SMon_SWState = EcuM_SWState;
	EcuM_TimeActive += 5u;
	EcuM_TimeWithoutReset += 5u;
	EcuM_WUPLine = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
	(void)ComM_GetCurrentComMode(COMM_CHANNEL_CAN, &comMode);

	for(uint8_t i = 0; i < 4u; i++)
	{
		EcuM_SWV[i] = EcuM_SoftwareVersion[i];
	}

	if(0u == EcuM_MainCounter)
	{
		(void)Dem_SetOperationCycleState(DEM_OPERATION_CYCLE_POWER, DEM_CYCLE_STATE_START);
		if(EcuM_ResetReason)
		{
			(void)Dem_SetEventStatus(DEM_EVENT_MCU_RESET, DEM_EVENT_STATUS_FAILED);
		}
		else if(Dem_IsEventFailed(DEM_EVENT_MCU_RESET))
		{
			(void)Dem_SetEventStatus(DEM_EVENT_MCU_RESET, DEM_EVENT_STATUS_PASSED);
		}
		else
		{
			/* Do nothing. */
		}
	}

	if(1u == EcuM_WUPLine
			|| COMM_FULL_COMMUNICATION == comMode
			|| 1u == SMon_CalculatedCommand)
	{
		EcuM_SWState = ECUM_STATE_RUN;
		EcuM_RunTimer = ECUM_RUN_TIMEOUT_TICKS;
		EcuM_PostRunTimer = ECUM_POSTRUN_TIMEOUT_TICKS;
		EcuM_PostRunPersisted = 0u;
	}
	else if(0u < EcuM_RunTimer)
	{
		EcuM_RunTimer--;
	}
	else
	{
		/* Do nothing. */
	}

	if(0u == EcuM_RunTimer)
	{
		EcuM_SWState = ECUM_STATE_POST_RUN;

		if(4u != SMon_ShortToPlusTest)
		{
			/* Discharge test still running: post-run does not count down yet. */
			EcuM_PostRunTimer = ECUM_POSTRUN_TIMEOUT_TICKS;
		}
		else if(0u < EcuM_PostRunTimer)
		{
			EcuM_PostRunTimer--;
		}
		else
		{
			/* Do nothing. */
		}
	}

	/* Do not mark an asynchronous persistence request successful on submission. */
	if(ECUM_STATE_POST_RUN == EcuM_SWState && 0u == EcuM_PostRunTimer)
	{
		NvM_RequestResultType nvmResult = NVM_REQ_NOT_OK;

		(void)NvM_GetErrorStatus(NVM_BLOCK_ID_ALL, &nvmResult);
		if(0u == EcuM_PostRunPersisted)
		{
			(void)Dem_SetOperationCycleState(DEM_OPERATION_CYCLE_POWER, DEM_CYCLE_STATE_END);
			if(NvM_WriteAll() == E_OK)
			{
				EcuM_PostRunPersisted = 2u;
			}
		}
		else if(2u == EcuM_PostRunPersisted && NVM_REQ_PENDING != nvmResult)
		{
			EcuM_PostRunPersisted = (NVM_REQ_OK == nvmResult) ? 1u : 3u;
		}
		else
		{
			/* Do nothing. */
		}

#if ECUM_STANDBY_ENABLED
		/* Failed persistence must not discard the only remaining RAM copy in Standby. */
		if((1u == EcuM_PostRunPersisted) && EcuM_StandbyAllowed(comMode))
		{
			EcuM_EnterStandby();
		}
#endif
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
