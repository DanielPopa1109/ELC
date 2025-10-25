#include "SMon.h"
#include "tim.h"
#include "main.h"
#include <stdint.h>
#include <math.h>

#define HIST_MAX_SAMPLES 30

typedef struct
{
	uint32_t samples[HIST_MAX_SAMPLES];
	uint16_t idx;
	uint16_t count;
	uint64_t sumsq;
	uint16_t size;
	uint16_t padding16;
}SMon_HistWindow;

typedef struct
{
	SMon_HistWindow win5s;
	SMon_HistWindow win10s;
	SMon_HistWindow win30s;
}SMon_SignalHist;

static SMon_SignalHist SMon_ISenseL1_Hist =
{
		.win5s = { .size=5 },
		.win10s = { .size=10 },
		.win30s = { .size=30 }
};
static SMon_SignalHist SMon_VfbT30_Hist =
{
		.win5s = { .size=5 },
		.win10s = { .size=10 },
		.win30s = { .size=30 }
};
static SMon_SignalHist SMon_VfbL1_Hist =
{
		.win5s = { .size=5 },
		.win10s = { .size=10 },
		.win30s = { .size=30 }
};

uint8_t SMon_FastResponseTrigger = 0u;
uint8_t SMon_RequestPhysicalStatus = 0xFF;
uint8_t SMon_CLSFlag = 0u; // CLS status flag - not started / running / done
uint8_t SMon_S2BErrorStatus = 0u;
uint8_t SMon_I2TError = 0u;
uint8_t SMon_L1_UVStatus = 0u;
uint8_t SMon_CLS_Failure = 0u;
uint8_t SMon_ECU_UV = 0u;
uint8_t SMon_ECU_OV = 0u;
uint8_t SMon_WupLineState = 0u; // SYS_WKUP
uint8_t SMon_SWState = 0u; // EcuM SW state
uint8_t SMon_ValidMeasFlag = 0u; // ADC valid measurement
uint8_t SMon_ShortToPlusTest = 0u; // Discharge Test Status
uint8_t SMon_CmdStat = 0xFF; // Command Status
uint8_t SMon_L1ST; // L1 Status
uint8_t SMon_RetryCnt; // Retry Counter
uint8_t SMon_LockSupply; // Lock Supply Output
uint16_t SMon_VfbL1 = 0xFFFFu; // Voltage Feedback L1/CLS
uint16_t SMon_VfbT30 = 0xFFFFu; // Voltage Feedback KL30
static uint16_t SMon_T30P50 = 0u; // 50% of KL30
static uint32_t SMon_MainCnt = 0u; // Main Counter
static uint32_t SMon_CounterUVL1 = 0u; // UV L1 Counter  For De-Bounce
static uint32_t SMon_CLSCheckVoltage_Timestamp;
uint32_t SMon_I2TCounter = 0u; // I2T Counter
uint32_t SMon_ISenseL1 = 0; // I Sense L1
uint32_t SMon_ISenseL1_RMS_5s;
uint32_t SMon_ISenseL1_RMS_10s;
uint32_t SMon_ISenseL1_RMS_30s;
uint32_t SMon_VfbT30_RMS_5s;
uint32_t SMon_VfbT30_RMS_10s;
uint32_t SMon_VfbT30_RMS_30s;
uint32_t SMon_VfbL1_RMS_5s;
uint32_t SMon_VfbL1_RMS_10s;
uint32_t SMon_VfbL1_RMS_30s;
float SMon_ISenseL1_Float;
float SMon_PeakCurrent;

const uint8_t SMon_P_Rtcntmax = 10u; // Retry Counter Parameter
const uint8_t SMon_P_CLSTime = 22u; // CLS Duration Parameter
const uint8_t SMon_P_WaitTimeOVUV = 60u; // OV UV De-bounce Time Parameter
const uint8_t SMon_P_WaitTimeCPC = 10u; // Wait Before Changing States For CPC Parameter
const uint8_t SMon_P_I2TDecrementPercentFactor = 85u; // Cooling Off Factor Parameter
const uint16_t SMon_P_ClsFailureWaitTime = 300u; // Wait Time Between CLS Retries Parameter
const uint16_t SMon_P_DischargeTimeCycles = 50600u; // ~50% Starting Voltage Discharge Time Parameter
const uint16_t SMon_P_LowVoltage = 800u; // 5TAU / Maximum Discharge Voltage Threshold Parameter
const uint16_t SMon_P_UV_KL30 = 7000u; // UV KL30 TH Parameter
const uint16_t SMon_P_OV_KL30 = 17000u; // OV KL30 TH Parameter
const uint16_t SMon_P_UV_CLS = 1633u; // Threshold For CLS OUT To Be After 5ms Parameter
const uint32_t SMon_P_LongDischargeTimeCycles = 1044000u; // Maximum Discharge Time Parameter
const uint32_t SMon_P_LowDisTimeCyc = 420000u; // 2TAU Discharge Time Parameter
const uint32_t SMon_P_PeakCurrent = 60000u;
const float SMon_P_ISenseNominal = 10.5; // Nominal Current Parameter
const float SMon_P_I2TRating = 1640; // I2T Rating Parameter

static uint32_t SMon_UpdateHistWindow(SMon_HistWindow* hw, uint32_t new_v);
static void SMon_UpdateSignalHist(SMon_SignalHist* sh, uint32_t value, uint32_t* rms5, uint32_t* rms10, uint32_t* rms30);
static void SMon_LoadCurrentErrorState(void);
static void SMon_LoadSwitchState(void);
static void SMon_HistogramsHandler(void);
static void SMon_ProcessShortToPlusTest(void);
static void SMon_ProcessLoadCurrentState(void);
static void SMon_I2TAccumulation(void);
static void SMon_ProcessEcuVoltageState(void);
static void SMon_ProcessLoadErrorStatus(void);
static void SMon_LoadSwitchingLogic(void);
static void SMon_LoadSwitchingDiagnosis(void);
void SMon_main(void);

static void SMon_I2TAccumulation(void)
{
	static float I2TCounter_Float;
	static float ISenseL1_Average_Float;
	static float ISenseL1_Avged_Float;
	static float DeltaAmper_Float;
	static uint8_t localI2tAvgCounter = 0u;

	if(1u == SMon_CmdStat)
	{
		if(20u > localI2tAvgCounter)
		{
			localI2tAvgCounter++;
			ISenseL1_Average_Float += SMon_ISenseL1_Float;
		}
		else
		{
			ISenseL1_Avged_Float = ISenseL1_Average_Float / localI2tAvgCounter;
			localI2tAvgCounter = 0u;
			ISenseL1_Average_Float = 0u;
		}
	}
	else
	{
		/* Do nothing. */
	}

	if(1u == SMon_FastResponseTrigger)
	{
		SMon_FastResponseTrigger = 0u;
		ISenseL1_Avged_Float = SMon_PeakCurrent;
	}
	else
	{
		/* Do nothing. */
	}

	if(SMon_P_ISenseNominal < ISenseL1_Avged_Float && 0u == SMon_I2TError && 1u == SMon_CmdStat)
	{
		DeltaAmper_Float = (ISenseL1_Avged_Float * ISenseL1_Avged_Float - SMon_P_ISenseNominal * SMon_P_ISenseNominal) / 10u;
		I2TCounter_Float += DeltaAmper_Float;
		SMon_I2TCounter = (uint32_t)I2TCounter_Float;
	}
	else
	{
		if(SMon_MainCnt % 20u == 0)
		{
			I2TCounter_Float = (I2TCounter_Float * SMon_P_I2TDecrementPercentFactor) / 100;
			SMon_I2TCounter = (uint32_t)I2TCounter_Float;

			if(1 > I2TCounter_Float)
			{
				I2TCounter_Float = 0;
				SMon_I2TCounter = (uint32_t)I2TCounter_Float;
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

	if(I2TCounter_Float > SMon_P_I2TRating)
	{
		SMon_I2TError = 1u;
	}
	else if(0 == I2TCounter_Float && 1u == SMon_I2TError)
	{
		SMon_I2TError = 0u;
	}
	else
	{
		/* Do nothing. */
	}
}

static uint32_t SMon_UpdateHistWindow(SMon_HistWindow* hw, uint32_t new_v)
{
	if(hw->count == hw->size)
	{
		hw->sumsq -= hw->samples[hw->idx] * hw->samples[hw->idx];
	}
	else
	{
		hw->count++;
	}

	hw->samples[hw->idx] = new_v;
	hw->sumsq += new_v * new_v;
	hw->idx = (hw->idx + 1) % hw->size;

	return (uint32_t)sqrtf(hw->sumsq / hw->count);
}

static void SMon_UpdateSignalHist(SMon_SignalHist* sh, uint32_t value, uint32_t* rms5, uint32_t* rms10, uint32_t* rms30)
{
	*rms5  = SMon_UpdateHistWindow(&sh->win5s,  value);
	*rms10 = SMon_UpdateHistWindow(&sh->win10s, value);
	*rms30 = SMon_UpdateHistWindow(&sh->win30s, value);
}

static void SMon_ProcessShortToPlusTest(void)
{
	static uint32_t localDischargeTimer = 0u; // normal discharge counter
	static uint32_t localLowDisTimer = 0u; // very low voltage discharge counter
	static uint32_t localS2BCounter = 0u; // longer case discharge time counter

	if(1u != SMon_CmdStat)
	{
		if(0u == SMon_ShortToPlusTest)
		{
			SMon_ShortToPlusTest = 1u; // start short to plus test
		}
		else
		{
			/* Do nothing. */
		}
	}
	else if(1u == SMon_CmdStat)
	{
		SMon_ShortToPlusTest = 0u; // reset short to plus test status
		localDischargeTimer = 0u; // reset normal discharge counter
		localLowDisTimer = 0u; // reset very low voltage discharge counter
		localS2BCounter = 0u; // reset longer case discharge time counter
	}
	else
	{
		/* Do nothing. */
	}

	if(0u != SMon_ShortToPlusTest)
	{
		if(0u == localDischargeTimer) // "first call"
		{
			localDischargeTimer = SMon_MainCnt; // time-stamp
			localLowDisTimer = SMon_MainCnt; // time-stamp
			localS2BCounter = SMon_MainCnt; // time-stamp
		}
		else
		{
			switch(SMon_ShortToPlusTest) // short-to-plus test phases
			{
			case 1u: // 50% of voltage discharge time check
				if(SMon_P_DischargeTimeCycles <= (SMon_MainCnt - localDischargeTimer))
				{
					if(SMon_T30P50 <= SMon_VfbL1)
					{
						SMon_ShortToPlusTest = 2u; // go-to next phase
						SMon_S2BErrorStatus = 1u;
					}
					else
					{
						/* Do nothing. */
					}
				}
				else
				{
					if(SMon_T30P50 > SMon_VfbL1) // abort this test phase
					{
						SMon_ShortToPlusTest = 2u; // go-to next phase
						SMon_S2BErrorStatus = 0u;
					}
					else
					{
						/* Do nothing. */
					}
				}
				break;
			case 2u:
				if(SMon_P_LowDisTimeCyc <= (SMon_MainCnt - localLowDisTimer))
				{
					if((SMon_VfbT30 * 3 / 10) <= SMon_VfbL1)
					{
						SMon_ShortToPlusTest = 3u; // go-to next phase
						SMon_S2BErrorStatus = 2u;
					}
					else
					{
						/* Do nothing. */
					}
				}
				else
				{
					if((SMon_VfbT30 * 3 / 10) > SMon_VfbL1) // abort this test phase
					{
						SMon_ShortToPlusTest = 3u; // go-to next phase
						SMon_S2BErrorStatus = 0u;
					}
					else
					{
						/* Do nothing. */
					}
				}
				break;
			case 3u:
				if(SMon_P_LongDischargeTimeCycles <= (SMon_MainCnt - localS2BCounter))
				{
					if(SMon_P_LowVoltage <= SMon_VfbL1)
					{
						SMon_S2BErrorStatus = 3u;
						SMon_ShortToPlusTest = 4u; // finish short to plus test
					}
					else
					{
						/* Do nothing. */
					}
				}
				else
				{
					if(SMon_P_LowVoltage > SMon_VfbL1) // abort this test phase
					{
						SMon_ShortToPlusTest = 4u; // finish short to plus test
						SMon_S2BErrorStatus = 0u;
					}
					else
					{
						/* Do nothing. */
					}
				}
				break;
			default: // do nothing
				break;
			}
		}
	}
	else
	{
		/* Do nothing. */
	}
}

static void SMon_ProcessLoadCurrentState(void)
{
	SMon_T30P50 = ((SMon_VfbT30 * 5u) / 10u); // get 50% of T30

	if(SMon_VfbL1 >= SMon_T30P50)
	{
		SMon_L1ST = 1u;
	}
	else
	{
		SMon_L1ST = 0u;
	}

	if(1u == SMon_CmdStat && 0u == SMon_CLS_Failure && 0u == SMon_L1_UVStatus)
	{
		SMon_RequestPhysicalStatus = 1u;
	}
	else
	{
		SMon_RequestPhysicalStatus = 0u;
	}
}

static void SMon_ProcessEcuVoltageState(void)
{
	static uint32_t CounterUVKL30 = 0u; // UV Counter For De-Bounce
	static uint32_t CounterOVKL30 = 0u; // OV Counter For De-Bounce

	if(SMon_P_UV_KL30 <= SMon_VfbT30 && SMon_P_OV_KL30 >= SMon_VfbT30) // UV OV checks
	{
		CounterUVKL30 = 0u; // reset counters
		CounterOVKL30 = 0u; // reset counters
		SMon_ECU_UV = 0u;
		SMon_ECU_OV = 0u;
	}
	else
	{
		if(SMon_P_UV_KL30 > SMon_VfbT30) // under-voltage
		{
			if(0u == CounterUVKL30) // "first call"
			{
				CounterUVKL30 = SMon_MainCnt; // time-stamp
			}
			else
			{
				if(SMon_P_WaitTimeOVUV < (SMon_MainCnt - CounterUVKL30)) // de-bounce for UV
				{
					SMon_ECU_UV = 1u;
				}
				else
				{
					SMon_ECU_UV = 0u;
				}
			}
		}
		else
		{
			CounterUVKL30 = 0u; // reset counter
		}

		if(SMon_P_OV_KL30 < SMon_VfbT30) // over-voltage
		{
			if(0u == CounterOVKL30) // "first call"
			{
				CounterOVKL30 = SMon_MainCnt; // time-stamp
			}
			else
			{
				if(SMon_P_WaitTimeOVUV < (SMon_MainCnt - CounterOVKL30)) // de-bounce for OV - 300ms
				{
					SMon_ECU_OV = 1u;
				}
				else
				{
					SMon_ECU_OV = 0u;
				}
			}
		}
		else
		{
			CounterOVKL30 = 0u; // reset counter
		}
	}
}

static void SMon_ProcessLoadErrorStatus(void)
{
	static uint8_t pSMon_I2TError = 0u;
	static uint8_t pSMon_CLS_Failure = 0u;
	static uint8_t pSMon_CmdStat = 0u;
	static uint32_t localTimeStamp = 0u;

	if(1u == SMon_CLS_Failure)
	{
		if(0u == localTimeStamp)
		{
			localTimeStamp = SMon_MainCnt;
		}
		else
		{
			if(SMon_P_ClsFailureWaitTime < (SMon_MainCnt - localTimeStamp))
			{
				SMon_CLS_Failure = 0u;
				localTimeStamp = 0u;
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

	if(1u == SMon_ECU_OV || 1u == SMon_ECU_UV)
	{
		SMon_RequestPhysicalStatus = 0u;
	}
	else
	{
		/* Do nothing. */
	}

	if(0u != SMon_LockSupply)
	{
		SMon_RequestPhysicalStatus = 0u;
	}
	else
	{
		/* Do nothing. */
	}

	if(1u == SMon_I2TError)
	{
		SMon_RequestPhysicalStatus = 0u;
	}
	else
	{
		/* Do nothing. */
	}

	if(1u == pSMon_CLS_Failure && 0u == SMon_CLS_Failure && 0u == SMon_LockSupply)
	{
		pSMon_CLS_Failure = SMon_CLS_Failure;
		SMon_RetryCnt++;
	}
	else
	{
		/* Do nothing. */
	}

	if(1u == pSMon_I2TError && 0u == SMon_I2TError && 0u == SMon_LockSupply)
	{
		pSMon_I2TError = SMon_I2TError;
		SMon_RetryCnt++;
	}
	else
	{
		/* Do nothing. */
	}

	if(SMon_P_Rtcntmax == SMon_RetryCnt)
	{
		SMon_LockSupply = 1u;
	}
	else
	{
		/* Do nothing. */
	}

	if(1u == SMon_L1_UVStatus && 0u == SMon_CmdStat && 1u == pSMon_CmdStat)
	{
		pSMon_CmdStat = SMon_CmdStat;
		SMon_L1_UVStatus = 0u;
		SMon_RequestPhysicalStatus = 0u;
		SMon_CounterUVL1 = 0u; // reset counter
	}
	else
	{
		/* Do nothing. */
	}

	if(pSMon_CmdStat != SMon_CmdStat)
	{
		pSMon_CmdStat = SMon_CmdStat;
	}
	else
	{
		/* Do nothing. */
	}

	if(pSMon_I2TError != SMon_I2TError)
	{
		pSMon_I2TError = SMon_I2TError;
	}
	else
	{
		/* Do nothing. */
	}

	if(pSMon_CLS_Failure != SMon_CLS_Failure)
	{
		pSMon_CLS_Failure = SMon_CLS_Failure;
	}
	else
	{
		/* Do nothing. */
	}
}

static void SMon_LoadSwitchingDiagnosis(void)
{
	if(1u == SMon_CLSFlag) // CLS ongoing
	{
		if(SMon_P_CLSTime - 20u <= (SMon_MainCnt - SMon_CLSCheckVoltage_Timestamp))
		{
			if(SMon_P_UV_CLS <= SMon_VfbL1) // voltage is rising, no error
			{
				SMon_CLS_Failure = 0u;
			}
			else
			{
				SMon_CLS_Failure = 1u;
				SMon_RequestPhysicalStatus = 0u;
			}
		}
		else
		{
			/* Do nothing. */
		}

		if(SMon_P_CLSTime - 14u <= (SMon_MainCnt - SMon_CLSCheckVoltage_Timestamp))
		{
			if(SMon_T30P50 <= SMon_VfbL1) // voltage is rising, no error
			{
				SMon_CLS_Failure = 0u;
			}
			else
			{
				SMon_CLS_Failure = 1u;
				SMon_RequestPhysicalStatus = 0u;
			}
		}
		else
		{
			/* Do nothing. */
		}
	}
	else if(2u == SMon_CLSFlag) // CLS is done
	{
		if(SMon_T30P50 <= SMon_VfbL1) // No UV on output
		{
			SMon_L1_UVStatus = 0u;
			SMon_CounterUVL1 = 0u; // reset counter
		}
		else
		{
			if(SMon_VfbL1 < SMon_T30P50)
			{
				if(0u == SMon_CounterUVL1) // "first call"
				{
					SMon_CounterUVL1 = SMon_MainCnt; // time-stamp
				}
				else
				{
					if(SMon_P_WaitTimeOVUV < (SMon_MainCnt - SMon_CounterUVL1)) // de-bounce for UV - 300ms
					{
						SMon_L1_UVStatus = 1u;
						SMon_RequestPhysicalStatus = 0u;
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
		}
	}
	else
	{
		/* Do nothing. */
	}
}

static void SMon_LoadSwitchingLogic(void)
{
	static uint8_t localCPCFlag = 0u; // CPC status flag - off / on
	static uint32_t localCLSCounter = 0u; // time to wait with CLS active before changing CPC flag states
	static uint32_t localCPCCounter = 0u; // time to wait with CPC counter before changing CPC flag states

	if(1u == SMon_RequestPhysicalStatus)
	{
		if(0u == localCPCFlag) // CPC off
		{
			localCPCFlag = 1u; // CPC ongoing
			localCPCCounter = SMon_MainCnt; // time-stamp
			htim1.Instance->CCR1 = 257u; // switch on CPC
		}
		else if(1u == localCPCFlag) // CPC ongoing
		{
			if(SMon_P_WaitTimeCPC <= (SMon_MainCnt - localCPCCounter)) // de-bounce time 50ms
			{
				localCPCFlag = 2u; // CPC finished
				localCPCCounter = 0u; // reset counter
			}
			else
			{
				/* Do nothing. */
			}
		}
		else if(2u == localCPCFlag) // CPC finished
		{
			if(0u == SMon_CLSFlag) // CLS off
			{
				if(0u == localCLSCounter) // "first call"
				{
					localCLSCounter = SMon_MainCnt; // time-stamp
					SMon_CLSCheckVoltage_Timestamp = SMon_MainCnt;
					SMon_CLSFlag = 1u; // CLS ongoing
					HAL_GPIO_WritePin(ENL1CLS_GPIO_Port, ENL1CLS_Pin, 1u); // switch on CLS circuit
				}
				else
				{
					/* Do nothing. */
				}
			}
			else if(1u == SMon_CLSFlag) // CLS ongoing
			{
				if(SMon_P_CLSTime <= (SMon_MainCnt - localCLSCounter)) // de-bounce time based on parameter
				{
					localCLSCounter = 0u; // reset counter
					SMon_CLSFlag = 2u; // CLS finished
					HAL_GPIO_WritePin(ENL1CLS_GPIO_Port, ENL1CLS_Pin, 0u); // switch off CLS circuit
				}
				else if(SMon_P_CLSTime - 1u <= (SMon_MainCnt - localCLSCounter))
				{
					HAL_GPIO_WritePin(ENL1_GPIO_Port, ENL1_Pin, 1u); // switch on L1 circuit
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
		localCPCFlag = 0u; // reset flag
		SMon_CLSFlag = 0u; // reset flag
		SMon_CounterUVL1 = 0u; // reset counter
		localCLSCounter = 0u; // reset counter
		localCPCCounter = 0u; // reset counter
		SMon_CLSCheckVoltage_Timestamp = 0u;
		htim1.Instance->CCR1 = 0u; // switch off CPC
		HAL_GPIO_WritePin(ENL1CLS_GPIO_Port, ENL1CLS_Pin, 0u); // switch off CLS
		HAL_GPIO_WritePin(ENL1_GPIO_Port, ENL1_Pin, 0u); // switch off L1
	}
}

static void SMon_LoadCurrentErrorState(void)
{
	SMon_ProcessLoadCurrentState();
	SMon_ProcessLoadErrorStatus();
	SMon_I2TAccumulation();
}

static void SMon_LoadSwitchState(void)
{
	SMon_LoadSwitchingDiagnosis();
	SMon_LoadSwitchingLogic();
	SMon_ProcessShortToPlusTest();
}

static void SMon_HistogramsHandler(void)
{
	SMon_UpdateSignalHist(&SMon_ISenseL1_Hist, SMon_ISenseL1, &SMon_ISenseL1_RMS_5s, &SMon_ISenseL1_RMS_10s, &SMon_ISenseL1_RMS_30s);
	SMon_UpdateSignalHist(&SMon_VfbT30_Hist, SMon_VfbT30, &SMon_VfbT30_RMS_5s, &SMon_VfbT30_RMS_10s, &SMon_VfbT30_RMS_30s);
	SMon_UpdateSignalHist(&SMon_VfbL1_Hist, SMon_VfbL1, &SMon_VfbL1_RMS_5s, &SMon_VfbL1_RMS_10s, &SMon_VfbL1_RMS_30s);
}

void SMon_main(void)
{
	SMon_LoadCurrentErrorState();
	SMon_ProcessEcuVoltageState();
	SMon_LoadSwitchState();

	if(0u != SMon_MainCnt && SMon_MainCnt % 200u == 0u)
	{
		SMon_HistogramsHandler();
	}
	else
	{
		/* Do nothing. */
	}

	SMon_MainCnt++;
}
