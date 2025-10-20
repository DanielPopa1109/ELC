#include "EcuM.h"
#include "stm32f1xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include "CanH.h"
#include "adc.h"

extern uint8_t SMon_ShortToPlusTest; // Discharge Test Status
extern CAN_HandleTypeDef hcan;
extern CanH_ComStat_t CanH_CommunicationState;
extern uint8_t SMon_CmdStat;
uint8_t EcuM_SWState = 1u;
uint8_t EcuM_WUPLine = 0u;
uint8_t EcuM_SleeModeActive = 0u;
uint32_t EcuM_RunTimer = 58u;
uint32_t EcuM_PostRunTimer = 58u;
static uint32_t mainCnt = 0u;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

void EcuM_GoSleep(void);
void EcuM_main();
void EcuM_PerformReset(uint8_t reason, uint8_t info);
void EcuM_main()
{
	EcuM_WUPLine = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

	if(1u == SMon_CmdStat || 1u == EcuM_WUPLine || FULL_COMMUNICATION == CanH_CommunicationState)
	{
		EcuM_SWState = 1u;
		EcuM_RunTimer = 58u;
		EcuM_PostRunTimer = 58u;
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

	if(4u != SMon_ShortToPlusTest && 0u == EcuM_RunTimer)
	{
		EcuM_SWState = 2u;
		EcuM_PostRunTimer = 58u;
	}
	else if(4u == SMon_ShortToPlusTest && 0u == EcuM_RunTimer)
	{
		if(0u < EcuM_PostRunTimer)
		{
			EcuM_PostRunTimer--;

			if(0u == EcuM_PostRunTimer)
			{
				EcuM_GoSleep();
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

	mainCnt++;
}

void EcuM_GoSleep(void)
{
	__disable_irq();
	//	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1);
	//	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2);
	//	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_3);
	//	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_4);
	//	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5);
	//	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_6);
	//	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_7);
	//	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_8);
	//	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9);
	//	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_10);
	//	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_15);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_1);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_2);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_3);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_4);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_5);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_7);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_9);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_11);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_12);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_13);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_14);
	//	HAL_GPIO_DeInit(GPIOB, GPIO_PIN_15);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_0);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_2);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_3);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_4);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_5);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_7);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_9);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_11);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_12);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_13);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_14);
	//	HAL_GPIO_DeInit(GPIOC, GPIO_PIN_15);
	//	HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_14 | GPIO_PIN_13;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_1 | GPIO_PIN_5 | GPIO_PIN_3;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	__enable_irq();
	HAL_ADC_Stop_DMA(&hadc1);
	HAL_ADC_DeInit(&hadc1);
	__disable_irq();
	__HAL_RCC_GPIOB_CLK_DISABLE();
	__HAL_RCC_GPIOC_CLK_DISABLE();
	__HAL_RCC_DMA1_CLK_DISABLE();
	__HAL_RCC_ADC1_CLK_DISABLE();
	__HAL_RCC_AFIO_CLK_DISABLE();
	__HAL_RCC_TIM1_CLK_DISABLE();
	HAL_TIM_PWM_Stop_IT(&htim1, 0);
	HAL_TIM_PWM_DeInit(&htim1);
	HAL_SuspendTick();
	for(uint8_t i = 0; i < 82; i++) HAL_NVIC_ClearPendingIRQ(i);
	SysTick->CTRL &= ~(SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk);
	HAL_TIM_Base_Start_IT(&htim2);
	EcuM_SleeModeActive = 1;
	HAL_CAN_RequestSleep(&hcan);
	__enable_irq();
	HAL_DBGMCU_DisableDBGSleepMode();
	HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);
	EcuM_PerformReset(0,0);
}

void EcuM_ProcessTimerInterrupt(void)
{
	if(1u == HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0))
	{
		EcuM_PerformReset(0,0);
	}
	else
	{
		HAL_PWR_EnableSleepOnExit();
	}
}

void EcuM_PerformReset(uint8_t reason, uint8_t info)
{
	NVIC_SystemReset();
}
