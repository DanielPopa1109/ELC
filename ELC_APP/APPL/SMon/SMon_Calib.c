/* SMon_Calib.c - SMon calibration image (A2L SMon_P_*) loader.
 *
 * The 292-byte calibration page at 0x08003800 holds 72 little-endian 32-bit slots (integers
 * and IEEE floats) followed by the version byte [290] and the format marker 0xAA [291].
 * The slot table below is the single description of the layout: target variable and the
 * built-in default. The APP never erases or rewrites this page; if the stored image is not
 * valid the defaults are used from RAM only and the image buffer mirrors them.
 *
 * Integrity: SMon_CalibIntegrity = 0 defaults/invalid, 1 legacy image accepted without a
 * checksum (the original firmware stored none), 2 CRC-validated CAL2 image
 * (word0 = CRC32 of bytes 8..291, word1 = "CAL2" 0x324C4143). */
#include "SMon.h"
#include "../../BSW/Mem/Fls/Fls.h"
#include "../../BSW/Sys/Crc/Crc.h"
#include <string.h>

#define SMON_CALIB_SLOTS      72u
#define SMON_CALIB_SIZE       292u
#define SMON_CALIB_VERSION_AT 290u
#define SMON_CALIB_MARKER_AT  291u
#define SMON_CALIB_MARKER     0xAAu
#define SMON_CALIB_VERSION    0x21u
#define SMON_CALIB_CAL2       0x324C4143u

typedef union
{
	uint32_t u;
	float    f;
} SMon_CalibValue_t;

typedef struct
{
	void             *target;   /* NULL: unused legacy slot, kept for layout compatibility */
	SMon_CalibValue_t def;
} SMon_CalibSlot_t;

uint8_t SMon_CalibImage[SMON_CALIB_SIZE];
uint8_t SMon_CalibIntegrity;

static const SMon_CalibSlot_t SMon_CalibSlots[SMON_CALIB_SLOTS] =
{
    /*  0 */ { NULL, { 0u } },  /* unused legacy slot */
    /*  1 */ { NULL, { 0u } },  /* unused legacy slot */
    /*  2 */ { &SMon_P_DischargeTimeCycles, { .u = 132000u } },  // ~50% Starting Voltage Discharge Time Parameter
    /*  3 */ { &SMon_P_NTC_PullUp_ResistorVale, { .u = 91000u } },
    /*  4 */ { &SMon_P_LongDischargeTimeCycles, { .u = 564000u } },  // Maximum Discharge Time Parameter
    /*  5 */ { &SMon_P_LowDisTimeCyc, { .u = 229600 } },  // 2TAU Discharge Time Parameter
    /*  6 */ { &SMon_P_ClsFailureWaitTime, { .u = 190u } },  // Wait Time Between CLS Retries Parameter
    /*  7 */ { &SMon_P_LowVoltage, { .u = 1200u } },  // 5TAU / Maximum Discharge Voltage Threshold Parameter
    /*  8 */ { &SMon_P_UV_KL30, { .u = 6900u } },  // UV KL30 TH Parameter
    /*  9 */ { &SMon_P_OV_KL30, { .u = 17200u } },  // OV KL30 TH Parameter
    /* 10 */ { &SMon_P_UV_CLS, { .u = 700u } },
    /* 11 */ { &SMon_P_Varef, { .u = 3300u } },
    /* 12 */ { &SMon_P_ADC_MaxValue, { .u = 4095u } },
    /* 13 */ { NULL, { 0u } },  /* unused legacy slot */
    /* 14 */ { &SMon_P_NTC_L1_TwoPointCalibration_ParamB, { .u = 4984u } },  // Beta_cal in K
    /* 15 */ { &SMon_P_StatusVoltageL1Filter, { .u = 250u } },
    /* 16 */ { &SMon_P_I2TDebounceTime, { .u = 20u } },
    /* 17 */ { &SMon_P_Rtcntmax, { .u = 13u } },  // Retry Counter Parameter
    /* 18 */ { &SMon_P_CLSTime, { .u = 22u } },  // CLS Duration Parameter
    /* 19 */ { &SMon_P_WaitTimeOVUV, { .u = 80u } },  // OV UV De-bounce Time Parameter
    /* 20 */ { NULL, { 0u } },  /* unused legacy slot */
    /* 21 */ { &SMon_P_I2TDecrementPercentFactor, { .u = 85u } },  // Cooling Off Factor Parameter
    /* 22 */ { &SMon_P_NTC_L1_TwoPointCalibration_ParamA, { .u = 12332u } },  // R0_cal in ohms
    /* 23 */ { &SMon_P_BattRestTimeTicks, { .u = 200u } },  /* tune to scheduler tick */
    /* 24 */ { &SMon_P_VFB_T30_TwoPointCalibration_ParamA, { .f = 8.16f } },
    /* 25 */ { &SMon_P_VFB_T30_TwoPointCalibration_ParamB, { .f = 21.93f } },
    /* 26 */ { &SMon_P_VFB_L1_TwoPointCalibration_ParamA, { .f = 8.13f } },
    /* 27 */ { &SMon_P_VFB_L1_TwoPointCalibration_ParamB, { .f = 8.13f } },
    /* 28 */ { &SMon_P_ISenseNominal, { .f = 28.000f } },  // Nominal Current Parameter
    /* 29 */ { &SMon_P_I2TRating, { .f = 1176.0f } },  // I2T Rating Parameter
    /* 30 */ { &SMon_P_RoomTempKelvin, { .f = 297.15f } },
    /* 31 */ { &SMon_P_VoltsAt25, { .f = 1430.0f } },
    /* 32 */ { &SMon_P_AvgSlope, { .f = 4.30f } },
    /* 33 */ { &SMon_P_RoomTemperature, { .f = 25.0f } },
    /* 34 */ { &SMon_P_Kelvin, { .f = 273.15f } },
    /* 35 */ { NULL, { 0u } },  /* unused legacy slot */
    /* 36 */ { &SMon_P_AlphaFilter, { .f = 0.90f } },
    /* 37 */ { &SMon_P_AlphaFilterExtChIsense, { .f = 0.0001f } },
    /* 38 */ { &SMon_P_TwoPointCalib_ConvFacISense, { .f = 15.91f } },
    /* 39 */ { &SMon_P_TwoPointCalib_NoLoad_ISense, { .f = 613.2f } },
    /* 40 */ { &SMon_P_ExternalChargerThreshold, { .f = -1.0f } },
    /* 41 */ { &SMon_P_NTCTemperatureMax, { .f = 90.0f } },
    /* 42 */ { &SMon_P_NTCTemperatureRelease, { .f = 80.0f } },
    /* 43 */ { &SMon_P_BattNominalCapacity_Ah, { .f = 77.0f } },
    /* 44 */ { NULL, { 0u } },  /* unused legacy slot */
    /* 45 */ { &SMon_P_BattMinSoC_pct, { .f = 1.0f } },
    /* 46 */ { &SMon_P_BattMaxSoC_pct, { .f = 100.0f } },
    /* 47 */ { NULL, { 0u } },  /* unused legacy slot */
    /* 48 */ { &SMon_P_BattRestVoltDelta_V, { .f = 0.02f } },
    /* 49 */ { &SMon_P_BattWeakVolt_V, { .f = 11.80f } },
    /* 50 */ { &SMon_P_BattDeepDischargeVolt_V, { .f = 11.10f } },
    /* 51 */ { &SMon_P_BattCrankVolt_V, { .f = 9.60f } },
    /* 52 */ { &SMon_P_BattAlphaVolt, { .f = 0.02f } },
    /* 53 */ { &SMon_P_BattAlphaCurr, { .f = 0.05f } },
    /* 54 */ { &SMon_P_BattAlphaRint, { .f = 0.10f } },
    /* 55 */ { &SMon_P_BattChargeEfficiency, { .f = 0.90f } },
    /* 56 */ { &SMon_P_BattOcvCorrectionGain, { .f = 0.20f } },
    /* 57 */ { &SMon_P_BattSoHMin_pct, { .f = 1.0f } },
    /* 58 */ { &SMon_P_BattSoHMax_pct, { .f = 100.0f } },
    /* 59 */ { &SMon_P_BattNominalRint_Ohm, { .f = 0.015f } },
    /* 60 */ { &SMon_P_BattBadRint_Ohm, { .f = 0.060f } },
    /* 61 */ { NULL, { 0u } },  /* unused legacy slot */
    /* 62 */ { &SMon_P_BattCurrentDeadband_A, { .f = 1.0f } },
    /* 63 */ { NULL, { 0u } },  /* unused legacy slot */
    /* 64 */ { &SMon_P_BattCurrentHys_A, { .f = 0.30f } },
    /* 65 */ { &SMon_P_BattChargePathDeltaOn_V, { .f = 0.25f } },
    /* 66 */ { &SMon_P_BattChargePathDeltaOff_V, { .f = 0.10f } },
    /* 67 */ { &SMon_P_BattRintMinStep_A, { .f = 0.20f } },
    /* 68 */ { NULL, { 0u } },  /* unused legacy slot */
    /* 69 */ { NULL, { 0u } },  /* unused legacy slot */
    /* 70 */ { NULL, { 0u } },  /* unused legacy slot */
    /* 71 */ { &SMon_P_BattRestDetectCurrent_A, { .f = 0.30f } }
};

/* Slots that carry an IEEE float: a NaN/Inf pattern invalidates the image. */
static const uint8_t SMon_CalibFloatSlots[] =
{
	24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u, 33u, 34u, 36u, 37u, 38u, 39u, 40u,
	41u, 42u, 43u, 45u, 46u, 48u, 49u, 50u, 51u, 52u, 53u, 54u, 55u, 56u, 57u, 58u,
	59u, 60u, 62u, 64u, 65u, 66u, 67u, 71u
};

/* Integer slots used as divisors must be non-zero: Varef, ADC_MaxValue, NTC A/B. */
static const uint8_t SMon_CalibNonZeroSlots[] = { 11u, 12u, 14u, 22u };

static uint32_t SMon_CalibWord(uint8_t slot)
{
	uint32_t word;

	(void)memcpy(&word, &SMon_CalibImage[4u * slot], 4u);
	return word;
}

static uint8_t SMon_CalibFormatValid(void)
{
	uint8_t i;

	SMon_CalibIntegrity = 0u;
	if (SMon_CalibImage[SMON_CALIB_MARKER_AT] != SMON_CALIB_MARKER)
	{
		return 0u;
	}
	for (i = 0u; i < sizeof(SMon_CalibFloatSlots); ++i)
	{
		if ((SMon_CalibWord(SMon_CalibFloatSlots[i]) & 0x7F800000u) == 0x7F800000u)
		{
			return 0u;
		}
	}
	for (i = 0u; i < sizeof(SMon_CalibNonZeroSlots); ++i)
	{
		if (SMon_CalibWord(SMon_CalibNonZeroSlots[i]) == 0u)
		{
			return 0u;
		}
	}
	if (SMon_CalibWord(1u) == SMON_CALIB_CAL2)
	{
		if (SMon_CalibWord(0u) != Crc_CalculateCRC32(&SMon_CalibImage[8], 284u, 0u, 1u))
		{
			return 0u;
		}
		SMon_CalibIntegrity = 2u;
	}
	else
	{
		SMon_CalibIntegrity = 1u;
	}
	return 1u;
}

void SMon_CalibInit(void)
{
	uint8_t slot;

	(void)Fls_Read(FLS_CAL_BASE_ADDRESS, SMon_CalibImage, SMON_CALIB_SIZE);
	if (SMon_CalibFormatValid() != 0u)
	{
		for (slot = 0u; slot < SMON_CALIB_SLOTS; ++slot)
		{
			if (SMon_CalibSlots[slot].target != NULL)
			{
				(void)memcpy(SMon_CalibSlots[slot].target, &SMon_CalibImage[4u * slot], 4u);
			}
		}
	}
	else
	{
		for (slot = 0u; slot < SMON_CALIB_SLOTS; ++slot)
		{
			if (SMon_CalibSlots[slot].target != NULL)
			{
				(void)memcpy(SMon_CalibSlots[slot].target, &SMon_CalibSlots[slot].def, 4u);
				(void)memcpy(&SMon_CalibImage[4u * slot], &SMon_CalibSlots[slot].def, 4u);
			}
		}
		SMon_CalibImage[SMON_CALIB_MARKER_AT] = SMON_CALIB_MARKER;
		SMon_CalibImage[SMON_CALIB_VERSION_AT] = SMON_CALIB_VERSION;
		/* Defaults stay in RAM: never erase or rewrite the calibration page at start-up. */
	}
}

uint8_t SMon_CalibGetVersion(void)
{
	return SMon_CalibImage[SMON_CALIB_VERSION_AT];
}
