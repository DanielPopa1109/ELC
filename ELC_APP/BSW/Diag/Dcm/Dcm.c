#include "Dcm.h"
#include "stdint.h"
#include "stdbool.h"
#include "can.h"
#include <string.h>

#define NUM_BINS 256
#define HIST_BYTES_PER_BIN 4   // uint32_t per bin
#define HIST_TOTAL_BYTES (NUM_BINS * HIST_BYTES_PER_BIN)
#define ISO_TP_MAX_SINGLE_FRAME 7  // 8-byte CAN frame, 1 byte for PCI
#define ISO_TP_MAX_FRAME_DATA 7

extern uint32_t SMon_Hist_ISenseL1_ms[NUM_BINS];
extern uint32_t SMon_Hist_VfbT30_ms[NUM_BINS];
extern uint32_t SMon_Hist_VfbL1_ms[NUM_BINS];
static uint32_t Dcm_MainCounter = 0u;
uint32_t Dcm_TxMailbox = 0;
CAN_RxHeaderTypeDef Dcm_DiagRxHeader = {0, 0, 0, 0, 0, 0, 0};
uint8_t Dcm_RxData[8u];
uint8_t Dcm_TxData[8u];
CAN_TxHeaderTypeDef Dcm_DiagTxHeader;
uint8_t Dcm_CDTCS_Status = 1u;
extern uint8_t SMon_CmdStat;
extern uint8_t SMon_RetryCnt;
extern uint8_t SMon_LockSupply;
uint32_t Dcm_ActiveSessionState __attribute((section(".ncr")));
uint8_t Dcm_SWV[4u] = {1u, 1u, 0xFFu, 0xFFu};

void Dcm_main();
void Dcm_ProgrammingSession();
void Dcm_HardReset();
void Dcm_ReadSWV();
void Dcm_RC_HealSupply();
void Dcm_RC_ReadHistograms();
void Dcm_SendHistogramISO_TP(uint32_t *hist_array);
void Dcm_SendNrc();

extern void EcuM_PerformReset(uint8_t reason, uint8_t info);

void Dcm_RC_ReadHistograms()
{
	Dcm_SendHistogramISO_TP(SMon_Hist_ISenseL1_ms);
	Dcm_SendHistogramISO_TP(SMon_Hist_VfbT30_ms);
	Dcm_SendHistogramISO_TP(SMon_Hist_VfbL1_ms);
}

void Dcm_SendHistogramISO_TP(uint32_t *hist_array)
{
	uint16_t byte_index = 0;
	uint8_t frame_data[7];
	uint8_t dlc;

	while(byte_index < HIST_TOTAL_BYTES)
	{
		dlc = 0;

		for(uint8_t i=0; i<ISO_TP_MAX_FRAME_DATA && byte_index < HIST_TOTAL_BYTES; i++)
		{
			// Convert uint32_t bin to bytes, 1 byte per PCI frame data slot
			uint32_t bin_val = hist_array[byte_index / HIST_BYTES_PER_BIN];
			frame_data[i] = (bin_val >> ((byte_index % 4)*8)) & 0xFF;
			byte_index++;
			dlc++;
		}

		// Set PCI byte for first frame / consecutive frame
		Dcm_TxData[0] = 0x20; // Consecutive frame PCI

		for(uint8_t j=0; j<dlc; j++)
		{
			Dcm_TxData[j+1] = frame_data[j];
		}

		Dcm_DiagTxHeader.DLC = 8u;
		Dcm_DiagTxHeader.StdId = 0x703u;
		while(0u == HAL_CAN_AddTxMessage(&hcan, &Dcm_DiagTxHeader, Dcm_TxData, &Dcm_TxMailbox));
		HAL_Delay(1);
	}
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
