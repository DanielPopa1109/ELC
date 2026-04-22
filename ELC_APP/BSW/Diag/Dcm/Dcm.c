#include "Dcm.h"
#include "Nvm.h"
#include "Dem.h"
#include "EcuM.h"
#include "FreeRTOSConfig.h"
#include "SMon.h"
#include "CanH.h"

static iso_tx_ctx_t g =
{
		.active = 0u, .fc_expect_id = 0u, .fc_pending = 0u,
		.stmin_ms_default = 0u, .timeout_ms = 100u
};
volatile uint8_t Dcm_RequestPending;
volatile uint8_t Dcm_IsoTp_TxActive = 0u;
uint8_t g_sec_seed[4u];
volatile uint8_t g_sec_level = 0u;
volatile uint8_t g_sec_unlocked = 0;
uint8_t rx[8u];
uint8_t Dcm_RxData[8u];
uint8_t Dcm_TxData[8u];
uint8_t Dcm_SWV[4u] = {33u, 33u, 33u, 0xFFu};
uint8_t Dcm_LoadStatus = 0xFFu;
uint8_t Dcm_CC = 0u;
uint8_t Dcm_CDTCS = 0u;
uint32_t Dcm_SessionCounter = 2000u;
uint32_t Dcm_ActiveSessionState __attribute((section(".ncr")));
uint32_t Dcm_MainCounter = 0u;
uint32_t Dcm_TxMailbox = 0;
CAN_TxHeaderTypeDef Dcm_DiagTxHeader;
CAN_RxHeaderTypeDef Dcm_DiagRxHeader = {0, 0, 0, 0, 0, 0, 0};
uint32_t Dcm_LoadTimer = 0u;
uint8_t Dcm_OBD_VIN[17u] =
{
    0x57u, 0x42u, 0x41u, 0x35u, 0x41u, 0x37u, 0x43u, 0x35u, 0x30u,
    0x45u, 0x44u, 0x31u, 0x32u, 0x33u, 0x34u, 0x35u, 0x36u
};
uint8_t Dcm_OBD_CALID[4u] = {0u};
uint8_t Dcm_OBD_ECU_NAME[3u] = {69u, 76u, 67u};

bool Dcm_IsoTp_RxHook(const CAN_RxHeaderTypeDef *rh, const uint8_t *data);
bool Dcm_IsoTp_Send(uint32_t req_canid, const uint8_t *payload, uint16_t len, uint8_t pad, uint8_t force_pad);
void Dcm_IsoTp_Config(uint8_t stmin_default_ms, uint32_t timeout_ms);
void Dcm_TesterPresent();
void Dcm_ExtendedSession();
void Dcm_DefaultSession();
void Dcm_LoadControl();
void Dcm_main();
void Dcm_ProgrammingSession();
void Dcm_HardReset();
void Dcm_ReadSWV();
void Dcm_RDBI_SecureAccess(void);
void Dcm_RC_HealSupply();
void Dcm_ReadLoadStatus();
void Dcm_SendNrc(uint8_t nrc);
void Dcm_RDBI_ResetCounter();
void Dcm_RDBI_TimeActive();
void Dcm_RDBI_TimeWithoutReset();
void Dcm_RDBI_ResetData();
void Dcm_RDBI_CpuLoad();
void Dcm_CommunicationControl();
void Dcm_ControlDTCSetting();
void Dcm_CDTCI();
void Dcm_RDTCI();
void Dcm_RequestSeed();
void Dcm_SendKey();
void Dcm_RDBI_ReadActiveDiagnosticSession();
void Dcm_RDBI_ReadActiveSoftwareBlock();
void Dcm_RDBI_T30Min10s(void);
void Dcm_RDBI_T30Max10s(void);
void Dcm_RDBI_T30Avg10s(void);
void Dcm_RDBI_L1Min10s(void);
void Dcm_RDBI_L1Max10s(void);
void Dcm_RDBI_L1Avg10s(void);
void Dcm_RDBI_L1_Current_Min10s(void);
void Dcm_RDBI_L1_Current_Max10s(void);
void Dcm_RDBI_L1_Current_Avg10s(void);
void Dcm_RDBI_T30MinSW(void);
void Dcm_RDBI_T30MaxSW(void);
void Dcm_RDBI_T30AvgSW(void);
void Dcm_RDBI_L1MinSW(void);
void Dcm_RDBI_L1MaxSW(void);
void Dcm_RDBI_L1AvgSW(void);
void Dcm_RDBI_L1_Current_MinSW(void);
void Dcm_RDBI_L1_Current_MaxSW(void);
void Dcm_RDBI_L1_Current_AvgSW(void);
void Dcm_RC_ReadMinMaxAvg(void);
uint32_t GenKeyFromSeed32(uint32_t seed32, uint32_t level);
void SecCalcKeyFromSeed(const uint8_t *seedBytes, uint8_t *keyBytes, uint8_t seedLen, uint8_t level);
HAL_StatusTypeDef Dcm_CanSendSF(uint32_t stdId,uint8_t *data,uint8_t len);

void Dcm_RC_ReadMinMaxAvg(void)
{
	uint8_t payload[84u];
	uint16_t len = 0u;

	/* Positive response: 0x71 */
	payload[len++] = 0x71u;
	payload[len++] = rx[2u];   /* subfunction echo */
	payload[len++] = rx[3u];   /* routine ID */
	payload[len++] = rx[4u];   // Routine ID low

	/* Pack 18 floats */
	for(uint8_t i = 0u; i < 18u; i++)
	{
		memcpy(&payload[len], &SMon_DataMinMaxAvg[i], sizeof(float));
		len += sizeof(float);
	}

	(void)Dcm_IsoTp_Send(
			Dcm_DiagRxHeader.StdId,
			payload,
			len,
			0x00u,
			0u
	);

	memset(rx, 0, 8);
}

void Dcm_RDBI_T30Min10s(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_10s_T30_Min, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_T30Max10s(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_10s_T30_Max, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_T30Avg10s(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_10s_T30_Avg, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1Min10s(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_10s_L1_Min, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1Max10s(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_10s_L1_Max, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1Avg10s(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_10s_L1_Avg, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1_Current_Min10s(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_10s_L1I_Min, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1_Current_Max10s(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_10s_L1I_Max, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1_Current_Avg10s(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_10s_L1I_Avg, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_T30MinSW(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_SW_T30_Min, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_T30MaxSW(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_SW_T30_Max, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_T30AvgSW(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_SW_T30_Avg, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1MinSW(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_SW_L1_Min, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1MaxSW(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_SW_L1_Max, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1AvgSW(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_SW_L1_Avg, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1_Current_MinSW(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_SW_L1I_Min, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1_Current_MaxSW(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_SW_L1I_Max, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_L1_Current_AvgSW(void)
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];

	memcpy(&Dcm_TxData[4u], &SMon_SW_L1I_Avg, sizeof(float));

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_SecureAccess(void)
{
	Dcm_TxData[0u] = rx[0u] + 1;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = g_sec_unlocked;
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_ReadActiveSoftwareBlock()
{
	Dcm_TxData[0u] = rx[0u] + 1;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = 0x01u;;
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_ReadActiveDiagnosticSession()
{
	Dcm_TxData[0u] = rx[0u] + 1;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = Dcm_ActiveSessionState;
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

uint32_t GenKeyFromSeed32(uint32_t seed32, uint32_t level)
{
	uint32_t x = seed32 ^ 0xA5A5A5A5u;

	x += 0x13572468u + (level * 0x1F3D5B79u);
	x = (x << 3) | (x >> (32u - 3u));  // ROL3

	uint32_t rev =
			((seed32 & 0x000000FFu) << 24) |
			((seed32 & 0x0000FF00u) << 8)  |
			((seed32 & 0x00FF0000u) >> 8)  |
			((seed32 & 0xFF000000u) >> 24);

	return x ^ rev;
}

void SecCalcKeyFromSeed(const uint8_t *seedBytes,
                        uint8_t *keyBytes,
                        uint8_t seedLen,
                        uint8_t level)
{
    uint32_t seed32;
    uint32_t key32;

    /* Defensive checks */
    if ((seedBytes == NULL) || (keyBytes == NULL))
    {
        return;
    }

    if (seedLen < 4u)
    {
        /* Reject: invalid seed length */
        keyBytes[0] = 0u;
        keyBytes[1] = 0u;
        keyBytes[2] = 0u;
        keyBytes[3] = 0u;
        return;
    }

    seed32 =
        ((uint32_t)seedBytes[0] << 24) |
        ((uint32_t)seedBytes[1] << 16) |
        ((uint32_t)seedBytes[2] << 8)  |
        ((uint32_t)seedBytes[3]);

    key32 = GenKeyFromSeed32(seed32, level);

    keyBytes[0] = (uint8_t)(key32 >> 24);
    keyBytes[1] = (uint8_t)(key32 >> 16);
    keyBytes[2] = (uint8_t)(key32 >> 8);
    keyBytes[3] = (uint8_t)(key32);
}

HAL_StatusTypeDef Dcm_CanSendSF(uint32_t stdId, uint8_t *data, uint8_t len)
{
	CAN_TxHeaderTypeDef th = {0};
	uint32_t mbx;

	th.StdId = stdId;
	th.IDE   = CAN_ID_STD;
	th.RTR   = CAN_RTR_DATA;
	th.DLC   = len;

	HAL_StatusTypeDef st;

	do
	{
		st = HAL_CAN_AddTxMessage(&hcan, &th, data, &mbx);
	} while (st != HAL_OK);

	return st;
}

void Dcm_RequestSeed()
{
	uint32_t s = HAL_GetTick();

	g_sec_level    = rx[2u];
	g_sec_unlocked = 0u;
	s ^= 0xA5A55A5Au;
	g_sec_seed[0] = (uint8_t)(s >> 24);
	g_sec_seed[1] = (uint8_t)(s >> 16);
	g_sec_seed[2] = (uint8_t)(s >> 8);
	g_sec_seed[3] = (uint8_t)(s >> 0);
	Dcm_TxData[0u] = (uint8_t)(rx[0u] + 4u);
	Dcm_TxData[1u] = (uint8_t)(rx[1u] + 0x40u);
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = g_sec_seed[0];
	Dcm_TxData[4u] = g_sec_seed[1];
	Dcm_TxData[5u] = g_sec_seed[2];
	Dcm_TxData[6u] = g_sec_seed[3];
	Dcm_TxData[7u] = 0x00u;
	Dcm_DiagTxHeader.DLC   = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagTxHeader.StdId, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for (uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i]         = 0u;
	}
}

void Dcm_SendKey()
{
	uint8_t rxKey[4];
	uint8_t expKey[4];

	rxKey[0] = rx[3u];
	rxKey[1] = rx[4u];
	rxKey[2] = rx[5u];
	rxKey[3] = rx[6u];

	SecCalcKeyFromSeed(g_sec_seed, expKey, 4u, g_sec_level);

	if(rxKey[0] == expKey[0] && rxKey[1] == expKey[1] && rxKey[2] == expKey[2] && rxKey[3] == expKey[3])
	{
		g_sec_unlocked = 1u;
		Dcm_TxData[0u] = 0x02u;
		Dcm_TxData[1u] = (uint8_t)(rx[1u] + 0x40u);
		Dcm_TxData[2u] = rx[2u];
		Dcm_TxData[3u] = 0x00u;
		Dcm_TxData[4u] = 0x00u;
		Dcm_TxData[5u] = 0x00u;
		Dcm_TxData[6u] = 0x00u;
		Dcm_TxData[7u] = 0x00u;
	}
	else
	{
		Dcm_TxData[0u] = 0x03u;
		Dcm_TxData[1u] = 0x7Fu;
		Dcm_TxData[2u] = rx[1u];
		Dcm_TxData[3u] = 0x35u;
		Dcm_TxData[4u] = 0x00u;
		Dcm_TxData[5u] = 0x00u;
		Dcm_TxData[6u] = 0x00u;
		Dcm_TxData[7u] = 0x00u;
	}

	Dcm_DiagTxHeader.DLC   = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagTxHeader.StdId, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for (uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i]         = 0u;
	}
}

void Dcm_CDTCI()
{
	Dcm_TxData[0u] = 0x03;
	Dcm_TxData[1u] = 0x7f;
	Dcm_TxData[2u] = rx[1u];
	Dcm_TxData[3u] = 0x78;
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0; i < DEM_MAX_FF_DTC; i++)
	{
		Dem_DTC_Stat[i] = 0x50u;
	}

	memset(&Dem_FF[0u].occurrenceCnt, 0u, sizeof(Dem_FF));

	Nvm_WriteBlock(1u, &Dem_DTC_Stat[0u]);
	Nvm_WriteBlock(2u, &Dem_FF[0u].occurrenceCnt);

	Dcm_TxData[0u] = rx[0u];
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

static bool Dcm_PutU8(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint8_t v)
{
    if ((*len + 1u) > maxLen)
    {
        return false;
    }

    buf[*len] = v;
    *len += 1u;
    return true;
}

static bool Dcm_PutU16BE(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint16_t v)
{
    if ((*len + 2u) > maxLen)
    {
        return false;
    }

    buf[*len + 0u] = (uint8_t)(v >> 8);
    buf[*len + 1u] = (uint8_t)(v);
    *len += 2u;
    return true;
}

static bool Dcm_PutU24BE(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint32_t v)
{
    if ((*len + 3u) > maxLen)
    {
        return false;
    }

    buf[*len + 0u] = (uint8_t)(v >> 16);
    buf[*len + 1u] = (uint8_t)(v >> 8);
    buf[*len + 2u] = (uint8_t)(v);
    *len += 3u;
    return true;
}

static bool Dcm_PutFloatRaw(uint8_t *buf, uint16_t *len, uint16_t maxLen, float v)
{
    if ((*len + 4u) > maxLen)
    {
        return false;
    }

    memcpy(&buf[*len], &v, sizeof(float));
    *len += 4u;
    return true;
}

static bool Dcm_PutDidU8(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint16_t did, uint8_t value)
{
    return Dcm_PutU16BE(buf, len, maxLen, did) &&
           Dcm_PutU8(buf, len, maxLen, value);
}

static bool Dcm_PutDidU16(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint16_t did, uint16_t value)
{
    return Dcm_PutU16BE(buf, len, maxLen, did) &&
           Dcm_PutU16BE(buf, len, maxLen, value);
}

static bool Dcm_PutDidFloat(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint16_t did, float value)
{
    return Dcm_PutU16BE(buf, len, maxLen, did) &&
           Dcm_PutFloatRaw(buf, len, maxLen, value);
}

/* Encode one snapshot record in a standards-oriented way:
 *
 *  DTC[3]
 *  statusOfDTC[1]
 *  snapshotRecordNumber[1]
 *  numberOfIdentifiers[1]
 *  DID[2] data...
 *  DID[2] data...
 *  ...
 *
 * Returns false on buffer overflow.
 */
static bool Dcm_EncodeOneSnapshotRecord(uint8_t *payload,
                                        uint16_t *len,
                                        uint16_t maxLen,
                                        uint32_t dtc,
                                        uint8_t status,
                                        uint8_t snapshotRecordNumber,
                                        const Dem_FreezeFrame_t *ff)
{
    const uint8_t numberOfIdentifiers = 12u;

    if ((payload == NULL) || (len == NULL) || (ff == NULL))
    {
        return false;
    }

    if (!Dcm_PutU24BE(payload, len, maxLen, dtc)) return false;
    if (!Dcm_PutU8(payload, len, maxLen, status)) return false;
    if (!Dcm_PutU8(payload, len, maxLen, snapshotRecordNumber)) return false;
    if (!Dcm_PutU8(payload, len, maxLen, numberOfIdentifiers)) return false;

    if (!Dcm_PutDidU16(payload, len, maxLen, DCM_DID_FF_OCCURRENCE_COUNT, ff->occurrenceCnt)) return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_YEAR,             ff->year))          return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_MONTH,            ff->month))         return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_DAY,              ff->day))           return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_HOUR,             ff->hour))          return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_MINUTE,           ff->minute))        return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_SECOND,           ff->second))        return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_VEH_STATUS,       ff->vehStatus)) return false;
    if (!Dcm_PutDidU16(payload, len, maxLen, DCM_DID_FF_L1_MV,            ff->L1VFB))            return false;
    if (!Dcm_PutDidU16(payload, len, maxLen, DCM_DID_FF_T30_MV,           ff->T30VFB))           return false;
    if (!Dcm_PutDidFloat(payload, len, maxLen, DCM_DID_FF_NTC_C,          ff->L1NTC))return false;
    if (!Dcm_PutDidFloat(payload, len, maxLen, DCM_DID_FF_ISENSE_A,       ff->L1ISENSE))        return false;

    return true;
}

/* Standards-oriented implementation for UDS 0x19 0x04
 *
 * Request expected:
 *   rx[1] = 0x19
 *   rx[2] = 0x04
 *   rx[3] = DTC high
 *   rx[4] = DTC mid
 *   rx[5] = DTC low
 *   rx[6] = snapshot record number
 *
 * Notes:
 * - record number 0xFF = all records for the requested DTC
 * - DTC 0xFFFFFF may be accepted as wildcard for "all DTCs"
 *   if you want that behavior; some testers use exact DTC only.
 */
void Dcm_RDTCI_19_04_Standard(void)
{
    uint8_t payload[DCM_19_04_MAX_PAYLOAD];
    uint16_t len = 0u;
    uint32_t reqDtc;
    uint8_t reqRecordNumber;
    bool anyMatch = false;

    /* Need at least SID, subfunction, DTC[3], record number */
    if (rx[0u] < 5u)
    {
        Dcm_SendNrc(DCM_NRC_INCORRECT_LENGTH);
        return;
    }

    reqDtc =
        ((uint32_t)rx[3u] << 16) |
        ((uint32_t)rx[4u] << 8)  |
        ((uint32_t)rx[5u]);

    reqRecordNumber = rx[6u];

    payload[len++] = 0x59u;
    payload[len++] = 0x04u;

    for (uint8_t i = 0u; i < DEM_MAX_FF_DTC; i++)
    {
        const uint32_t dtc = Dem_PreDefined_DTC_Table[i];
        const uint8_t status = Dem_DTC_Stat[i];
        const uint8_t snapshotRecordNumber = 0x01u; /* one FF record per DTC in your current design */

        /* No stored DTC */
        if (status == 0x50u)
        {
            continue;
        }

        /* DTC filter */
        if ((reqDtc != DCM_19_04_ALL_DTCS) && (dtc != reqDtc))
        {
            continue;
        }

        /* Snapshot record filter */
        if ((reqRecordNumber != DCM_19_04_ALL_RECORDS) && (reqRecordNumber != snapshotRecordNumber))
        {
            continue;
        }

        if (!Dcm_EncodeOneSnapshotRecord(payload,
                                         &len,
                                         (uint16_t)sizeof(payload),
                                         dtc,
                                         status,
                                         snapshotRecordNumber,
                                         &Dem_FF[i]))
        {
            /* Buffer too small -> request out of range is acceptable here */
            Dcm_SendNrc(DCM_NRC_REQUEST_OUT_OF_RANGE);
            return;
        }

        anyMatch = true;
    }

    if (!anyMatch)
    {
        Dcm_SendNrc(DCM_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    (void)Dcm_IsoTp_Send(
        Dcm_DiagRxHeader.StdId,
        payload,
        len,
        0x00u,
        0u
    );

    for (uint8_t i = 0u; i < 8u; i++)
    {
        Dcm_TxData[i] = 0u;
        rx[i] = 0u;
    }
}

void Dcm_RDTCI(void)
{
    uint8_t spayload[3u + (DEM_MAX_FF_DTC * 4u)];
    uint16_t slen = 0u;
    uint8_t reqStatusMask = rx[3u];

    /* Positive response: 0x59 0x02 */
    spayload[slen++] = 0x59u;
    spayload[slen++] = 0x02u;

    /* Supported DTC status bits by this ECU */
    /* Adjust this if your DEM really supports a different set */
    spayload[slen++] = 0xFFu;

    for (uint8_t i = 0u; i < DEM_MAX_FF_DTC; i++)
    {
        uint8_t st = Dem_DTC_Stat[i];
        uint32_t dtc = Dem_PreDefined_DTC_Table[i];

        /* Skip "no fault stored" entries */
        if (st == 0x50u)
        {
            continue;
        }

        /* Apply request filter like UDS expects */
        if ((st & reqStatusMask) == 0u)
        {
            continue;
        }

        /* Standard UDS order: DTC[3] + status[1] */
        spayload[slen++] = (uint8_t)(dtc >> 16);
        spayload[slen++] = (uint8_t)(dtc >> 8);
        spayload[slen++] = (uint8_t)(dtc);
        spayload[slen++] = st;
    }

    (void)Dcm_IsoTp_Send(Dcm_DiagRxHeader.StdId, spayload, slen, 0x00u, 0u);

    for (uint8_t i = 0u; i < 8u; i++)
    {
        Dcm_TxData[i] = 0u;
        rx[i] = 0u;
    }
}

void Dcm_CommunicationControl()
{
	Dcm_TxData[0u] = rx[0u];
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_CC = rx[2u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_ControlDTCSetting()
{
	Dcm_TxData[0u] = rx[0u];
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_CDTCS = rx[2u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_CpuLoad()
{
	Dcm_TxData[0u] = rx[0u] + 1;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = (uint8_t)OS_XCP_CpuLoad;
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_ResetData()
{
	Dcm_TxData[0u] = rx[0u] + 2;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = EcuM_ResetReason;
	Dcm_TxData[5u] = EcuM_ResetInfo;
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_ResetCounter()
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = (uint8_t)(EcuM_TotalResetCounter);
	Dcm_TxData[5u] = (uint8_t)(EcuM_TotalResetCounter >> 8u);
	Dcm_TxData[6u] = (uint8_t)(EcuM_TotalResetCounter >> 16u);
	Dcm_TxData[7u] = (uint8_t)(EcuM_TotalResetCounter >> 24u);
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_TimeActive()
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = (uint8_t)(EcuM_TimeActive);
	Dcm_TxData[5u] = (uint8_t)(EcuM_TimeActive >> 8u);
	Dcm_TxData[6u] = (uint8_t)(EcuM_TimeActive >> 16u);
	Dcm_TxData[7u] = (uint8_t)(EcuM_TimeActive >> 24u);
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RDBI_TimeWithoutReset()
{
	Dcm_TxData[0u] = rx[0u] + 4;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = (uint8_t)(EcuM_TimeWithoutReset);
	Dcm_TxData[5u] = (uint8_t)(EcuM_TimeWithoutReset >> 8u);
	Dcm_TxData[6u] = (uint8_t)(EcuM_TimeWithoutReset >> 16u);
	Dcm_TxData[7u] = (uint8_t)(EcuM_TimeWithoutReset >> 24u);
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_TesterPresent()
{
	Dcm_TxData[0u] = rx[0u];
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_DefaultSession()
{
	Dcm_TxData[0u] = rx[0u];
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_ActiveSessionState = rx[2u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_ExtendedSession()
{
	Dcm_TxData[0u] = rx[0u];
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_ActiveSessionState = rx[2u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_LoadControl()
{
	Dcm_TxData[0u] = rx[0u];
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];

	Dcm_LoadStatus = rx[5u];

	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	Dcm_LoadTimer = (rx[6] * 5000u) / 5u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_ReadLoadStatus()
{
	Dcm_TxData[0u] = rx[0u] + 1u;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = SMon_L1ST;
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

static inline void le32(uint8_t *p, uint32_t v)
{
	p[0]= (uint8_t) v;
	p[1]= (uint8_t)(v>>8);
	p[2]= (uint8_t)(v>>16);
	p[3]= (uint8_t)(v>>24);
}

void Dcm_IsoTp_Config(uint8_t stmin_default_ms, uint32_t timeout_ms)
{
	g.stmin_ms_default = stmin_default_ms;
	g.timeout_ms = timeout_ms;
}

bool Dcm_IsoTp_RxHook(const CAN_RxHeaderTypeDef *rh, const uint8_t *d)
{
	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}

	if (!g.active)
	{
		return false;
	}
	else
	{
		/* Do nothing. */
	}

	if (rh->IDE != CAN_ID_STD)
	{
		return false;
	}
	else
	{
		/* Do nothing. */
	}

	if (rh->StdId != g.fc_expect_id)
	{
		return false;
	}
	else
	{
		/* Do nothing. */
	}

	if ( (d[0] & 0xF0u) != 0x30u )
	{
		return false; /* not FC */
	}
	else
	{
		/* Do nothing. */
	}

	if (!g.fc_pending)
	{
		memcpy((void*)g.fc_bytes, d, 8u);
		g.fc_pending = 1;
	}
	else
	{
		/* Do nothing. */
	}

	return true; /* consumed by ISO-TP */
}

static HAL_StatusTypeDef can_tx8(uint32_t stdId, const uint8_t *data, uint8_t len, uint8_t pad, uint8_t force_pad)
{
	CAN_TxHeaderTypeDef th = {0};
	uint32_t mbx;
	uint8_t buf[8];
	const uint8_t *p;

	th.StdId = stdId;
	th.IDE   = CAN_ID_STD;
	th.RTR   = CAN_RTR_DATA;

	if (force_pad)
	{
		memset(buf, pad, 8);
		memcpy(buf, data, len);
		th.DLC = 8;
		p = buf;
	}
	else
	{
		th.DLC = len;
		p = data;
	}

	HAL_StatusTypeDef st;

	do
	{
		st = HAL_CAN_AddTxMessage(&hcan, &th, (uint8_t*)p, &mbx);
	} while (st != HAL_OK);

	return st;
}

static int wait_fc(uint8_t *bs, uint8_t *stmin_ms)
{
	uint32_t t0 = HAL_GetTick();

	while ((HAL_GetTick() - t0) < g.timeout_ms)
	{
		if (g.fc_pending)
		{
			g.fc_pending = 0;

			const uint8_t *rx = g.fc_bytes;
			uint8_t fs = rx[0] & 0x0Fu;

			if (fs == 0x00u)
			{
				*bs = rx[1];

				uint8_t st = rx[2];

				if (st <= 0x7Fu)
				{
					*stmin_ms = st;
				}
				else if (st >= 0xF1u && st <= 0xF9u)
				{
					*stmin_ms = 1;
				}
				else
				{
					*stmin_ms = g.stmin_ms_default;
				}

				return 1;
			}
			else if (fs == 0x01u)
			{
				t0 = HAL_GetTick();
			}
			else
			{
				return -1;
			}
		}
		else
		{
			/* Do nothing. */
		}
	}

	return 0; /* timeout */
}

bool Dcm_IsoTp_Send(uint32_t req_canid, const uint8_t *payload, uint16_t len, uint8_t pad, uint8_t force_pad)
{
	uint32_t resp_id = req_canid + DCM_RESP_ID_DELTA;
	uint8_t f[8];

	Dcm_IsoTp_TxActive = 1u;   /* block normal CAN TX while we stream */

	if (len <= 7u)
	{
		f[0] = (uint8_t)(0x00u | (len & 0x0Fu));

		memcpy(&f[1], payload, len);

		bool ok = (can_tx8(resp_id, f, (uint8_t)(1u + len), pad, force_pad) == HAL_OK);

		Dcm_IsoTp_TxActive = 0u;

		return ok;
	}
	else
	{
		/* Do nothing. */
	}

	/* First Frame */

	f[0] = (uint8_t)(0x10u | ((len >> 8) & 0x0Fu));
	f[1] = (uint8_t)(len & 0xFFu);

	memcpy(&f[2], payload, 6u);

	g.active = 1;
	g.fc_expect_id = req_canid;
	g.fc_pending = 0;

	if (can_tx8(resp_id, f, 8u, pad, force_pad) != HAL_OK)
	{
		g.active = 0;
		Dcm_IsoTp_TxActive = 0u;

		return false;
	}
	else
	{
		/* Do nothing. */
	}

	uint8_t bs = 0, stmin = g.stmin_ms_default;

	int fc = wait_fc(&bs, &stmin);

	if (fc <= 0)
	{
		g.active = 0;
		Dcm_IsoTp_TxActive = 0u;

		return false;
	}
	else
	{
		/* Do nothing. */
	}

	uint16_t idx = 6u;
	uint8_t  sn  = 1u;
	uint8_t  bs_cnt = bs;

	while (idx < len) {
		if (bs != 0u && bs_cnt == 0u)
		{
			fc = wait_fc(&bs, &stmin);

			if (fc <= 0)
			{
				g.active = 0;
				Dcm_IsoTp_TxActive = 0u;

				return false;
			}
			else
			{
				/* Do nothing. */
			}

			bs_cnt = bs;
		}
		else
		{
			/* Do nothing. */
		}

		f[0] = (uint8_t)(0x20u | (sn & 0x0Fu));

		uint8_t chunk = (uint8_t)((len - idx) >= 7u ? 7u : (len - idx));

		memset(&f[1], pad, 7u);
		memcpy(&f[1], &payload[idx], chunk);

		if (can_tx8(resp_id, f, (uint8_t)(1u + chunk), pad, force_pad) != HAL_OK)
		{
			g.active = 0;
			Dcm_IsoTp_TxActive = 0u;

			return false;
		}
		else
		{
			/* Do nothing. */
		}

		idx += chunk;
		sn = (uint8_t)((sn + 1u) & 0x0Fu);

		if (bs != 0u && bs_cnt > 0u)
		{
			bs_cnt--;
		}
		else
		{
			/* Do nothing. */
		}

		if (stmin)
		{
			HAL_Delay(stmin);
		}
		else
		{
			/* Do nothing. */
		}
	}

	g.active = 0;
	Dcm_IsoTp_TxActive = 0u;

	return true;
}

void Dcm_ProgrammingSession()
{
	Dcm_TxData[0u] = 0x03;
	Dcm_TxData[1u] = 0x7f;
	Dcm_TxData[2u] = rx[1u];
	Dcm_TxData[3u] = 0x78;
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	Nvm_WriteAll();

	Dcm_TxData[0u] = rx[0u];
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_ActiveSessionState = rx[2u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	HAL_Delay(1);

	EcuM_PerformReset(0u, 0u);
}

void Dcm_SendNrc(uint8_t nrc)
{
    Dcm_TxData[0u] = 0x03u;
    Dcm_TxData[1u] = 0x7Fu;
    Dcm_TxData[2u] = rx[1u];
    Dcm_TxData[3u] = nrc;

    Dcm_TxData[4u] = 0u;
    Dcm_TxData[5u] = 0u;
    Dcm_TxData[6u] = 0u;
    Dcm_TxData[7u] = 0u;

    Dcm_DiagTxHeader.DLC   = 4u;
    Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

    (void)Dcm_CanSendSF(
        Dcm_DiagTxHeader.StdId,
        Dcm_TxData,
        Dcm_DiagTxHeader.DLC
    );

    for(uint8_t i = 0u; i < 8u; i++)
    {
        Dcm_TxData[i] = 0u;
        rx[i] = 0u;
    }
}

void Dcm_HardReset()
{
	Dcm_TxData[0u] = 0x03;
	Dcm_TxData[1u] = 0x7f;
	Dcm_TxData[2u] = rx[1u];
	Dcm_TxData[3u] = 0x78;
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	EcuM_ResetInfo = 0u;
	EcuM_ResetReason = 0u;

	Nvm_WriteAll();

	Dcm_TxData[0u] = rx[0u];
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	HAL_Delay(1);

	EcuM_PerformReset(0u, 0u);
}

void Dcm_ReadSWV()
{
	Dcm_TxData[0u] = rx[0u] + 4u;
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = Dcm_SWV[0u];
	Dcm_TxData[5u] = Dcm_SWV[1u];
	Dcm_TxData[6u] = Nvm_ParamFlashBlock[290u];
	Dcm_TxData[7u] = Dcm_SWV[3u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);
	Dcm_DiagTxHeader.DLC = 0;
	Dcm_DiagTxHeader.StdId = 0;

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}
}

void Dcm_RC_HealSupply()
{
	SMon_RetryCnt = 0u;
	SMon_LockSupply = 0u;
	Dcm_TxData[0u] = rx[0u];
	Dcm_TxData[1u] = rx[1u] + 0x40u;
	Dcm_TxData[2u] = rx[2u];
	Dcm_TxData[3u] = rx[3u];
	Dcm_TxData[4u] = rx[4u];
	Dcm_TxData[5u] = rx[5u];
	Dcm_TxData[6u] = rx[6u];
	Dcm_TxData[7u] = rx[7u];
	Dcm_DiagTxHeader.DLC = Dcm_DiagRxHeader.DLC;
	Dcm_DiagTxHeader.StdId = Dcm_DiagRxHeader.StdId + 0x01u;

	(void)Dcm_CanSendSF(Dcm_DiagRxHeader.StdId + 0x01u, Dcm_TxData, Dcm_DiagTxHeader.DLC);

	for(uint8_t i = 0u; i < 8u; i++)
	{
		Dcm_TxData[i] = 0u;
		rx[i] = 0u;
	}

	Dcm_DiagTxHeader.DLC = 0;
	Dcm_DiagTxHeader.StdId = 0;
}

void Dcm_main()
{
	__disable_irq();

	if (Dcm_RequestPending)
	{
		for (uint8_t i = 0u; i < 8u; i++)
		{
			rx[i] = Dcm_RxData[i];
			Dcm_RxData[i] = 0u;
		}

		Dcm_RequestPending = 0u;
		Dcm_SessionCounter = 1000u;
	}
	else
	{
		if(0u != Dcm_SessionCounter)
		{
			Dcm_SessionCounter --;
		}
		else
		{
			/* Do nothing. */
		}

	}

	if(0u < Dcm_LoadTimer)
	{
		Dcm_LoadTimer--;
	}
	else
	{
		/* Do nothing. */
	}

	if(0u == Dcm_LoadTimer)
	{
		Dcm_LoadStatus = 0xFFu;
	}
	else
	{
		/* Do nothing. */
	}

	__enable_irq();
	if(0u == Dcm_MainCounter)
	{
		Dcm_ActiveSessionState = 1u;
		Dcm_IsoTp_Config(/*stmin_default_ms=*/5u, /*timeout_ms=*/10000u);
	}
	else
	{
		/* Do nothing. */
	}

	if(0u == Dcm_SessionCounter)
	{
		Dcm_ActiveSessionState = 1u;
	}
	else
	{
		/* Do nothing. */
	}

	if(0x19u == rx[1u] && 0x04 == rx[2u])
	{
		Dcm_RDTCI_19_04_Standard();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x14u == rx[1u])
	{
		Dcm_CDTCI();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x19u == rx[1u] && 0x02 == rx[2u])
	{
		Dcm_RDTCI();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x28u == rx[1u] && 3u == Dcm_ActiveSessionState)
	{
		Dcm_CommunicationControl();
	}
	else if(0x28u == rx[1u] && 3u != Dcm_ActiveSessionState)
	{
		Dcm_SendNrc(DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);
	}
	else
	{
		/* Do nothing. */
	}

	if(0x85u == rx[1u] && 3u == Dcm_ActiveSessionState)
	{
		Dcm_ControlDTCSetting();
	}
	else if(0x85u == rx[1u] && 3u != Dcm_ActiveSessionState)
	{
		Dcm_SendNrc(DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);
	}
	else
	{
		/* Do nothing. */
	}

	if(0x22u == rx[1u])
	{
		switch(rx[3u])
		{

		case 0x01u:

			Dcm_RDBI_ResetCounter();

			break;

		case 0x03u:

			Dcm_RDBI_TimeActive();

			break;
		case 0x04u:

			Dcm_RDBI_TimeWithoutReset();

			break;

		case 0x05u:

			Dcm_RDBI_ResetData();

			break;

		case 0x06u:

			Dcm_RDBI_CpuLoad();

			break;

		case 0x07u:

			Dcm_RDBI_SecureAccess();

			break;

		case 11u:

			Dcm_RDBI_T30Min10s();

			break;

		case 12u:

			Dcm_RDBI_T30Max10s();

			break;

		case 13u:

			Dcm_RDBI_T30Avg10s();

			break;

		case 14u:

			Dcm_RDBI_L1Min10s();

			break;

		case 15u:

			Dcm_RDBI_L1Max10s();

			break;

		case 16u:

			Dcm_RDBI_L1Avg10s();

			break;

		case 17u:

			Dcm_RDBI_L1_Current_Min10s();

			break;

		case 18u:

			Dcm_RDBI_L1_Current_Max10s();

			break;

		case 19u:

			Dcm_RDBI_L1_Current_Avg10s();

			break;

		case 20u:

			Dcm_RDBI_T30MinSW();

			break;

		case 21u:

			Dcm_RDBI_T30MaxSW();

			break;

		case 22u:

			Dcm_RDBI_T30AvgSW();

			break;

		case 23u:

			Dcm_RDBI_L1MinSW();

			break;

		case 24u:

			Dcm_RDBI_L1MaxSW();

			break;

		case 25u:

			Dcm_RDBI_L1AvgSW();

			break;

		case 26u:

			Dcm_RDBI_L1_Current_MinSW();

			break;

		case 27u:

			Dcm_RDBI_L1_Current_MaxSW();

			break;

		case 28u:

			Dcm_RDBI_L1_Current_AvgSW();

			break;

		case 29u:

			Dcm_ReadLoadStatus();

			break;

		case 0x80u:

			Dcm_ReadSWV();

			break;

		case 0x7Eu:

			Dcm_RDBI_ReadActiveDiagnosticSession();

			break;

		case 0x7Fu:

			Dcm_RDBI_ReadActiveSoftwareBlock();

			break;

		default:

			Dcm_SendNrc(DCM_NRC_SUBFUNCTION_NOT_SUPPORTED);

			break;

		}
	}
	else
	{
		/* Do nothing. */
	}

	if(0x27u == rx[1u] && 0x01u == rx[2u])
	{
		Dcm_RequestSeed();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x27u == rx[1u] && 0x02u == rx[2u])
	{
		Dcm_SendKey();
	}
	else
	{
		/* Do nothing. */
	}

	if(1u == g_sec_unlocked)
	{
		Dem_SetDtc(0x62u, 0x2fu);
	}
	else
	{
		if(0x2fu == Dem_GetDtcStatus(0x62u))
		{
			Dem_SetDtc(0x62u, 0x2eu);
		}
		else
		{
			/* Do nothing. */
		}
	}

	if(0x02 == rx[2u] && 3u == Dcm_ActiveSessionState && (1u == g_sec_unlocked) && 6u == CanH_VehicleStatus)
	{
		Dcm_ProgrammingSession();
	}
	else if(0x02 == rx[2u] && (3u != Dcm_ActiveSessionState || 0u == g_sec_unlocked || 6u != CanH_VehicleStatus))
	{
		if(3u != Dcm_ActiveSessionState)
		{
			Dcm_SendNrc(DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);
		}
		else if(0u == g_sec_unlocked)
		{
			Dcm_SendNrc(DCM_NRC_SECURITY_ACCESS_DENIED);
		}
		else if(6u != CanH_VehicleStatus)
		{
			Dcm_SendNrc(DCM_NRC_CONDITIONS_NOT_CORRECT);
		}
		else
		{
			// do nothing.
		}
	}
	else
	{
		/* Do nothing. */
	}

	if(0x11u == rx[1u] && 0x01u == rx[2u])
	{
		Dcm_HardReset();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x40u == rx[4u] && 0x31u == rx[1u])
	{
		Dcm_RC_HealSupply();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x31u == rx[1u] && 0x43u == rx[4u])   /* choose routine ID = 0x50 */
	{
		Dcm_RC_ReadMinMaxAvg();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x42u == rx[4u] && 0x31u == rx[1u] && 3u == Dcm_ActiveSessionState)
	{
		Dcm_LoadControl();
	}
	else if(0x42u == rx[4u] && 0x31u == rx[1u] && 3u != Dcm_ActiveSessionState)
	{
		Dcm_SendNrc(DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);
	}
	else
	{
		/* Do nothing. */
	}

	if(0x10u == rx[1u] && 0x01u == rx[2u])
	{
		Dcm_DefaultSession();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x10u == rx[1u] && 0x03u == rx[2u])
	{
		Dcm_ExtendedSession();
	}
	else
	{
		/* Do nothing. */
	}

	if(0x3E == rx[1u] && 0x00u == rx[2u])
	{
		Dcm_TesterPresent();
	}
	else
	{
		/* Do nothing. */
	}

	Dcm_MainCounter++;
}
