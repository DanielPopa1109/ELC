#include "Nvm.h"
#include "Dem.h"
#include "SMon.h"
#include "NvmStore.h"
#include <string.h>
#include <math.h>
uint8_t Nvm_ParamFlashBlock[292];
#define PAYLOAD_SIZE (sizeof(Dem_DTC_Stat)+sizeof(Dem_FF)+sizeof(SMon_Battery))
_Static_assert(PAYLOAD_SIZE <= NVM_STORE_MAX, "NvM snapshot exceeds one page");
static uint32_t *snapshot;
static const uint32_t defaultStatus[DEM_MAX_FF_DTC] = {
0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50};
static const Dem_FreezeFrame_t defaultFF[DEM_MAX_FF_DTC];
static const SMon_BatteryData_t defaultBattery;
static void *const blocks[4]={0,Dem_DTC_Stat,Dem_FF,&SMon_Battery};
static const void *const defaults[4]={0,defaultStatus,defaultFF,&defaultBattery};
static const uint16_t sizes[4]={0,sizeof(Dem_DTC_Stat),sizeof(Dem_FF),sizeof(SMon_Battery)};
static void copyBlocks(uint8_t load)
{
    uint8_t *p=(uint8_t*)snapshot;
    for(unsigned i=1;i<4;++i) {if(load) memcpy(blocks[i],p,sizes[i]);else memcpy(p,blocks[i],sizes[i]);p+=sizes[i];}
}
int MemIf_Read(uint32_t off,void *dst,uint16_t size)
{
    if(!dst || off>2048u || size>2048u-off) return -1;
    memcpy(dst,(const void*)(NVM_DTC_START_ADDRESS+off),size);return 0;
}
int MemIf_Erase(uint8_t page)
{
    FLASH_EraseInitTypeDef e={0};uint32_t err,mask=__get_PRIMASK();int rc=-1;
    if(page>1u)return -1;
    __disable_irq();
    if(HAL_FLASH_Unlock()==HAL_OK) {
        e.TypeErase=FLASH_TYPEERASE_PAGES;e.PageAddress=NVM_DTC_START_ADDRESS+page*1024u;e.NbPages=1;
        if(HAL_FLASHEx_Erase(&e,&err)==HAL_OK) {
            rc=0;for(unsigned i=0;i<256;++i)if(*(volatile uint32_t*)(e.PageAddress+4u*i)!=0xFFFFFFFFu)rc=-1;
        }
    }
    HAL_FLASH_Lock();__set_PRIMASK(mask);return rc;
}
int MemIf_Program(uint32_t off,uint32_t word)
{
    uint32_t mask;int rc=-1;
    if((off&3u)||off>2044u)return -1;
    mask=__get_PRIMASK();__disable_irq();
    if(HAL_FLASH_Unlock()==HAL_OK && HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,NVM_DTC_START_ADDRESS+off,word)==HAL_OK &&
       *(volatile uint32_t*)(NVM_DTC_START_ADDRESS+off)==word)rc=0;
    HAL_FLASH_Lock();__set_PRIMASK(mask);return rc;
}
void Nvm_ReadAll(void)
{
    uint32_t protectedPages=0,pattern[2];
    NvmStore_Init(PAYLOAD_SIZE);snapshot=NvmStore_GetBuffer();
    if(!NvmStore_Read(snapshot)){copyBlocks(1);return;}
    for(unsigned i=1;i<4;++i)memcpy(blocks[i],defaults[i],sizes[i]);
    /* Read legacy records without erasing; choose only a page holding no latest valid record. */
    MemIf_Read(0,pattern,8);
    uint32_t latest[4]={0};
    if(pattern[0]==0xB6B6B6B6u && pattern[1]==pattern[0]) {
        protectedPages|=1u; /* Legacy recovery requires its page-zero start marker. */
        for(uint32_t off=8;off+16u<=2048u;) {
            Nvm_Header_t h;uint32_t crc;
            MemIf_Read(off,&h,8);
            if(h.blockId==0xFFu)break;
            if(h.blockId==0 || h.blockId>=4 || h.blockSize!=sizes[h.blockId] || off+16u+h.blockSize>2048u)break;
            MemIf_Read(off+8,snapshot,h.blockSize);MemIf_Read(off+8+h.blockSize,&crc,4);
            /* Legacy STM32 CRC consumes little-endian words, no final xor. */
            uint32_t calc=0xFFFFFFFFu;
            for(unsigned w=0;w<h.blockSize/4u;++w){calc^=snapshot[w];for(unsigned b=0;b<32;++b)calc=(calc<<1)^((calc&0x80000000u)?0x04C11DB7u:0);}
            if(calc==crc){memcpy(blocks[h.blockId],snapshot,h.blockSize);latest[h.blockId]=off;}
            off+=16u+h.blockSize;
        }
        for(unsigned i=1;i<4;++i)if(latest[i]) {
            protectedPages|=1u<<(latest[i]/1024u);
            protectedPages|=1u<<((latest[i]+15u+sizes[i])/1024u);
        }
    } else {
        /* Unknown non-erased contents are retained for recovery. */
        for(unsigned page=0;page<2;++page)for(unsigned off=0;off<1024u;off+=4u){uint32_t v;MemIf_Read(page*1024u+off,&v,4);if(v!=0xFFFFFFFFu)protectedPages|=1u<<page;}
    }
    if(!(protectedPages&1u))NvmStore_AllowMigration(0);
    else if(!(protectedPages&2u))NvmStore_AllowMigration(1);
}
int Nvm_WriteBlock(uint16_t id,const void *data)
{
    if(!snapshot || id==0 || id>=4 || !data || NvmStore_GetResult()==NVM_PENDING)return -1;
    uint32_t mask=__get_PRIMASK();__disable_irq();
    if(data!=blocks[id])memcpy(blocks[id],data,sizes[id]);
    copyBlocks(0);int rc=NvmStore_Write(snapshot);__set_PRIMASK(mask);return rc;
}
int Nvm_WriteAll(void){return Nvm_WriteBlock(1,blocks[1]);}
int Nvm_ReadBlock(uint16_t id,void *dst)
{
    if(!snapshot||id==0||id>=4||!dst||NvmStore_GetResult()==NVM_PENDING||NvmStore_Read(snapshot))return -1;
    uint16_t off=0;for(unsigned i=1;i<id;++i)off+=sizes[i];memcpy(dst,(uint8_t*)snapshot+off,sizes[id]);return 0;
}
uint8_t Nvm_IsWriteAllowed(void){return SMon_L1ST==0u && SMon_CalculatedCommand==0u && SMon_CLSFlag!=1u;}
void Nvm_MainFunction(void){if(Nvm_IsWriteAllowed())NvmStore_MainFunction();}
Nvm_Result Nvm_GetErrorStatus(uint16_t id){return (id<4u)?NvmStore_GetResult():NVM_FAILED;}
void Nvm_FlashReadData(uint32_t addr,uint32_t *dst,uint16_t words)
{
    if(dst && ((addr>=NVM_DTC_START_ADDRESS && addr<=0x08010000u && 4u*words<=0x08010000u-addr)||
       (addr>=NVM_PARAMFLASH_START_ADDRESS && addr<=0x08003C00u && 4u*words<=0x08003C00u-addr)))memcpy(dst,(void*)addr,4u*words);
}
static uint32_t rd_u32(const uint8_t *p)
{
	return (uint32_t)p[0] |
			((uint32_t)p[1] << 8) |
			((uint32_t)p[2] << 16) |
			((uint32_t)p[3] << 24);
}

static float rd_f32(const uint8_t *p)
{
	uint32_t tmp = rd_u32(p);
	float f;
	memcpy(&f, &tmp, 4);
	return f;
}

uint8_t Nvm_ParameterIntegrity;
static int Nvm_ParamFormatValid(void)
{
    Nvm_ParameterIntegrity=0;
    if(Nvm_ParamFlashBlock[291]!=0xAAu)return 0;
    static const uint8_t floatSlots[]={24,25,26,27,28,29,30,31,32,33,34,36,37,38,39,40,41,42,43,45,46,48,49,50,51,52,53,54,55,56,57,58,59,60,62,64,65,66,67,71};
    for(unsigned i=0;i<sizeof floatSlots;++i)
        if((rd_u32(Nvm_ParamFlashBlock+4u*floatSlots[i])&0x7F800000u)==0x7F800000u)return 0;
    if(!rd_u32(&Nvm_ParamFlashBlock[48]))return 0;
    if(!rd_u32(&Nvm_ParamFlashBlock[44]))return 0;
    if(!rd_u32(&Nvm_ParamFlashBlock[88]))return 0;
    if(!rd_u32(&Nvm_ParamFlashBlock[56]))return 0;
    if(rd_u32(Nvm_ParamFlashBlock+4)==0x324C4143u){
        if(rd_u32(Nvm_ParamFlashBlock)!=Crc_CalculateCRC32(Nvm_ParamFlashBlock+8,284u))return 0;
        Nvm_ParameterIntegrity=2;
    }else Nvm_ParameterIntegrity=1; /* compatible legacy image has no integrity field */
    return 1;
}
void Nvm_InitParamFlash(void)
{
	Nvm_FlashReadData(NVM_PARAMFLASH_START_ADDRESS, (uint32_t*)Nvm_ParamFlashBlock, sizeof(Nvm_ParamFlashBlock) / 4u);

	if(Nvm_ParamFormatValid())
	{
		uint32_t idx = 0u;

		idx += 4; /* legacy slot kept to preserve param flash layout */
		idx += 4; /* unused legacy slot */
		SMon_P_DischargeTimeCycles        = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_NTC_PullUp_ResistorVale    = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_LongDischargeTimeCycles    = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_LowDisTimeCyc              = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_ClsFailureWaitTime         = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_LowVoltage                 = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_UV_KL30                    = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_OV_KL30                    = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_UV_CLS                     = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_Varef                      = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_ADC_MaxValue               = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		idx += 4; /* unused legacy slot */
		SMon_P_NTC_L1_TwoPointCalibration_ParamB = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_StatusVoltageL1Filter       = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_I2TDebounceTime             = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_Rtcntmax                    = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_CLSTime                     = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_WaitTimeOVUV                = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		idx += 4; /* unused legacy slot */
		SMon_P_I2TDecrementPercentFactor   = rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_NTC_L1_TwoPointCalibration_ParamA 	= rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattRestTimeTicks        			= rd_u32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_VFB_T30_TwoPointCalibration_ParamA = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_VFB_T30_TwoPointCalibration_ParamB = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_VFB_L1_TwoPointCalibration_ParamA  = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_VFB_L1_TwoPointCalibration_ParamB  = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_ISenseNominal                      = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_I2TRating                          = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_RoomTempKelvin                     = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_VoltsAt25                          = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_AvgSlope                           = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_RoomTemperature                    = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_Kelvin                             = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		idx += 4; /* unused legacy slot */
		SMon_P_AlphaFilter                        = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_AlphaFilterExtChIsense             = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_TwoPointCalib_ConvFacISense        = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_TwoPointCalib_NoLoad_ISense        = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_ExternalChargerThreshold           = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_NTCTemperatureMax                  = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_NTCTemperatureRelease              = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattNominalCapacity_Ah      = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		idx += 4; /* unused legacy slot */
		SMon_P_BattMinSoC_pct              = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattMaxSoC_pct              = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		idx += 4; /* unused legacy slot */
		SMon_P_BattRestVoltDelta_V         = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattWeakVolt_V              = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattDeepDischargeVolt_V     = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattCrankVolt_V             = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattAlphaVolt               = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattAlphaCurr               = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattAlphaRint               = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattChargeEfficiency        = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattOcvCorrectionGain       = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattSoHMin_pct              = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattSoHMax_pct              = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattNominalRint_Ohm         = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattBadRint_Ohm             = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		idx += 4; /* unused legacy slot */
		SMon_P_BattCurrentDeadband_A = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		idx += 4; /* unused legacy slot */
		SMon_P_BattCurrentHys_A             = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattChargePathDeltaOn_V      = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattChargePathDeltaOff_V     = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		SMon_P_BattRintMinStep_A            = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
		idx += 4; /* unused legacy slot */
		idx += 4; /* unused legacy slot */
		idx += 4; /* unused legacy slot */
		SMon_P_BattRestDetectCurrent_A = rd_f32(&Nvm_ParamFlashBlock[idx]); idx += 4;
	}
	else
	{
		SMon_P_StatusVoltageL1Filter = 250u;
		SMon_P_I2TDebounceTime = 20u;
		SMon_P_Rtcntmax = 13u; // Retry Counter Parameter
		SMon_P_CLSTime = 22u; // CLS Duration Parameter
		SMon_P_WaitTimeOVUV = 80u; // OV UV De-bounce Time Parameter
		SMon_P_I2TDecrementPercentFactor = 85u; // Cooling Off Factor Parameter
		SMon_P_ClsFailureWaitTime = 190u; // Wait Time Between CLS Retries Parameter
		SMon_P_DischargeTimeCycles = 132000u; // ~50% Starting Voltage Discharge Time Parameter
		SMon_P_LowVoltage = 1200u; // 5TAU / Maximum Discharge Voltage Threshold Parameter
		SMon_P_UV_KL30 = 6900u; // UV KL30 TH Parameter
		SMon_P_OV_KL30 = 17200u; // OV KL30 TH Parameter
		SMon_P_UV_CLS = 700u;
		SMon_P_Varef = 3300u;
		SMon_P_ADC_MaxValue = 4095u;
		SMon_P_NTC_PullUp_ResistorVale = 91000u;
		SMon_P_LongDischargeTimeCycles = 564000u; // Maximum Discharge Time Parameter
		SMon_P_LowDisTimeCyc = 229600; // 2TAU Discharge Time Parameter
		SMon_P_VFB_T30_TwoPointCalibration_ParamA = 8.16f;
		SMon_P_VFB_T30_TwoPointCalibration_ParamB = 21.93f;
		SMon_P_VFB_L1_TwoPointCalibration_ParamA = 8.13f;
		SMon_P_VFB_L1_TwoPointCalibration_ParamB = 8.13f;
		SMon_P_NTC_L1_TwoPointCalibration_ParamA = 12332u; // R0_cal in ohms
		SMon_P_BattRestTimeTicks        = 200u;   /* tune to scheduler tick */
		SMon_P_NTC_L1_TwoPointCalibration_ParamB = 4984u;  // Beta_cal in K
		SMon_P_ISenseNominal = 28.000f; // Nominal Current Parameter
		SMon_P_I2TRating = 1176.0f; // I2T Rating Parameter
		SMon_P_RoomTempKelvin = 297.15f;
		SMon_P_VoltsAt25 = 1430.0f;
		SMon_P_AvgSlope = 4.30f;
		SMon_P_RoomTemperature = 25.0f;
		SMon_P_Kelvin = 273.15f;
		SMon_P_AlphaFilter = 0.90f;
		SMon_P_AlphaFilterExtChIsense = 0.0001f;
		SMon_P_TwoPointCalib_ConvFacISense = 15.91f;
		SMon_P_TwoPointCalib_NoLoad_ISense = 613.2f;
		SMon_P_ExternalChargerThreshold = -1.0f;
		SMon_P_NTCTemperatureMax = 90.0f;
		SMon_P_NTCTemperatureRelease = 80.0f;
		SMon_P_BattNominalCapacity_Ah      = 77.0f;
		SMon_P_BattMinSoC_pct              = 1.0f;
		SMon_P_BattMaxSoC_pct              = 100.0f;
		SMon_P_BattRestVoltDelta_V         = 0.02f;
		SMon_P_BattWeakVolt_V              = 11.80f;
		SMon_P_BattDeepDischargeVolt_V     = 11.10f;
		SMon_P_BattCrankVolt_V             = 9.60f;
		SMon_P_BattAlphaVolt               = 0.02f;
		SMon_P_BattAlphaCurr               = 0.05f;
		SMon_P_BattAlphaRint               = 0.10f;
		SMon_P_BattChargeEfficiency        = 0.90f;
		SMon_P_BattOcvCorrectionGain       = 0.20f;
		SMon_P_BattSoHMin_pct              = 1.0f;
		SMon_P_BattSoHMax_pct              = 100.0f;
		SMon_P_BattNominalRint_Ohm         = 0.015f;
		SMon_P_BattBadRint_Ohm             = 0.060f;
		SMon_P_BattCurrentDeadband_A = 1.0f;
		SMon_P_BattCurrentHys_A            = 0.30f;
		SMon_P_BattChargePathDeltaOn_V     = 0.25f;
		SMon_P_BattChargePathDeltaOff_V    = 0.10f;
		SMon_P_BattRintMinStep_A           = 0.20f;
		SMon_P_BattRestDetectCurrent_A = 0.30f;

		uint32_t idx = 0u;

#define COPY_TO_BUF(val)                     \
		do \
		{                                        \
			memcpy(&Nvm_ParamFlashBlock[idx],        \
					&(val),                          \
					sizeof(val));                    \
					idx += sizeof(val);                     \
		} while(0)

		idx += 4; /* legacy slot kept to preserve param flash layout */
		idx += 4; /* unused legacy slot */
		COPY_TO_BUF(SMon_P_DischargeTimeCycles);
		COPY_TO_BUF(SMon_P_NTC_PullUp_ResistorVale);
		COPY_TO_BUF(SMon_P_LongDischargeTimeCycles);
		COPY_TO_BUF(SMon_P_LowDisTimeCyc);
		COPY_TO_BUF(SMon_P_ClsFailureWaitTime);
		COPY_TO_BUF(SMon_P_LowVoltage);
		COPY_TO_BUF(SMon_P_UV_KL30);
		COPY_TO_BUF(SMon_P_OV_KL30);
		COPY_TO_BUF(SMon_P_UV_CLS);
		COPY_TO_BUF(SMon_P_Varef);
		COPY_TO_BUF(SMon_P_ADC_MaxValue);
		idx += 4; /* unused legacy slot */
		COPY_TO_BUF(SMon_P_NTC_L1_TwoPointCalibration_ParamB);
		COPY_TO_BUF(SMon_P_StatusVoltageL1Filter);
		COPY_TO_BUF(SMon_P_I2TDebounceTime);
		COPY_TO_BUF(SMon_P_Rtcntmax);
		COPY_TO_BUF(SMon_P_CLSTime);
		COPY_TO_BUF(SMon_P_WaitTimeOVUV);
		idx += 4; /* unused legacy slot */
		COPY_TO_BUF(SMon_P_I2TDecrementPercentFactor);
		COPY_TO_BUF(SMon_P_NTC_L1_TwoPointCalibration_ParamA);
		COPY_TO_BUF(SMon_P_BattRestTimeTicks);
		COPY_TO_BUF(SMon_P_VFB_T30_TwoPointCalibration_ParamA);
		COPY_TO_BUF(SMon_P_VFB_T30_TwoPointCalibration_ParamB);
		COPY_TO_BUF(SMon_P_VFB_L1_TwoPointCalibration_ParamA);
		COPY_TO_BUF(SMon_P_VFB_L1_TwoPointCalibration_ParamB);
		COPY_TO_BUF(SMon_P_ISenseNominal);
		COPY_TO_BUF(SMon_P_I2TRating);
		COPY_TO_BUF(SMon_P_RoomTempKelvin);
		COPY_TO_BUF(SMon_P_VoltsAt25);
		COPY_TO_BUF(SMon_P_AvgSlope);
		COPY_TO_BUF(SMon_P_RoomTemperature);
		COPY_TO_BUF(SMon_P_Kelvin);
		idx += 4; /* unused legacy slot */
		COPY_TO_BUF(SMon_P_AlphaFilter);
		COPY_TO_BUF(SMon_P_AlphaFilterExtChIsense);
		COPY_TO_BUF(SMon_P_TwoPointCalib_ConvFacISense);
		COPY_TO_BUF(SMon_P_TwoPointCalib_NoLoad_ISense);
		COPY_TO_BUF(SMon_P_ExternalChargerThreshold);
		COPY_TO_BUF(SMon_P_NTCTemperatureMax);
		COPY_TO_BUF(SMon_P_NTCTemperatureRelease);
		COPY_TO_BUF(SMon_P_BattNominalCapacity_Ah);
		idx += 4; /* unused legacy slot */
		COPY_TO_BUF(SMon_P_BattMinSoC_pct);
		COPY_TO_BUF(SMon_P_BattMaxSoC_pct);
		idx += 4; /* unused legacy slot */
		COPY_TO_BUF(SMon_P_BattRestVoltDelta_V);
		COPY_TO_BUF(SMon_P_BattWeakVolt_V);
		COPY_TO_BUF(SMon_P_BattDeepDischargeVolt_V);
		COPY_TO_BUF(SMon_P_BattCrankVolt_V);
		COPY_TO_BUF(SMon_P_BattAlphaVolt);
		COPY_TO_BUF(SMon_P_BattAlphaCurr);
		COPY_TO_BUF(SMon_P_BattAlphaRint);
		COPY_TO_BUF(SMon_P_BattChargeEfficiency);
		COPY_TO_BUF(SMon_P_BattOcvCorrectionGain);
		COPY_TO_BUF(SMon_P_BattSoHMin_pct);
		COPY_TO_BUF(SMon_P_BattSoHMax_pct);
		COPY_TO_BUF(SMon_P_BattNominalRint_Ohm);
		COPY_TO_BUF(SMon_P_BattBadRint_Ohm);
		idx += 4; /* unused legacy slot */
		COPY_TO_BUF(SMon_P_BattCurrentDeadband_A);
		idx += 4; /* unused legacy slot */
		COPY_TO_BUF(SMon_P_BattCurrentHys_A);
		COPY_TO_BUF(SMon_P_BattChargePathDeltaOn_V);
		COPY_TO_BUF(SMon_P_BattChargePathDeltaOff_V);
		COPY_TO_BUF(SMon_P_BattRintMinStep_A);
		idx += 4; /* unused legacy slot */
		idx += 4; /* unused legacy slot */
		idx += 4; /* unused legacy slot */
		COPY_TO_BUF(SMon_P_BattRestDetectCurrent_A);

		Nvm_ParamFlashBlock[291u] = 0xAAu;
		Nvm_ParamFlashBlock[290u] = 0x21u;
		/* Defaults remain RAM-only: never erase calibration on startup. */

#undef COPY_TO_BUF
	}
}
