#include "Dem.h"
#include "can.h"
#include "Nvm.h"
#include "CanH.h"

extern CanH_ComStat_t CanH_CommunicationState;
extern uint8_t CSBSDAT_Year;
extern uint8_t CSBSDAT_Month;
extern uint8_t CSBSDAT_Day;
extern uint8_t CSBSDAT_Hour;
extern uint8_t CSBSDAT_Minute;
extern uint8_t CSBSDAT_Second;
extern uint8_t CSBSDAT_Millisecond;
extern uint8_t Dcm_CDTCS;
extern uint8_t OS_XCP_U8_CPU_Load;
extern uint16_t SMon_VfbL1;
extern uint16_t SMon_VfbT30;
extern float SMon_NTC_Temperature_L1;
extern float SMon_ISenseL1_Float;
extern float SMon_McuTempValue;

Dem_FreezeFrame_t Dem_FF[DEM_MAX_FF_DTC];

const uint32_t Dem_PreDefined_DTC_Table[DEM_MAX_FF_DTC] =
{
		0x50, // UV KL30
		0x51, // OV KL30
		0x52, // SHORT TO BATT L1 P1
		0x53, // OVERCURRENT L1
		0x54, // L1 LOCKED
		0x55, // CLS FAILURE L1
		0x56, // UV L1
		0x57, // MCU RESET
		0x58, // NTC ERROR
		0x59, // EXTERNAL CHARGER DETECTED
		0x5A, // LOAD REQUEST MSG MISSING
		0x5B, // LOAD REQUEST E2E ERROR
		0x5C, // SIGNAL INVALID LOAD REQUEST
		0x5D, // VEHICLE DATA TIMEOUT
		0x5E, // VEHICLE STATUS INVALID
		0x5F, // SHORT TO BATT L1 P2
		0x60, // SHORT TO BATT L1 P3
		0x61, // XCP USED
		0x62, // SA USED
		0x63, // SHORT TO BATT L1 ON
};

uint32_t Dem_DTC_Stat[DEM_MAX_FF_DTC] = {
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50,
		0x50
};

void Dem_SetDtc(uint32_t id, uint8_t stat);
void Dem_CaptureFreezeFrame(uint8_t idx);
uint8_t Dem_GetDtcStatus(uint32_t id);

void Dem_CaptureFreezeFrame(uint8_t idx)
{
	Dem_FF[idx].occurrenceCnt++;
	Dem_FF[idx].year = CSBSDAT_Year;
	Dem_FF[idx].month = CSBSDAT_Month;
	Dem_FF[idx].day = CSBSDAT_Day;
	Dem_FF[idx].hour = CSBSDAT_Hour;
	Dem_FF[idx].minute = CSBSDAT_Minute;
	Dem_FF[idx].second = CSBSDAT_Second;
	Dem_FF[idx].L1VFB = SMon_VfbL1;
	Dem_FF[idx].T30VFB = SMon_VfbT30;
	Dem_FF[idx].L1NTC = SMon_NTC_Temperature_L1;
	Dem_FF[idx].L1ISENSE = SMon_ISenseL1_Float;

	Nvm_WriteBlock(2u, &Dem_FF[0].occurrenceCnt);   // separate FF block
}

uint8_t Dem_GetDtcStatus(uint32_t id)
{
	uint8_t index = 0;
	uint8_t aux_index = 0xFFu;

	for(index = 0; index < DEM_MAX_FF_DTC; index++)
	{
		if(id == Dem_PreDefined_DTC_Table[index])
		{
			aux_index = index;
			break;
		}
		else
		{
			/* Do nothing. */
		}
	}

	if(aux_index >= DEM_MAX_FF_DTC)
	{
		return 0;
	}
	else
	{
		/* Do nothing. */
	}

	return Dem_DTC_Stat[aux_index];
}

void Dem_SetDtc(uint32_t id, uint8_t stat)
{
	uint8_t index = 0;
	uint8_t aux_index = 0xFFu;
	uint8_t prevStat = 0;

	for(index = 0; index < DEM_MAX_FF_DTC; index++)
	{
		if(id == Dem_PreDefined_DTC_Table[index])
		{
			aux_index = index;
			break;
		}
		else
		{
			/* Do nothing. */
		}
	}

	if(0xff != aux_index && 0u == Dcm_CDTCS)
	{
		prevStat = Dem_DTC_Stat[aux_index];

		if(stat != Dem_DTC_Stat[aux_index] && stat == 0x2fu)
		{
			Dem_DTC_Stat[aux_index] = stat;
			Nvm_WriteBlock(1u, &Dem_DTC_Stat[0u]);
			Dem_CaptureFreezeFrame(aux_index);
		}
		else
		{
			/* Do nothing. */
		}

		Dem_DTC_Stat[aux_index] = stat;

		if(0x2f == stat && 0x2f != prevStat && FULL_COMMUNICATION == CanH_CommunicationState)
		{
			static uint8_t arrtx[4];
			CAN_TxHeaderTypeDef TxHeader = {0u, 0u, 0u, 0u, 0u, 0u};
			uint32_t TxMailbox = 0u;

			arrtx[0u] = (uint8_t)(Dem_PreDefined_DTC_Table[aux_index]);
			arrtx[1u] = (uint8_t)(Dem_PreDefined_DTC_Table[aux_index] >> 8u);
			arrtx[2u] = ((uint8_t)(Dem_PreDefined_DTC_Table[aux_index] >> 16u) & 0x3Fu);
			arrtx[2u] = (((uint8_t)(Dem_PreDefined_DTC_Table[aux_index] >> 24u) & 0x03u) << 6u);
			arrtx[3u] = (((uint8_t)(Dem_PreDefined_DTC_Table[aux_index] >> 24u) >> 2u) & 0x03u );
			arrtx[3u] = ((stat & 0x3fu) << 2u);

			TxHeader.DLC = 4;
			TxHeader.StdId = 0x7FEu;

			HAL_StatusTypeDef st;

			do
			{
				st = HAL_CAN_AddTxMessage(&hcan, &TxHeader, arrtx, &TxMailbox);
			} while (st != HAL_OK);   // wait until a mailbox frees up

			for(uint8_t i = 0; i < 4; i++) arrtx[i] = 0;
			TxHeader.DLC = 0;
			TxHeader.StdId = 0;
		}
	}
	else
	{
		/* Do nothing. */
	}
}
