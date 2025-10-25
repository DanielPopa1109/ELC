#include "Dcm.h"
#include "stdint.h"
#include "stdbool.h"
#include "can.h"
#include <string.h>

uint32_t Dcm_ActiveSessionState __attribute((section(".ncr")));
uint32_t Dcm_MainCounter = 0u;
uint32_t Dcm_TxMailbox = 0;
CAN_TxHeaderTypeDef Dcm_DiagTxHeader;
CAN_RxHeaderTypeDef Dcm_DiagRxHeader = {0, 0, 0, 0, 0, 0, 0};
uint8_t Dcm_RxData[8u];
uint8_t Dcm_TxData[8u];
uint8_t Dcm_SWV[4u] = {2u, 2u, 0xFFu, 0xFFu};
extern uint8_t SMon_CmdStat;
extern uint8_t SMon_RetryCnt;
extern uint8_t SMon_LockSupply;
extern uint32_t SMon_ISenseL1_RMS_5s;
extern uint32_t SMon_ISenseL1_RMS_10s;
extern uint32_t SMon_ISenseL1_RMS_30s;
extern uint32_t SMon_VfbT30_RMS_5s;
extern uint32_t SMon_VfbT30_RMS_10s;
extern uint32_t SMon_VfbT30_RMS_30s;
extern uint32_t SMon_VfbL1_RMS_5s;
extern uint32_t SMon_VfbL1_RMS_10s;
extern uint32_t SMon_VfbL1_RMS_30s;

void Dcm_main();
void Dcm_ProgrammingSession();
void Dcm_HardReset();
void Dcm_ReadSWV();
void Dcm_RC_HealSupply();
void Dcm_RC_ReadHistograms();
void Dcm_SendNrc();
extern void EcuM_PerformReset(uint8_t reason, uint8_t info);

void Dcm_RC_ReadHistograms()
{
	Dcm_TxData[0u] = Dcm_RxData[0u];
	Dcm_TxData[1u] = Dcm_RxData[1u] + 0x40u;
	Dcm_TxData[2u] = Dcm_RxData[2u];
	Dcm_TxData[3u] = Dcm_RxData[3u];
	Dcm_TxData[4u] = Dcm_RxData[4u];
	Dcm_TxData[5u] = Dcm_RxData[5u];
	Dcm_TxData[6u] = Dcm_RxData[6u];
	Dcm_TxData[7u] = Dcm_RxData[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox);
	HAL_Delay(1);

	Dcm_TxData[0u] = (uint8_t)(SMon_ISenseL1_RMS_5s);
	Dcm_TxData[1u] = (uint8_t)(SMon_ISenseL1_RMS_5s >> 8u);
	Dcm_TxData[2u] = (uint8_t)(SMon_ISenseL1_RMS_5s >> 16u);
	Dcm_TxData[3u] = (uint8_t)(SMon_ISenseL1_RMS_5s >> 24u);
	Dcm_TxData[4u] = (uint8_t)(SMon_ISenseL1_RMS_10s);
	Dcm_TxData[5u] = (uint8_t)(SMon_ISenseL1_RMS_10s >> 8u);
	Dcm_TxData[6u] = (uint8_t)(SMon_ISenseL1_RMS_10s >> 16u);
	Dcm_TxData[7u] = (uint8_t)(SMon_ISenseL1_RMS_10s >> 24u);
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox);
	HAL_Delay(1);

	Dcm_TxData[0u] = (uint8_t)(SMon_ISenseL1_RMS_30s);
	Dcm_TxData[1u] = (uint8_t)(SMon_ISenseL1_RMS_30s >> 8u);
	Dcm_TxData[2u] = (uint8_t)(SMon_ISenseL1_RMS_30s >> 16u);
	Dcm_TxData[3u] = (uint8_t)(SMon_ISenseL1_RMS_30s >> 24u);
	Dcm_TxData[4u] = 0x00;
	Dcm_TxData[5u] = 0x00;
	Dcm_TxData[6u] = 0x00;
	Dcm_TxData[7u] = 0x00;
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox);
	HAL_Delay(1);

	Dcm_TxData[0u] = (uint8_t)(SMon_VfbT30_RMS_5s);
	Dcm_TxData[1u] = (uint8_t)(SMon_VfbT30_RMS_5s >> 8u);
	Dcm_TxData[2u] = (uint8_t)(SMon_VfbT30_RMS_5s >> 16u);
	Dcm_TxData[3u] = (uint8_t)(SMon_VfbT30_RMS_5s >> 24u);
	Dcm_TxData[4u] = (uint8_t)(SMon_VfbT30_RMS_10s);
	Dcm_TxData[5u] = (uint8_t)(SMon_VfbT30_RMS_10s >> 8u);
	Dcm_TxData[6u] = (uint8_t)(SMon_VfbT30_RMS_10s >> 16u);
	Dcm_TxData[7u] = (uint8_t)(SMon_VfbT30_RMS_10s >> 24u);
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox);
	HAL_Delay(1);

	Dcm_TxData[0u] = (uint8_t)(SMon_VfbT30_RMS_30s);
	Dcm_TxData[1u] = (uint8_t)(SMon_VfbT30_RMS_30s >> 8u);
	Dcm_TxData[2u] = (uint8_t)(SMon_VfbT30_RMS_30s >> 16u);
	Dcm_TxData[3u] = (uint8_t)(SMon_VfbT30_RMS_30s >> 24u);
	Dcm_TxData[4u] = 0x00;
	Dcm_TxData[5u] = 0x00;
	Dcm_TxData[6u] = 0x00;
	Dcm_TxData[7u] = 0x00;
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox);
	HAL_Delay(1);

	Dcm_TxData[0u] = (uint8_t)(SMon_VfbL1_RMS_5s);
	Dcm_TxData[1u] = (uint8_t)(SMon_VfbL1_RMS_5s >> 8u);
	Dcm_TxData[2u] = (uint8_t)(SMon_VfbL1_RMS_5s >> 16u);
	Dcm_TxData[3u] = (uint8_t)(SMon_VfbL1_RMS_5s >> 24u);
	Dcm_TxData[4u] = (uint8_t)(SMon_VfbL1_RMS_10s);
	Dcm_TxData[5u] = (uint8_t)(SMon_VfbL1_RMS_10s >> 8u);
	Dcm_TxData[6u] = (uint8_t)(SMon_VfbL1_RMS_10s >> 16u);
	Dcm_TxData[7u] = (uint8_t)(SMon_VfbL1_RMS_10s >> 24u);
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox);
	HAL_Delay(1);

	Dcm_TxData[0u] = (uint8_t)(SMon_VfbL1_RMS_30s);
	Dcm_TxData[1u] = (uint8_t)(SMon_VfbL1_RMS_30s >> 8u);
	Dcm_TxData[2u] = (uint8_t)(SMon_VfbL1_RMS_30s >> 16u);
	Dcm_TxData[3u] = (uint8_t)(SMon_VfbL1_RMS_30s >> 24u);
	Dcm_TxData[4u] = 0x00;
	Dcm_TxData[5u] = 0x00;
	Dcm_TxData[6u] = 0x00;
	Dcm_TxData[7u] = 0x00;
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox);
	HAL_Delay(1);
}

void Dcm_ProgrammingSession()
{
	Dcm_TxData[0u] = Dcm_RxData[0u];
	Dcm_TxData[1u] = Dcm_RxData[1u] + 0x40u;
	Dcm_TxData[2u] = Dcm_RxData[2u];
	Dcm_TxData[3u] = Dcm_RxData[3u];
	Dcm_TxData[4u] = Dcm_RxData[4u];
	Dcm_TxData[5u] = Dcm_RxData[5u];
	Dcm_TxData[6u] = Dcm_RxData[6u];
	Dcm_TxData[7u] = Dcm_RxData[7u];
	Dcm_ActiveSessionState = Dcm_RxData[2u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox);
	HAL_Delay(1);
	EcuM_PerformReset(0u, 0u);
}

void Dcm_SendNrc()
{
	Dcm_TxData[0u] = 0x03;
	Dcm_TxData[1u] = 0x7f;
	Dcm_TxData[2u] = 0x19;
	Dcm_TxData[3u] = 0x22;
	Dcm_TxData[4u] = Dcm_RxData[4u];
	Dcm_TxData[5u] = Dcm_RxData[5u];
	Dcm_TxData[6u] = Dcm_RxData[6u];
	Dcm_TxData[7u] = Dcm_RxData[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox);
}

void Dcm_HardReset()
{
	Dcm_TxData[0u] = Dcm_RxData[0u];
	Dcm_TxData[1u] = Dcm_RxData[1u] + 0x40u;
	Dcm_TxData[2u] = Dcm_RxData[2u];
	Dcm_TxData[3u] = Dcm_RxData[3u];
	Dcm_TxData[4u] = Dcm_RxData[4u];
	Dcm_TxData[5u] = Dcm_RxData[5u];
	Dcm_TxData[6u] = Dcm_RxData[6u];
	Dcm_TxData[7u] = Dcm_RxData[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox);
	HAL_Delay(1);
	EcuM_PerformReset(0u, 0u);
}

void Dcm_ReadSWV()
{
	Dcm_TxData[0u] = Dcm_RxData[0u] + 4u;
	Dcm_TxData[1u] = Dcm_RxData[1u] + 0x40u;
	Dcm_TxData[2u] = Dcm_RxData[2u];
	Dcm_TxData[3u] = Dcm_RxData[3u];
	Dcm_TxData[4u] = Dcm_SWV[0u];
	Dcm_TxData[5u] = Dcm_SWV[1u];
	Dcm_TxData[6u] = Dcm_SWV[2u];
	Dcm_TxData[7u] = Dcm_SWV[3u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	while(0 == HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox));
	for(uint8_t i = 0; i < 8; i++) Dcm_TxData[i] = 0;
	Dcm_DiagTxHeader.DLC = 0;
	Dcm_DiagTxHeader.StdId = 0;
}

void Dcm_RC_HealSupply()
{
	SMon_RetryCnt = 0u;
	SMon_LockSupply = 0u;
	Dcm_TxData[0u] = Dcm_RxData[0u];
	Dcm_TxData[1u] = Dcm_RxData[1u] + 0x40u;
	Dcm_TxData[2u] = Dcm_RxData[2u];
	Dcm_TxData[3u] = Dcm_RxData[3u];
	Dcm_TxData[4u] = Dcm_RxData[4u];
	Dcm_TxData[5u] = Dcm_RxData[5u];
	Dcm_TxData[6u] = Dcm_RxData[6u];
	Dcm_TxData[7u] = Dcm_RxData[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;
	HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox);
	for(uint8_t i = 0; i < 8; i++) Dcm_TxData[i] = 0;
	Dcm_DiagTxHeader.DLC = 0;
	Dcm_DiagTxHeader.StdId = 0;
}

void Dcm_main()
{
	if(0u == Dcm_MainCounter)
	{
		Dcm_ActiveSessionState = 0u;
	}
	else
	{
		/* Do nothing. */
	}

	if(0x02 == Dcm_RxData[2u] && 0u == SMon_CmdStat)
	{
		Dcm_ProgrammingSession();
	}
	else if(0x02 == Dcm_RxData[2u] && 1u == SMon_CmdStat)
	{
		Dcm_SendNrc();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x11u == Dcm_RxData[1u] && 0x01u == Dcm_RxData[2u] && 0u == SMon_CmdStat)
	{
		Dcm_HardReset();
	}
	else if(0x11u == Dcm_RxData[1u] && 0x01u == Dcm_RxData[2u] && 1u == SMon_CmdStat)
	{
		Dcm_SendNrc();
	}
	else
	{
		/* Do nothing. */
	}

	if(0xf1u == Dcm_RxData[2u] && 0x80u == Dcm_RxData[3u])
	{
		Dcm_ReadSWV();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x40u == Dcm_RxData[4u] && 0x31u == Dcm_RxData[1u])
	{
		Dcm_RC_HealSupply();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x41u == Dcm_RxData[4u] && 0x31u == Dcm_RxData[1u])
	{
		Dcm_RC_ReadHistograms();
	}
	else
	{
		/* Do nothing. */
	}

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_RxData[i] = 0u;
		Dcm_TxData[i] = 0u;
	}

	Dcm_MainCounter++;
}
