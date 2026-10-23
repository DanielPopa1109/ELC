#include "CanH.h"
#include "Dem.h"
#include "SMon.h"
#include "EcuM.h"
#include "Dcm.h"
#include "../Com/Com.h"
#include "../CanIf/CanIf.h"
#include "../CanNm/CanNm.h"
#include "../CanSM/CanSM.h"
#include "../ComM/ComM.h"
#include "../Can/Can.h"

Xcp_State_t Xcp_State = {0u, 0u, 0u, 0u};
uint8_t Xcp_TxData[8u];
CAN_TxHeaderTypeDef Xcp_TxHeader;
CanH_ComStat_t CanH_CommunicationState = PARTIAL_COMMUNICATION;
uint8_t CanH_NM3PN1_Value = 0u;
uint8_t CanH_RxData[8u] = {0u};
uint8_t CanH_TxData[8u] = {0u};
uint8_t CSBSDAT_Year = 0u;
uint8_t CSBSDAT_Month = 0u;
uint8_t CSBSDAT_Day = 0u;
uint8_t CSBSDAT_Hour = 0u;
uint8_t CSBSDAT_Minute = 0u;
uint8_t CSBSDAT_Second = 0u;
volatile uint8_t CanH_VehicleStatus = 0u;
volatile uint8_t CanH_AliveCounter_LoadRequest = 0u;
CAN_RxHeaderTypeDef CanH_RxHeader = {0u, 0u, 0u, 0u, 0u, 0u, 0u};
CAN_TxHeaderTypeDef CanH_TxHeader = {0u, 0u, 0u, 0u, 0u, 0u};
uint32_t CanH_MainCounter = 0u;
uint32_t CanH_TxMailbox = 0u;
uint32_t CanH_NoCommCounter = 0u;
uint32_t CanH_MissingLoadRequest = 0u;
uint32_t CanH_MissingVehicleData = 0u;
uint32_t CanH_E2eErrCnt = 0u;
const uint32_t CanH_P_DelayTxParam = 1200u;
static uint8_t loadRequestSeenInFullCom = 0u;
static uint8_t vehicleStatusSeenInFullCom = 0u;

static struct {
    SMon_BatteryData_t battery;uint16_t l1,t30;uint32_t i2t;
    float current,ntc,mcu;
} measurements;
void CanH_PublishMeasurements(void)
{
    uint32_t mask=__get_PRIMASK();__disable_irq();
    measurements.battery=SMon_Battery;measurements.l1=SMon_VfbL1;measurements.t30=SMon_VfbT30;
    measurements.i2t=SMon_I2TCounter;measurements.current=SMon_ISenseL1_Float;
    measurements.ntc=SMon_NTC_Temperature_L1;measurements.mcu=SMon_McuTempValue;
    __set_PRIMASK(mask);
}
void CanH_MainFunction(void);

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan);
static void Xcp_HandleCommand(const uint8_t *data, uint8_t len);
static bool Xcp_CanRx(const CAN_RxHeaderTypeDef *rh, const uint8_t *data);

static bool Xcp_AddrAllowed(uint32_t addr, uint32_t len)
{
	const uint32_t RAM_START = 0x20000000u;
	const uint32_t RAM_ENDp1 = 0x20005000u;

	if (addr < RAM_START || addr >= RAM_ENDp1)
	{
		return false;
	}
	else
	{
		// do nothing.
	}

	if (len > (RAM_ENDp1 - addr))
	{
		return false;
	}
	else
	{
		// do nothing.
	}

	return true;
}

static void Xcp_Send(uint8_t len)
{
    (void)CanIf_TransmitXcp(Xcp_TxData,len);
}

static void Xcp_SendError(uint8_t err)
{
	Xcp_TxData[0] = XCP_PID_ERR;
	Xcp_TxData[1] = err;
	Xcp_TxData[2] = 0u;
	Xcp_TxData[3] = 0u;
	Xcp_TxData[4] = 0u;
	Xcp_TxData[5] = 0u;
	Xcp_TxData[6] = 0u;
	Xcp_TxData[7] = 0u;

	Xcp_Send(8u);
}

static void Xcp_HandleCommand(const uint8_t *data, uint8_t len)
{
	const uint8_t pid = data[0];

	if (len == 0u)
	{
		return;
	}
	else
	{
		// do nothing.
	}

	switch (pid)
	{

	case XCP_CMD_CONNECT:
	{

		Xcp_State.connected     = 1u;
		Xcp_State.sessionStatus = 1u;

		/* CONNECT response:
		 * byte0: RES PID (0xFF)
		 * byte1: resource availability (0x01 = CAL/PAG only)
		 * byte2: comm mode (0x80 = Intel byte order)
		 * byte3: max CTO (8)
		 * byte4-5: max DTO (8)
		 * byte6: protocol layer version (1)
		 * byte7: transport layer version (1)
		 */

		Xcp_TxData[0] = XCP_PID_RES;
		Xcp_TxData[1] = 0x01u; /* CAL/PAG only, no DAQ/STIM/PGM */
		Xcp_TxData[2] = 0x80u; /* Intel byte order */
		Xcp_TxData[3] = 8u;    /* MAX_CTO */
		Xcp_TxData[4] = 8u;    /* MAX_DTO (low byte) */
		Xcp_TxData[5] = 0u;    /* MAX_DTO (high byte) */
		Xcp_TxData[6] = 0x01u; /* XCP protocol version 1.0 */
		Xcp_TxData[7] = 0x01u; /* transport layer version 1.0 */

		Xcp_Send(8u);

		Dem_SetDtc(0x61u, 0x2fu);

		break;
	}

	case XCP_CMD_DISCONNECT:
	{

		Xcp_State.connected     = 0u;
		Xcp_State.sessionStatus = 0u;

		Xcp_TxData[0] = XCP_PID_RES;
		Xcp_TxData[1] = XCP_ERR_OK;
		Xcp_TxData[2] = 0u;
		Xcp_TxData[3] = 0u;
		Xcp_TxData[4] = 0u;
		Xcp_TxData[5] = 0u;
		Xcp_TxData[6] = 0u;
		Xcp_TxData[7] = 0u;
		Xcp_Send(8u);

		if(0x2f == Dem_GetDtcStatus(0x61u))
		{
			Dem_SetDtc(0x61u, 0x2eu);
		}
		else
		{
			/* Do nothing. */
		}

		break;
	}

	case XCP_CMD_GET_STATUS:
	{

		/* GET_STATUS response:
		 * byte0: RES PID
		 * byte1: session status
		 * byte2: protection status
		 * byte3-4: reserved
		 * byte5-6: session config ID
		 * byte7: reserved
		 */
		Xcp_TxData[0] = XCP_PID_RES;
		Xcp_TxData[1] = Xcp_State.sessionStatus;
		Xcp_TxData[2] = 0x00u; /* no protection */
		Xcp_TxData[3] = 0x00u;
		Xcp_TxData[4] = 0x00u;
		Xcp_TxData[5] = 0x00u; /* sessionConfigID LSB */
		Xcp_TxData[6] = 0x00u; /* sessionConfigID MSB */
		Xcp_TxData[7] = 0x00u;
		Xcp_Send(8u);

		break;
	}
	case XCP_CMD_SET_MTA:
	{

		/* Layout commonly used on CAN:
		 * byte0: 0xF6
		 * byte1: reserved
		 * byte2: addrExt
		 * byte3-6: address (LE)
		 * byte7: don't care
		 */

		if (len < 7u)
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);

			break;
		}
		else
		{
			// do nothing.
		}

		Xcp_State.mtaExt = data[2];
		Xcp_State.mta =  (uint32_t)data[3]
										| ((uint32_t)data[4] << 8)
										| ((uint32_t)data[5] << 16)
										| ((uint32_t)data[6] << 24);

		if (Xcp_State.mtaExt != 0u)
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);

			break;
		}
		else
		{
			// do nothing.
		}

		if (!Xcp_AddrAllowed(Xcp_State.mta, 1u))
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);

			break;
		}
		else
		{
			// do nothing.
		}

		Xcp_TxData[0] = XCP_PID_RES;

		for (uint8_t i = 1u; i < 8u; i++)
		{
			Xcp_TxData[i] = 0u;
		}

		Xcp_Send(8u);

		break;
	}

	case XCP_CMD_UPLOAD:
	{

		uint8_t n = data[1];
		/* byte0: 0xF5, byte1: n */
		if (len < 2u)
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);

			break;
		}
		else
		{
			// do nothing.
		}

		if ((n == 0u) || (n > 7u))
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);

			break;
		}
		else
		{
			// do nothing.
		}

		if (Xcp_State.mtaExt != 0u)
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);

			break;
		}
		else
		{
			// do nothing.
		}

		if (!Xcp_AddrAllowed(Xcp_State.mta, (uint32_t)n))
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);

			break;
		}
		else
		{
			// do nothing.
		}

		volatile uint8_t *p = (volatile uint8_t *)Xcp_State.mta;

		Xcp_TxData[0] = XCP_PID_RES;

		for (uint8_t i = 0u; i < n; i++)
		{
			Xcp_TxData[1u + i] = p[i];
		}

		for (uint8_t i = (uint8_t)(1u + n); i < 8u; i++)
		{
			Xcp_TxData[i] = 0u;
		}

		Xcp_State.mta += (uint32_t)n; /* auto-increment */
		Xcp_Send((uint8_t)(1u + n));

		break;
	}


	case XCP_CMD_SHORT_UPLOAD:
	{
		/* SHORT_UPLOAD request layout (CTO, 8 bytes):
		 * byte0: PID = 0xF4
		 * byte1: number of data bytes (N)
		 * byte2: reserved
		 * byte3: address extension (segment) – only 0 supported here
		 * byte4-7: address (little endian, Intel order)
		 */

		uint8_t n = data[1];

		if (len < 8u)
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);

			break;
		}
		else
		{
			// do nothing.
		}

		if ((n == 0u) || (n > 7u))
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);

			break;
		}
		else
		{
			// do nothing.
		}

		uint8_t addrExt = data[3];

		if (addrExt != 0u)
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);

			break;
		}
		else
		{
			// do nothing.
		}

		uint32_t addr =  (uint32_t)data[4]
										| ((uint32_t)data[5] << 8)
										| ((uint32_t)data[6] << 16)
										| ((uint32_t)data[7] << 24);

		if (!Xcp_AddrAllowed(addr, (uint32_t)n))
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);

			break;
		}
		else
		{
			// do nothing.
		}

		volatile uint8_t *p = (volatile uint8_t *)addr;

		Xcp_TxData[0] = XCP_PID_RES;

		for (uint8_t i = 0u; i < n; i++)
		{
			Xcp_TxData[1u + i] = p[i];
		}

		for (uint8_t i = (uint8_t)(1u + n); i < 8u; i++)
		{
			Xcp_TxData[i] = 0u;
		}

		Xcp_Send((uint8_t)(1u + n));

		break;
	}

	case XCP_CMD_GET_COMM_MODE_INFO:
	{
		/* Positive response to GET_COMM_MODE_INFO (0xFB)
		 * byte0: RES PID (0xFF)
		 * byte1: reserved
		 * byte2: COMM_MODE_OPTIONAL (all 0 -> no master/slave block, etc.)
		 * byte3: reserved
		 * byte4: MAX_BS     (0 -> no master block mode)
		 * byte5: MIN_ST     (0 -> no min separation time)
		 * byte6: QUEUE_SIZE (0 -> no TX queue info)
		 * byte7: driver version (0x10 = v1.0)
		 */
		Xcp_TxData[0] = XCP_PID_RES;
		Xcp_TxData[1] = 0x00u;
		Xcp_TxData[2] = 0x00u;  /* COMM_MODE_OPTIONAL: all flags 0 */
		Xcp_TxData[3] = 0x00u;
		Xcp_TxData[4] = 0x00u;  /* MAX_BS */
		Xcp_TxData[5] = 0x00u;  /* MIN_ST */
		Xcp_TxData[6] = 0x00u;  /* QUEUE_SIZE */
		Xcp_TxData[7] = 0x10u;  /* driver version 1.0 */

		Xcp_Send(8u);

		break;
	}

	case XCP_CMD_GET_ID:
	{
		/* Tool asks for ECU/A2L identification.
		 * We answer "type supported but no ID string" (length = 0).
		 * That’s valid and most tools accept it.
		 */
		if (len < 2u)
		{
			Xcp_SendError(XCP_ERR_OUT_OF_RANGE);
			break;
		}
		else
		{
			// do nothing.
		}
		/* uint8_t reqType = data[1]; */ /* currently unused */

		/* Positive response:
		 * byte0: RES PID
		 * byte1: MODE = 1 (ID in CTO)
		 * byte2-3: reserved
		 * byte4-7: Length = 0 (no ID bytes)
		 */
		Xcp_TxData[0] = XCP_PID_RES;
		Xcp_TxData[1] = 0x01u;  /* MODE = 1 */
		Xcp_TxData[2] = 0x00u;
		Xcp_TxData[3] = 0x00u;
		Xcp_TxData[4] = 0x00u;  /* length = 0 (DWORD) */
		Xcp_TxData[5] = 0x00u;
		Xcp_TxData[6] = 0x00u;
		Xcp_TxData[7] = 0x00u;

		Xcp_Send(8u);

		break;
	}


	default:
	{

		Xcp_SendError(XCP_ERR_CMD_UNKNOWN);

		break;
	}

	}
}

static bool Xcp_CanRx(const CAN_RxHeaderTypeDef *rh, const uint8_t *data)
{
	if ((rh->IDE != CAN_ID_STD) || (rh->StdId != XCP_CAN_RX_ID))
	{
		return false;
	}
	else
	{
		// do nothing.
	}

	Xcp_HandleCommand(data, rh->DLC);

	return true;
}

void CanH_MainFunction(void)
{
	uint8_t loadRequest;
	uint8_t vehicleStatus;
	CanNm_MainFunction();
	ComM_MainFunction();
	CanSM_MainFunction();
	CanH_NM3PN1_Value = CanNm_GetPn1();
	CanH_NoCommCounter = CanNm_GetInactiveTicks();
	CanH_CommunicationState = (CanSM_GetCurrentComMode() == CANSM_FULL_COMMUNICATION) ?
		FULL_COMMUNICATION : NO_COMMUNICATION;
	Com_SetLoadStatus(SMon_L1ST, (FULL_COMMUNICATION == CanH_CommunicationState && 0u == (Dcm_CC&1u)) ? 1u : 0u);
	Com_MainFunctionTx();
	Com_MainFunctionRx();
	if ((CanH_CommunicationState == FULL_COMMUNICATION) && Dcm_CC<2u &&
	    (Com_GetLoadRequest(&loadRequest) == 0))
	{
		SMon_CmdStat = loadRequest;
		CanH_MissingLoadRequest = 0u;
		loadRequestSeenInFullCom = 1u;
	}
	else
	{
		SMon_CmdStat = 0u;
		if (CanH_CommunicationState == FULL_COMMUNICATION)
		{
			if ((loadRequestSeenInFullCom != 0u) &&
			    (Com_LoadRequestTimedOut() != 0u))
				CanH_MissingLoadRequest = 201u;
			else if (CanH_MissingLoadRequest < 201u)
				++CanH_MissingLoadRequest;
		}
		else
		{
			CanH_MissingLoadRequest = 0u;
			loadRequestSeenInFullCom = 0u;
		}
	}
	if (Com_GetVehicleStatus(&vehicleStatus) == 0)
	{
		CanH_VehicleStatus = vehicleStatus;
		CanH_MissingVehicleData = 0u;
		if (CanH_CommunicationState == FULL_COMMUNICATION)
			vehicleStatusSeenInFullCom = 1u;
		else
			vehicleStatusSeenInFullCom = 0u;
	}
	else
	{
		CanH_VehicleStatus = 0u;
		if (CanH_CommunicationState == FULL_COMMUNICATION)
		{
			if ((vehicleStatusSeenInFullCom != 0u) &&
			    (Com_VehicleStatusTimedOut() != 0u))
				CanH_MissingVehicleData = 201u;
			else if (CanH_MissingVehicleData < 201u)
				++CanH_MissingVehicleData;
		}
		else
		{
			CanH_MissingVehicleData = 0u;
			vehicleStatusSeenInFullCom = 0u;
		}
	}
	if (Com_HasInvalidVehicleStatus() != 0u)
		Dem_SetDtc(0x5Eu, 0x2fu);
	else if (Dem_GetDtcStatus(0x5Eu) == 0x2fu)
		Dem_SetDtc(0x5Eu, 0x2eu);
	CanH_E2eErrCnt = Com_GetInvalidFrameCount();

	if(FULL_COMMUNICATION == CanH_CommunicationState && 0u == (Dcm_CC&1u))
	{
		if(CanH_MainCounter % 40u == 0u && (CanH_NM3PN1_Value & ((1u << 6u))))
		{
			memcpy(&CanH_TxData[0u], &measurements.battery.Charge_Ah_Acc, sizeof(float));
			memcpy(&CanH_TxData[4u], &measurements.battery.Discharge_Ah_Acc, sizeof(float));

			CanH_TxHeader.DLC = 8;
			CanH_TxHeader.StdId = 0x6f1;

			HAL_StatusTypeDef st;
			uint32_t cnt = 0u;

			do
			{
				st = Com_QueueTelemetry((uint16_t)CanH_TxHeader.StdId, CanH_TxData, (uint8_t)CanH_TxHeader.DLC);
				cnt++;
			} while (st != HAL_OK  && cnt < CanH_P_DelayTxParam);

			cnt = 0u;

			memcpy(&CanH_TxData[0u], &measurements.battery.SoC_CC_pct, sizeof(float));
			memcpy(&CanH_TxData[4u], &measurements.battery.SoC_OCV_pct, sizeof(float));

			CanH_TxHeader.DLC = 8;
			CanH_TxHeader.StdId = 0x6f2;

			do
			{
				st = Com_QueueTelemetry((uint16_t)CanH_TxHeader.StdId, CanH_TxData, (uint8_t)CanH_TxHeader.DLC);
				cnt++;
			} while (st != HAL_OK  && cnt < CanH_P_DelayTxParam);

			cnt = 0u;

			memcpy(&CanH_TxData[0u], &measurements.battery.SoC_pct, sizeof(float));
			memcpy(&CanH_TxData[4u], &measurements.battery.SoH_pct, sizeof(float));

			CanH_TxHeader.DLC = 8;
			CanH_TxHeader.StdId = 0x6f3;

			do
			{
				st = Com_QueueTelemetry((uint16_t)CanH_TxHeader.StdId, CanH_TxData, (uint8_t)CanH_TxHeader.DLC);
				cnt++;
			} while (st != HAL_OK  && cnt < CanH_P_DelayTxParam);

			cnt = 0u;

			memcpy(&CanH_TxData[0u], &measurements.battery.AvailableCapacity_Ah, sizeof(float));
			memcpy(&CanH_TxData[4u], &measurements.battery.InternalResistance_Ohm, sizeof(float));

			CanH_TxHeader.DLC = 8;
			CanH_TxHeader.StdId = 0x6f4;

			do
			{
				st = Com_QueueTelemetry((uint16_t)CanH_TxHeader.StdId, CanH_TxData, (uint8_t)CanH_TxHeader.DLC);
				cnt++;
			} while (st != HAL_OK  && cnt < CanH_P_DelayTxParam);

			cnt = 0u;

			memcpy(&CanH_TxData[0u], &measurements.battery.AvgChargeCurrent_A, sizeof(float));
			memcpy(&CanH_TxData[4u], &measurements.battery.AvgDischargeCurrent_A, sizeof(float));

			CanH_TxHeader.DLC = 8;
			CanH_TxHeader.StdId = 0x6f5;

			do
			{
				st = Com_QueueTelemetry((uint16_t)CanH_TxHeader.StdId, CanH_TxData, (uint8_t)CanH_TxHeader.DLC);
				cnt++;
			} while (st != HAL_OK  && cnt < CanH_P_DelayTxParam);

			cnt = 0u;

			memcpy(&CanH_TxData[0u], &measurements.battery.RuntimeRemaining_h, sizeof(float));
			memcpy(&CanH_TxData[4u], &measurements.battery.TimeToFull_h, sizeof(float));

			CanH_TxHeader.DLC = 8;
			CanH_TxHeader.StdId = 0x6f6;

			do
			{
				st = Com_QueueTelemetry((uint16_t)CanH_TxHeader.StdId, CanH_TxData, (uint8_t)CanH_TxHeader.DLC);
				cnt++;
			} while (st != HAL_OK  && cnt < CanH_P_DelayTxParam);

			cnt = 0u;

			/* ZGW BattDiagnosis 0x6F7 is one packed byte, not eight
			 * separate boolean bytes. Bit 2 remains 0: ZGW defines a
			 * boolean but ELC computes a percentage with no agreed mapping.
			 */
			CanH_TxData[0] =
				((measurements.battery.BatteryRested != 0u) ? 0x01u : 0u) |
				((measurements.battery.Charging != 0u) ? 0x02u : 0u) |
				((measurements.battery.CrankingEvent != 0u) ? 0x08u : 0u) |
				((measurements.battery.DeepDischarge != 0u) ? 0x10u : 0u) |
				((measurements.battery.Discharging != 0u) ? 0x20u : 0u) |
				((measurements.battery.ValidOcvCalibration != 0u) ? 0x40u : 0u) |
				((measurements.battery.WeakBattery != 0u) ? 0x80u : 0u);

			CanH_TxHeader.DLC = 1;
			CanH_TxHeader.StdId = 0x6f7;

			do
			{
				st = Com_QueueTelemetry((uint16_t)CanH_TxHeader.StdId, CanH_TxData, (uint8_t)CanH_TxHeader.DLC);
				cnt++;
			} while (st != HAL_OK  && cnt < CanH_P_DelayTxParam);

			cnt = 0u;

			memset(CanH_TxData, 0u, sizeof(CanH_TxData));
		}

		if(CanH_MainCounter != 0 && (CanH_NM3PN1_Value & ((1u << 6))))
		{
			CanH_TxData[0] = (uint8_t)(measurements.t30);
			CanH_TxData[1] = (uint8_t)(measurements.t30 >> 8u);
			CanH_TxData[2] = (uint8_t)(measurements.l1);
			CanH_TxData[3] = (uint8_t)(measurements.l1 >> 8u);

			memcpy(&CanH_TxData[4u], &measurements.current, sizeof(float));

			CanH_TxHeader.DLC = 8;
			CanH_TxHeader.StdId = 0x6ef;

			HAL_StatusTypeDef st;
			uint32_t cnt = 0u;

			do
			{
				st = Com_QueueTelemetry((uint16_t)CanH_TxHeader.StdId, CanH_TxData, (uint8_t)CanH_TxHeader.DLC);
				cnt++;
			} while (st != HAL_OK  && cnt < CanH_P_DelayTxParam);

			cnt = 0u;

			for(uint8_t i = 0; i < 8; i++)
			{
				CanH_TxData[i] = 0;
			}

			CanH_TxHeader.DLC = 0;
			CanH_TxHeader.StdId = 0;
		}
		else
		{
			/* Do nothing. */
		}

		if(CanH_MainCounter != 0 && (CanH_NM3PN1_Value & ((1u << 6))))
		{
			memcpy(&CanH_TxData[0u], &measurements.ntc, sizeof(float));
			memcpy(&CanH_TxData[4u], &measurements.mcu, sizeof(float));

			CanH_TxHeader.DLC = 8;
			CanH_TxHeader.StdId = 0x6ee;

			HAL_StatusTypeDef st;
			uint32_t cnt = 0u;

			do
			{
				st = Com_QueueTelemetry((uint16_t)CanH_TxHeader.StdId, CanH_TxData, (uint8_t)CanH_TxHeader.DLC);
				cnt++;
			} while (st != HAL_OK  && cnt < CanH_P_DelayTxParam);

			cnt = 0u;

			for(uint8_t i = 0; i < 8; i++)
			{
				CanH_TxData[i] = 0;
			}

			CanH_TxHeader.DLC = 0;
			CanH_TxHeader.StdId = 0;
		}
		else
		{
			/* Do nothing. */
		}

		if(CanH_MainCounter != 0 && (CanH_NM3PN1_Value & ((1u << 6))))
		{
			CanH_TxData[0] = (uint8_t)(measurements.i2t);
			CanH_TxData[1] = (uint8_t)(measurements.i2t >> 8u);
			CanH_TxData[2] = (uint8_t)(measurements.i2t >> 16u);
			CanH_TxData[3] = (uint8_t)(measurements.i2t >> 24u);
			CanH_TxHeader.DLC = 4;
			CanH_TxHeader.StdId = 0x6f0;

			HAL_StatusTypeDef st;
			uint32_t cnt = 0u;

			do
			{
				st = Com_QueueTelemetry((uint16_t)CanH_TxHeader.StdId, CanH_TxData, (uint8_t)CanH_TxHeader.DLC);
				cnt++;
			} while (st != HAL_OK  && cnt < CanH_P_DelayTxParam);

			cnt = 0u;

			for(uint8_t i = 0; i < 8; i++)
			{
				CanH_TxData[i] = 0;
			}

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

	CanH_MainCounter++;

	if(200u < CanH_MissingLoadRequest)
	{
		SMon_CmdStat = 0u;
		Dem_SetDtc(0x5Au, 0x2f);
	}
	else
	{
		if(0x2f == Dem_GetDtcStatus(0x5Au))
		{
			Dem_SetDtc(0x5Au, 0x2e);
		}
		else
		{
			/* Do nothing. */
		}
	}

	if(200u < CanH_MissingVehicleData)
	{
		Dem_SetDtc(0x5Du, 0x2f);
	}
	else
	{
		if(0x2f == Dem_GetDtcStatus(0x5Du))
		{
			Dem_SetDtc(0x5Du, 0x2e);
		}
		else
		{
			/* Do nothing. */
		}
	}

	if(50u < CanH_E2eErrCnt)
	{
		Dem_SetDtc(0x5Bu, 0x2f);
	}
	else
	{
		if(0x2f == Dem_GetDtcStatus(0x5Bu))
		{
			Dem_SetDtc(0x5Bu, 0x2e);
		}
		else
		{
			/* Do nothing. */
		}
	}

	for(uint8_t i = 0; i < 8u; i++)
	{
		CanH_TxData[i] = 0u;
	}
}

static void CanH_Receive(CAN_HandleTypeDef *handle,uint32_t fifo)
{
    CAN_RxHeaderTypeDef rh;uint8_t data[8];
    while(HAL_CAN_GetRxFifoFillLevel(handle,fifo)) {
        if(HAL_CAN_GetRxMessage(handle,fifo,&rh,data)!=HAL_OK)break;
        if(rh.IDE==CAN_ID_STD && rh.RTR==CAN_RTR_DATA && rh.DLC<=8u)
            Can_QueueRx((uint16_t)rh.StdId,data,(uint8_t)rh.DLC);
    }
}
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan){CanH_Receive(hcan,CAN_RX_FIFO0);}
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan){CanH_Receive(hcan,CAN_RX_FIFO1);}
void CanH_RxIndication(uint16_t id,const uint8_t *data,uint8_t len)
{
    if(id==0x202u && len>=4u){
        uint32_t t=(uint32_t)data[0]|((uint32_t)data[1]<<8)|((uint32_t)data[2]<<16)|((uint32_t)data[3]<<24);
        CSBSDAT_Year=t&63u;CSBSDAT_Month=(t>>6)&15u;CSBSDAT_Day=(t>>10)&31u;
        CSBSDAT_Hour=(t>>15)&31u;CSBSDAT_Minute=(t>>20)&63u;CSBSDAT_Second=(t>>26)&63u;
    }
    if(FULL_COMMUNICATION==CanH_CommunicationState && Dcm_CC<2u){
        CAN_RxHeaderTypeDef rh={0};rh.StdId=id;rh.DLC=len;rh.IDE=CAN_ID_STD;
        Xcp_CanRx(&rh,data);
    }
}
