#include "CanH.h"
#include "Dem.h"
#include "SMon.h"
#include "main.h"
#include "../Com/Com.h"
#include "../CanIf/CanIf.h"
#include "../CanNm/CanNm.h"
#include "../ComM/ComM.h"
#include <string.h>

Xcp_State_t Xcp_State = {0u, 0u, 0u, 0u};
uint8_t Xcp_TxData[8u];
CanH_ComStat_t CanH_CommunicationState = PARTIAL_COMMUNICATION;
uint8_t CanH_NM3PN1_Value = 0u;
uint8_t CSBSDAT_Year = 0u;
uint8_t CSBSDAT_Month = 0u;
uint8_t CSBSDAT_Day = 0u;
uint8_t CSBSDAT_Hour = 0u;
uint8_t CSBSDAT_Minute = 0u;
uint8_t CSBSDAT_Second = 0u;
volatile uint8_t CanH_VehicleStatus = 0u;
volatile uint8_t CanH_AliveCounter_LoadRequest = 0u;
uint32_t CanH_MainCounter = 0u;
uint32_t CanH_NoCommCounter = 0u;
uint32_t CanH_MissingLoadRequest = 0u;
uint32_t CanH_MissingVehicleData = 0u;
uint32_t CanH_E2eErrCnt = 0u;
static uint8_t loadRequestSeenInFullCom = 0u;
static uint8_t vehicleStatusSeenInFullCom = 0u;

/* XCP response transmission: one outstanding response, retried until the CAN TX
 * confirmation arrives (lost confirmations re-arm after XCP_TX_TIMEOUT_MS). */
#define XCP_TX_TIMEOUT_MS 150u
static uint8_t Xcp_TxLength;
static volatile uint8_t Xcp_TxPending;
static volatile uint8_t Xcp_TxInFlight;
static uint8_t Xcp_TxTimer;

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

static void Xcp_HandleCommand(const uint8_t *data, uint8_t len);

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
	/* XCP is request/response: one response in flight. Gated like all application traffic. */
	if ((ComM_IsTxAllowed(COMM_CHANNEL_CAN) == 0u) || (Xcp_TxPending != 0u) || (len == 0u) || (len > 8u))
	{
		return;
	}
	Xcp_TxLength = len;
	Xcp_TxPending = 1u;
}

/* Called every 1 ms from the diagnostic task (same context as the CAN driver main functions). */
void Xcp_MainFunction(void)
{
	PduInfoType info;

	if (Xcp_TxInFlight != 0u)
	{
		if ((Xcp_TxTimer != 0u) && (--Xcp_TxTimer == 0u))
		{
			Xcp_TxInFlight = 0u;     /* confirmation lost: retransmit */
		}
		return;
	}
	if ((Xcp_TxPending != 0u) && (ComM_IsTxAllowed(COMM_CHANNEL_CAN) != 0u))
	{
		info.SduDataPtr = Xcp_TxData;
		info.MetaDataPtr = NULL_PTR;
		info.SduLength = Xcp_TxLength;
		if (CanIf_Transmit(CANIF_TXPDU_XCP_RES, &info) == E_OK)
		{
			Xcp_TxInFlight = 1u;
			Xcp_TxTimer = (uint8_t)XCP_TX_TIMEOUT_MS;
		}
	}
}

void CanH_TxConfirmation(PduIdType TxPduId)
{
	if (TxPduId == CANIF_TXPDU_XCP_RES)
	{
		Xcp_TxPending = 0u;
		Xcp_TxInFlight = 0u;
	}
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

		(void)Dem_SetEventStatus(DEM_EVENT_XCP_USED, DEM_EVENT_STATUS_FAILED);

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

		if(Dem_IsEventFailed(DEM_EVENT_XCP_USED) != 0u)
		{
			(void)Dem_SetEventStatus(DEM_EVENT_XCP_USED, DEM_EVENT_STATUS_PASSED);
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

/* Telemetry signal n is CAN 0x6EE + n. */
static void CanH_SendTelemetry(uint8_t n, const uint8_t *data)
{
	(void)Com_SendSignal((Com_SignalIdType)(COM_SIGNAL_TELEMETRY_FIRST + n), data);
}

static void CanH_SendFloatPair(uint8_t n, float first, float second)
{
	uint8_t data[8];

	(void)memcpy(&data[0], &first, sizeof(float));
	(void)memcpy(&data[4], &second, sizeof(float));
	CanH_SendTelemetry(n, data);
}

/* Event reporting helper: failed while the condition holds, passed once it clears. */
static void CanH_ReportEvent(Dem_EventIdType event, boolean failed)
{
	if (failed != 0u)
	{
		(void)Dem_SetEventStatus(event, DEM_EVENT_STATUS_FAILED);
	}
	else if (Dem_IsEventFailed(event) != 0u)
	{
		(void)Dem_SetEventStatus(event, DEM_EVENT_STATUS_PASSED);
	}
}

void CanH_Init(void)
{
	Xcp_TxPending = 0u;
	Xcp_TxInFlight = 0u;
}

void CanH_MainFunction(void)
{
	uint8_t loadRequest = 0u;
	uint8_t vehicleStatus = 0u;
	uint8_t loadStatus;
	ComM_ModeType mode = COMM_SILENT_COMMUNICATION;
	boolean fullCom;

	(void)ComM_GetCurrentComMode(COMM_CHANNEL_CAN, &mode);
	fullCom = (mode == COMM_FULL_COMMUNICATION) ? TRUE : FALSE;
	CanH_NM3PN1_Value = CanNm_GetPn1();
	CanH_NoCommCounter = CanNm_GetInactiveTicks();
	CanH_CommunicationState = (fullCom != 0u) ? FULL_COMMUNICATION : NO_COMMUNICATION;

	loadStatus = SMon_L1ST;
	(void)Com_SendSignal(COM_SIGNAL_LOADSTATUS, &loadStatus);

	if ((fullCom != 0u) && (ComM_IsRxAllowed(COMM_CHANNEL_CAN) != 0u) &&
	    (Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &loadRequest) == E_OK))
	{
		SMon_CmdStat = loadRequest;
		CanH_MissingLoadRequest = 0u;
		loadRequestSeenInFullCom = 1u;
	}
	else
	{
		SMon_CmdStat = 0u;
		if (fullCom != 0u)
		{
			if ((loadRequestSeenInFullCom != 0u) &&
			    ((Com_GetRxStatus(COM_SIGNAL_LOADREQUEST) & COM_RXSTATUS_TIMEOUT) != 0u))
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
	if (Com_ReceiveSignal(COM_SIGNAL_VEHICLESTATE, &vehicleStatus) == E_OK)
	{
		CanH_VehicleStatus = vehicleStatus;
		CanH_MissingVehicleData = 0u;
		vehicleStatusSeenInFullCom = (fullCom != 0u) ? 1u : 0u;
	}
	else
	{
		CanH_VehicleStatus = 0u;
		if (fullCom != 0u)
		{
			if ((vehicleStatusSeenInFullCom != 0u) &&
			    ((Com_GetRxStatus(COM_SIGNAL_VEHICLESTATE) & COM_RXSTATUS_TIMEOUT) != 0u))
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
	CanH_ReportEvent(DEM_EVENT_VEHICLESTATUS_INVALID,
	                 ((Com_GetRxStatus(COM_SIGNAL_VEHICLESTATE) & COM_RXSTATUS_INVALID) != 0u) ? TRUE : FALSE);
	CanH_E2eErrCnt = Com_GetE2eErrorCount();

	if (ComM_IsTxAllowed(COMM_CHANNEL_CAN) != 0u)
	{
		const uint8_t networkRequested = ((CanH_NM3PN1_Value & (1u << 6u)) != 0u) ? 1u : 0u;

		if ((CanH_MainCounter % 40u == 0u) && (networkRequested != 0u))
		{
			uint8_t flags;

			CanH_SendFloatPair(3u, measurements.battery.Charge_Ah_Acc, measurements.battery.Discharge_Ah_Acc);   /* 0x6F1 */
			CanH_SendFloatPair(4u, measurements.battery.SoC_CC_pct, measurements.battery.SoC_OCV_pct);           /* 0x6F2 */
			CanH_SendFloatPair(5u, measurements.battery.SoC_pct, measurements.battery.SoH_pct);                  /* 0x6F3 */
			CanH_SendFloatPair(6u, measurements.battery.AvailableCapacity_Ah, measurements.battery.InternalResistance_Ohm); /* 0x6F4 */
			CanH_SendFloatPair(7u, measurements.battery.AvgChargeCurrent_A, measurements.battery.AvgDischargeCurrent_A);    /* 0x6F5 */
			CanH_SendFloatPair(8u, measurements.battery.RuntimeRemaining_h, measurements.battery.TimeToFull_h);              /* 0x6F6 */

			/* ZGW BattDiagnosis 0x6F7 is one packed byte, not eight separate boolean bytes. Bit 2
			 * remains 0: ZGW defines a boolean but ELC computes a percentage with no agreed mapping. */
			flags = (uint8_t)(
				((measurements.battery.BatteryRested != 0u) ? 0x01u : 0u) |
				((measurements.battery.Charging != 0u) ? 0x02u : 0u) |
				((measurements.battery.CrankingEvent != 0u) ? 0x08u : 0u) |
				((measurements.battery.DeepDischarge != 0u) ? 0x10u : 0u) |
				((measurements.battery.Discharging != 0u) ? 0x20u : 0u) |
				((measurements.battery.ValidOcvCalibration != 0u) ? 0x40u : 0u) |
				((measurements.battery.WeakBattery != 0u) ? 0x80u : 0u));
			CanH_SendTelemetry(9u, &flags);                                                                      /* 0x6F7 */
		}

		if ((CanH_MainCounter != 0u) && (networkRequested != 0u))
		{
			uint8_t data[8];

			data[0] = (uint8_t)(measurements.t30);
			data[1] = (uint8_t)(measurements.t30 >> 8u);
			data[2] = (uint8_t)(measurements.l1);
			data[3] = (uint8_t)(measurements.l1 >> 8u);
			(void)memcpy(&data[4], &measurements.current, sizeof(float));
			CanH_SendTelemetry(1u, data);                                                                       /* 0x6EF */

			CanH_SendFloatPair(0u, measurements.ntc, measurements.mcu);                                         /* 0x6EE */

			data[0] = (uint8_t)(measurements.i2t);
			data[1] = (uint8_t)(measurements.i2t >> 8u);
			data[2] = (uint8_t)(measurements.i2t >> 16u);
			data[3] = (uint8_t)(measurements.i2t >> 24u);
			CanH_SendTelemetry(2u, data);                                                                       /* 0x6F0 */
		}
	}

	CanH_MainCounter++;

	if (CanH_MissingLoadRequest > 200u)
	{
		SMon_CmdStat = 0u;
	}
	CanH_ReportEvent(DEM_EVENT_LOADREQUEST_MISSING, (CanH_MissingLoadRequest > 200u) ? TRUE : FALSE);
	CanH_ReportEvent(DEM_EVENT_VEHICLEDATA_TIMEOUT, (CanH_MissingVehicleData > 200u) ? TRUE : FALSE);
	CanH_ReportEvent(DEM_EVENT_LOADREQUEST_E2E, (CanH_E2eErrCnt > 50u) ? TRUE : FALSE);
}

void CanH_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
	const uint8_t *data;

	if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR))
	{
		return;
	}
	data = PduInfoPtr->SduDataPtr;
	if ((RxPduId == CANIF_RXPDU_TIMEDATE) && (PduInfoPtr->SduLength >= 4u))
	{
		uint32_t t = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
		CSBSDAT_Year = t & 63u; CSBSDAT_Month = (t >> 6) & 15u; CSBSDAT_Day = (t >> 10) & 31u;
		CSBSDAT_Hour = (t >> 15) & 31u; CSBSDAT_Minute = (t >> 20) & 63u; CSBSDAT_Second = (t >> 26) & 63u;
	}
	else if ((RxPduId == CANIF_RXPDU_XCP_CMD) && (PduInfoPtr->SduLength != 0u))
	{
		if ((CanH_CommunicationState == FULL_COMMUNICATION) && (ComM_IsRxAllowed(COMM_CHANNEL_CAN) != 0u))
		{
			Xcp_HandleCommand(data, (uint8_t)PduInfoPtr->SduLength);
		}
	}
	else
	{
		/* other PDUs are not handled here */
	}
}
