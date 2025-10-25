#include "CanH.h"
#include "can.h"

uint32_t CanH_MainCounter = 0;
CAN_RxHeaderTypeDef CanH_RxHeader = {0, 0, 0, 0, 0, 0, 0};
uint8_t CanH_RxData[8] = {0};
CAN_TxHeaderTypeDef CanH_TxHeader = {0, 0, 0, 0, 0, 0};
uint8_t CanH_TxData[8] = {0};
uint32_t CanH_TxMailbox = 0;
uint32_t CanH_NoCommCounter = 0;
uint8_t CanH_RequestBusSleep = 0;
CanH_ComStat_t CanH_CommunicationState = PARTIAL_COMMUNICATION;
extern CAN_RxHeaderTypeDef Dcm_DiagRxHeader;
extern CAN_HandleTypeDef hcan;
extern uint8_t Dcm_RxData[8u];

extern uint8_t EcuM_SleeModeActive;
extern uint8_t SMon_S2BErrorStatus;
extern uint8_t SMon_I2TError;
extern uint8_t SMon_L1_UVStatus;
extern uint8_t SMon_CLS_Failure;
extern uint8_t SMon_ECU_UV;
extern uint8_t SMon_ECU_OV;
extern uint8_t SMon_CmdStat;
extern uint8_t SMon_WupLineState; // SYS_WKUP
extern uint32_t SMon_ISenseL1; // I Sense L1
extern uint16_t SMon_VfbL1; // Voltage Feedback L1/CLS
extern uint16_t SMon_VfbT30; // Voltage Feedback KL30
extern uint8_t SMon_L1ST;
extern uint8_t SMon_RetryCnt;
extern uint8_t SMon_LockSupply;
extern uint32_t SMon_I2TCounter;
extern uint8_t EcuM_SWState;

extern void EcuM_PerformReset(uint8_t reason, uint8_t info);
void CanH_MainFunction(void);
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_RxFifo0MsgFullCallback(CAN_HandleTypeDef *hcan);

void CanH_MainFunction(void)
{
	if(0x04 != HAL_CAN_GetError(&hcan))
	{
		if(FULL_COMMUNICATION == CanH_CommunicationState)
		{
			if(CanH_MainCounter % 200 == 0)
			{
				CanH_TxData[0] = SMon_L1ST;
				CanH_TxHeader.DLC = 1;
				CanH_TxHeader.StdId = 0x51;
				HAL_CAN_AddTxMessage(&hcan, &CanH_TxHeader, CanH_TxData, &CanH_TxMailbox);
				for(uint8_t i = 0; i < 8; i++) CanH_TxData[i] = 0;
				CanH_TxHeader.DLC = 0;
				CanH_TxHeader.StdId = 0;
			}
			else
			{
				/* Do nothing. */
			}

			if(CanH_MainCounter % 2 == 0)
			{
				CanH_TxData[0] = (uint8_t)(SMon_ISenseL1);
				CanH_TxData[1] = (uint8_t)(SMon_ISenseL1 >> 8u);
				CanH_TxData[2] = (uint8_t)(SMon_ISenseL1 >> 16u);
				CanH_TxData[3] = (uint8_t)(SMon_ISenseL1 >> 24u);
				CanH_TxData[4] = (uint8_t)(SMon_VfbT30);
				CanH_TxData[5] = (uint8_t)(SMon_VfbT30 >> 8u);
				CanH_TxData[6] = (uint8_t)(SMon_VfbL1);
				CanH_TxData[7] = (uint8_t)(SMon_VfbL1 >> 8u);
				CanH_TxHeader.DLC = 8;
				CanH_TxHeader.StdId = 0x6ef;
				HAL_CAN_AddTxMessage(&hcan, &CanH_TxHeader, CanH_TxData, &CanH_TxMailbox);
				for(uint8_t i = 0; i < 8; i++) CanH_TxData[i] = 0;
				CanH_TxHeader.DLC = 0;
				CanH_TxHeader.StdId = 0;
			}
			else
			{
				/* Do nothing. */
			}

			if(CanH_MainCounter % 20 == 0)
			{
				CanH_TxData[0] = SMon_ECU_UV;
				CanH_TxData[1] = SMon_ECU_OV;
				CanH_TxData[2] = (uint8_t)(SMon_I2TCounter);
				CanH_TxData[3] = (uint8_t)(SMon_I2TCounter >> 8u);
				CanH_TxData[4] = (uint8_t)(SMon_I2TCounter >> 16u);
				CanH_TxData[5] = (uint8_t)(SMon_I2TCounter >> 24u);
				CanH_TxData[6] = SMon_CLS_Failure;
				CanH_TxData[7] = SMon_L1_UVStatus;
				CanH_TxHeader.DLC = 8;
				CanH_TxHeader.StdId = 0x6f0;
				HAL_CAN_AddTxMessage(&hcan, &CanH_TxHeader, CanH_TxData, &CanH_TxMailbox);
				for(uint8_t i = 0; i < 8; i++) CanH_TxData[i] = 0;
				CanH_TxHeader.DLC = 0;
				CanH_TxHeader.StdId = 0;
			}
			else
			{
				/* Do nothing. */
			}

			if(CanH_MainCounter % 10 == 0)
			{
				CanH_TxData[0] = SMon_I2TError;
				CanH_TxData[1] = SMon_LockSupply;
				CanH_TxData[2] = SMon_RetryCnt;
				CanH_TxData[3] = SMon_WupLineState;
				CanH_TxData[4] = SMon_S2BErrorStatus;
				CanH_TxHeader.DLC = 5;
				CanH_TxHeader.StdId = 0x6f1;
				HAL_CAN_AddTxMessage(&hcan, &CanH_TxHeader, CanH_TxData, &CanH_TxMailbox);
				for(uint8_t i = 0; i < 8; i++) CanH_TxData[i] = 0;
				CanH_TxHeader.DLC = 0;
				CanH_TxHeader.StdId = 0;
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

	if(58u <= CanH_NoCommCounter)
	{
		CanH_CommunicationState = NO_COMMUNICATION;
	}
	else
	{
		/* Do nothing. */
	}

	CanH_NoCommCounter++;
	CanH_MainCounter++;

	for(uint8_t i = 0; i < 8; i++)
	{
		CanH_TxData[i] = 0;
	}
}
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	if(1u == EcuM_SleeModeActive)
	{
		HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &CanH_RxHeader, CanH_RxData);

		if(0x3FF == CanH_RxHeader.StdId)
		{
			if(0x12 == CanH_RxData[0])
			{
				EcuM_PerformReset(0,0);
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

		HAL_PWR_EnableSleepOnExit();
	}
	else
	{
		/* Do nothing. */
	}
	/* Pending and full callback to prevent messages being lost. */
	HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &CanH_RxHeader, CanH_RxData);

	if(0x3ff == CanH_RxHeader.StdId)
	{
		if(0x12 == CanH_RxData[0])
		{
			CanH_RequestBusSleep = 0;
			CanH_CommunicationState = FULL_COMMUNICATION;
			CanH_NoCommCounter = 0;
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

	if(0x50u == CanH_RxHeader.StdId)
	{
		SMon_CmdStat = CanH_RxData[0];
	}
	else
	{
		/* Do nothing. */
	}
	/* DIAG */
	if(0x702 == CanH_RxHeader.StdId)
	{
		Dcm_DiagRxHeader.StdId = CanH_RxHeader.StdId;
		Dcm_DiagRxHeader.DLC = CanH_RxHeader.DLC;

		for(uint8_t i = 0; i < 8; i++)
		{
			Dcm_RxData[i] = CanH_RxData[i];
		}
	}
	else
	{
		/* Do nothing. */
	}

	CanH_RxHeader.DLC = 0;
	CanH_RxHeader.ExtId = 0;
	CanH_RxHeader.FilterMatchIndex = 0;
	CanH_RxHeader.IDE = 0;
	CanH_RxHeader.RTR = 0;
	CanH_RxHeader.StdId = 0;
	CanH_RxHeader.Timestamp = 0;

	for(uint8_t i = 0; i < 8; i++)
	{
		CanH_RxData[i] = 0;
	}
}
void HAL_CAN_RxFifo0MsgFullCallback(CAN_HandleTypeDef *hcan)
{
	if(1u == EcuM_SleeModeActive)
	{
		HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &CanH_RxHeader, CanH_RxData);

		if(0x3FF == CanH_RxHeader.StdId)
		{
			if(0x12 == CanH_RxData[0])
			{
				EcuM_PerformReset(0,0);
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

		HAL_PWR_EnableSleepOnExit();
	}
	else
	{
		/* Do nothing. */
	}

	/* Pending and full callback to prevent messages being lost. */
	HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &CanH_RxHeader, CanH_RxData);

	if(0x3ff == CanH_RxHeader.StdId)
	{
		if(0x12 == CanH_RxData[0])
		{
			CanH_RequestBusSleep = 0;
			CanH_CommunicationState = FULL_COMMUNICATION;
			CanH_NoCommCounter = 0;
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

	if(0x50u == CanH_RxHeader.StdId)
	{
		SMon_CmdStat = CanH_RxData[0];
	}
	else
	{
		/* Do nothing. */
	}
	/* DIAG */
	if(0x702 == CanH_RxHeader.StdId)
	{
		Dcm_DiagRxHeader.StdId = CanH_RxHeader.StdId;
		Dcm_DiagRxHeader.DLC = CanH_RxHeader.DLC;

		for(uint8_t i = 0; i < 8; i++)
		{
			Dcm_RxData[i] = CanH_RxData[i];
		}
	}
	else
	{
		/* Do nothing. */
	}

	CanH_RxHeader.DLC = 0;
	CanH_RxHeader.ExtId = 0;
	CanH_RxHeader.FilterMatchIndex = 0;
	CanH_RxHeader.IDE = 0;
	CanH_RxHeader.RTR = 0;
	CanH_RxHeader.StdId = 0;
	CanH_RxHeader.Timestamp = 0;

	for(uint8_t i = 0; i < 8; i++)
	{
		CanH_RxData[i] = 0;
	}
}
