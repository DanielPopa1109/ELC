#ifndef SMON_H
#define SMON_H

#include "main.h"
#include <stdint.h>
#include <math.h>
#include "stdlib.h"
#include "adc.h"
#include "tim.h"

typedef struct
{
	float Voltage_V;
	float VoltageL1_V;
	float Current_A;
	float VoltageFilt_V;
	float VoltageL1Filt_V;
	float CurrentFilt_A;
	float DeltaL1T30_V;
	float OcvEst_V;
	float SoC_pct;
	float SoC_CC_pct;
	float SoC_OCV_pct;
	float SoH_pct;
	float SoH_Rint_pct;
	float SoH_VoltageCap_pct;
	float WorstRestedVoltage_V;
	uint8_t WorstRestedVoltageValid;
	float InternalResistance_Ohm;
	float NominalCapacity_Ah;
	float AvailableCapacity_Ah;
	float AvgChargeCurrent_A;
	float AvgDischargeCurrent_A;
	float RuntimeRemaining_h;
	float TimeToFull_h;
	float Charge_Ah_Acc;
	float Charge_Wh_Acc;
	float Discharge_Ah_Acc;
	float Discharge_Wh_Acc;
	float Net_Ah_Acc;
	float Net_Wh_Acc;
	float Power_W;
	float ChargePower_W;
	float DischargePower_W;
	uint8_t State;
	uint8_t Charging;
	uint8_t Discharging;
	uint8_t ChargerPathActive;
	uint8_t BatteryRested;
	uint8_t ValidOcvCalibration;
	uint8_t WeakBattery;
	uint8_t DeepDischarge;
	uint8_t CrankingEvent;
	uint8_t Confidence_pct;
} SMon_BatteryData_t;

typedef struct
{
	uint8_t initialized;
	uint8_t prevState;
	uint8_t pendingState;
	uint8_t prevChargerPathActive;
	uint8_t lastRestVoltageValid;
	uint32_t pendingStateTicks;
	uint32_t chargePathOnTicks;
	uint32_t chargePathOffTicks;
	uint32_t restTicks;
	uint32_t chargeCurrentCnt;
	uint32_t dischargeCurrentCnt;
	float restReferenceV;
	float chargeCurrentSum_A;
	float dischargeCurrentSum_A;
	float lastRestVoltage_V;
} SMon_BatteryInternal_t;

#define SMON_BATT_DT_S                (0.005f)
#define SMON_BATT_DT_H                (SMON_BATT_DT_S / 3600.0f)
#define SMON_BATT_STATE_REST          (0u)
#define SMON_BATT_STATE_DISCHARGE     (1u)
#define SMON_BATT_STATE_CHARGE        (2u)

extern uint8_t SMon_NtcError;
extern uint8_t SMon_ExternalChargerDetected;
extern uint8_t SMon_RequestPhysicalStatus;
extern uint8_t SMon_CLSFlag;
extern uint8_t SMon_S2BErrorStatus;
extern uint8_t SMon_I2TError;
extern uint8_t SMon_L1_UVStatus;
extern uint8_t SMon_CLS_Failure;
extern uint8_t SMon_ECU_UV;
extern uint8_t SMon_ECU_OV;
extern uint8_t SMon_SWState;
extern uint8_t SMon_ShortToPlusTest;
extern uint8_t SMon_CmdStat;
extern uint8_t SMon_CalculatedCommand;
extern uint8_t SMon_L1ST;
extern uint8_t SMon_RetryCnt;
extern uint8_t SMon_LockSupply;
extern uint16_t SMon_VfbL1;
extern uint16_t SMon_VfbT30;
extern uint32_t SMon_I2TCounter;
extern uint32_t SMon_ISenseL1;
extern float SMon_NTC_Temperature_L1;
extern float SMon_ISenseL1_Float;
extern float SMon_PeakCurrent;
extern float SMon_ISenseL1_ExtChISense;
extern float SMon_McuTempValue;
extern SMon_BatteryData_t SMon_Battery;
extern uint32_t SMon_P_StatusVoltageL1Filter;
extern uint32_t SMon_P_I2TDebounceTime;
extern uint32_t SMon_P_Rtcntmax;
extern uint32_t SMon_P_CLSTime;
extern uint32_t SMon_P_WaitTimeOVUV;
extern uint32_t SMon_P_I2TDecrementPercentFactor;
extern uint32_t SMon_P_ClsFailureWaitTime;
extern uint32_t SMon_P_DischargeTimeCycles;
extern uint32_t SMon_P_LowVoltage;
extern uint32_t SMon_P_UV_KL30;
extern uint32_t SMon_P_OV_KL30;
extern uint32_t SMon_P_UV_CLS;
extern uint32_t SMon_P_Varef;
extern uint32_t SMon_P_ADC_MaxValue;
extern uint32_t SMon_P_NTC_PullUp_ResistorVale;
extern uint32_t SMon_P_LongDischargeTimeCycles;
extern uint32_t SMon_P_LowDisTimeCyc;
extern float SMon_P_VFB_T30_TwoPointCalibration_ParamA;
extern float SMon_P_VFB_T30_TwoPointCalibration_ParamB;
extern float SMon_P_VFB_L1_TwoPointCalibration_ParamA;
extern float SMon_P_VFB_L1_TwoPointCalibration_ParamB;
extern uint32_t SMon_P_NTC_L1_TwoPointCalibration_ParamA;
extern uint32_t SMon_P_NTC_L1_TwoPointCalibration_ParamB;
extern float SMon_P_ISenseNominal;
extern float SMon_P_I2TRating;
extern float SMon_P_RoomTempKelvin;
extern float SMon_P_VoltsAt25;
extern float SMon_P_AvgSlope;
extern float SMon_P_RoomTemperature;
extern float SMon_P_Kelvin;
extern float SMon_P_AlphaFilter;
extern float SMon_P_AlphaFilterExtChIsense;
extern float SMon_P_TwoPointCalib_ConvFacISense;
extern float SMon_P_TwoPointCalib_NoLoad_ISense;
extern float SMon_P_ExternalChargerThreshold;
extern float SMon_P_NTCTemperatureMax;
extern float SMon_P_NTCTemperatureRelease;
extern float SMon_P_BattNominalCapacity_Ah      ;
extern float SMon_P_BattMinSoC_pct              ;
extern float SMon_P_BattMaxSoC_pct              ;
extern float SMon_P_BattRestVoltDelta_V         ;
extern uint32_t SMon_P_BattRestTimeTicks        ;
extern float SMon_P_BattWeakVolt_V              ;
extern float SMon_P_BattDeepDischargeVolt_V     ;
extern float SMon_P_BattCrankVolt_V             ;
extern float SMon_P_BattAlphaVolt               ;
extern float SMon_P_BattAlphaCurr               ;
extern float SMon_P_BattAlphaRint               ;
extern float SMon_P_BattChargeEfficiency        ;
extern float SMon_P_BattOcvCorrectionGain       ;
extern float SMon_P_BattSoHMin_pct              ;
extern float SMon_P_BattSoHMax_pct              ;
extern float SMon_P_BattNominalRint_Ohm         ;
extern float SMon_P_BattBadRint_Ohm             ;
extern float SMon_P_BattCurrentDeadband_A;
extern float SMon_P_BattCurrentHys_A            ;
extern float SMon_P_BattChargePathDeltaOn_V     ;
extern float SMon_P_BattChargePathDeltaOff_V    ;
extern float SMon_P_BattRintMinStep_A           ;
extern float SMon_P_BattRestDetectCurrent_A;

extern void SMon_main(void);

#endif /* SMON_H */
