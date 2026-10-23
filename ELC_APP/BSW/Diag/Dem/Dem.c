#include "Dem.h"
#include "Nvm.h"
#include "../../Com/Com/Com.h"
#include "main.h"
#include <string.h>
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
};

uint32_t Dem_DTC_Stat[DEM_MAX_FF_DTC];
static uint8_t enabled,dirty,writing,clearing;
static uint32_t revision,submitted;
static int clearResult;
static uint32_t broadcast,deferredFailed,deferredPassed;
static uint8_t coalesce;
void Dem_Init(void){enabled=1;dirty=writing=clearing=0;revision=submitted=0;clearResult=0;broadcast=deferredFailed=deferredPassed=0;coalesce=0;}
static int indexOf(uint32_t dtc){for(unsigned i=0;i<DEM_MAX_FF_DTC;++i)if(Dem_PreDefined_DTC_Table[i]==dtc)return (int)i;return -1;}
void Dem_SetDTCSetting(uint8_t value){enabled=value?1:0;}
void Dem_CaptureFreezeFrame(uint8_t i)
{
    if(i>=DEM_MAX_FF_DTC)return;
    uint16_t count=Dem_FF[i].occurrenceCnt;
    Dem_AppCapture(&Dem_FF[i]);Dem_FF[i].occurrenceCnt=(count==UINT16_MAX)?count:count+1u;
}
void Dem_SetDtc(uint32_t dtc,uint8_t report)
{
    int i=indexOf(dtc);if(i<0||!enabled)return;
    uint32_t mask=__get_PRIMASK();__disable_irq();
    if(clearing){if(report==0x2Fu){deferredFailed|=1u<<i;deferredPassed&=~(1u<<i);}else if(report==0x2Eu){deferredPassed|=1u<<i;deferredFailed&=~(1u<<i);}__set_PRIMASK(mask);return;}
    uint8_t old=(uint8_t)Dem_DTC_Stat[i],status=old;
    /* Monitor owns debounce. Legacy 0x2F/0x2E are FAILED/PASSED reports. */
    if(report==0x2Fu) {
        status=(uint8_t)((old & ~0x50u)|0x2Fu); /* confirmation threshold = one qualified failure */
        if(!(old&1u)){Dem_CaptureFreezeFrame((uint8_t)i);broadcast|=1u<<i;}
    } else if(report==0x2Eu) status=(uint8_t)(old&~0x51u);
    else {__set_PRIMASK(mask);return;}
    if(status!=old){Dem_DTC_Stat[i]=status;if(!dirty)coalesce=10;dirty=1;revision++;}
    __set_PRIMASK(mask);
}
uint8_t Dem_GetDtcStatus(uint32_t dtc)
{
    int i=indexOf(dtc);if(i<0)return 0;
    /* Preserve legacy monitor comparisons while UDS uses the actual status. */
    if(Dem_DTC_Stat[i]&1u)return 0x2Fu;
    if(Dem_DTC_Stat[i]&0x20u)return 0x2Eu;
    return (uint8_t)Dem_DTC_Stat[i];
}
int Dem_GetEvent(uint8_t i,uint32_t *dtc,uint8_t *status)
{
    if(i>=DEM_MAX_FF_DTC||!dtc||!status)return -1;
    *dtc=Dem_PreDefined_DTC_Table[i];*status=(uint8_t)Dem_DTC_Stat[i];return 0;
}
int Dem_GetSnapshot(uint32_t dtc,Dem_FreezeFrame_t *ff)
{
    int i=indexOf(dtc);if(i<0||!ff||!Dem_FF[i].occurrenceCnt)return -1;
    uint32_t mask=__get_PRIMASK();__disable_irq();memcpy(ff,&Dem_FF[i],sizeof *ff);__set_PRIMASK(mask);return 0;
}
void Dem_SetOperationCycle(uint8_t start)
{
    if(clearing)return;
    for(unsigned i=0;i<DEM_MAX_FF_DTC;++i){uint8_t old=(uint8_t)Dem_DTC_Stat[i],s=old;
        if(start)s=(uint8_t)((s&~2u)|0x40u);
        else if(!(s&0x42u))s&=(uint8_t)~4u;
        if(s!=old){Dem_DTC_Stat[i]=s;dirty=1;revision++;}
    }
    /* No calibrated aging threshold exists: confirmed history remains until clear. */
}
int Dem_ClearDTC(uint32_t group)
{
    if(group!=0xFFFFFFu && indexOf(group)<0)return -1;
    if(clearing||Nvm_GetErrorStatus(0)==NVM_PENDING)return -2;
    uint32_t mask=__get_PRIMASK();__disable_irq();
    for(unsigned i=0;i<DEM_MAX_FF_DTC;++i)if(group==0xFFFFFFu||group==Dem_PreDefined_DTC_Table[i]){
        Dem_DTC_Stat[i]=0x50u;memset(&Dem_FF[i],0,sizeof Dem_FF[i]);broadcast&=~(1u<<i);
    }
    clearing=1;clearResult=1;dirty=1;revision++;__set_PRIMASK(mask);return 0;
}
int Dem_GetClearResult(void){return clearResult;}
void Dem_main(void)
{
    uint32_t mask=__get_PRIMASK();__disable_irq();
    if(writing && Nvm_GetErrorStatus(0)!=NVM_PENDING){
        Nvm_Result r=Nvm_GetErrorStatus(0);writing=0;
        if(r==NVM_OK){if(revision==submitted)dirty=0;if(clearing){clearResult=0;clearing=0;}}
        else {if(clearing){clearResult=-1;clearing=0;}dirty=0;} /* bounded: explicit retry only */
    }
    if(coalesce && !clearing)coalesce--;
    if(dirty && !writing && (!coalesce || clearing) && Nvm_GetErrorStatus(0)!=NVM_PENDING){
        if(!Nvm_WriteAll()){submitted=revision;writing=1;}
        else if(clearing){clearResult=-1;clearing=0;dirty=0;}
    }
    if(!clearing && (deferredFailed||deferredPassed)){
        uint32_t failed=deferredFailed,passed=deferredPassed;deferredFailed=deferredPassed=0;
        for(unsigned i=0;i<DEM_MAX_FF_DTC;++i)if((failed|passed)&(1u<<i))Dem_SetDtc(Dem_PreDefined_DTC_Table[i],(failed&(1u<<i))?0x2F:0x2E);
    }
    for(unsigned i=0;i<DEM_MAX_FF_DTC;++i)if(broadcast&(1u<<i)){
        uint32_t dtc=Dem_PreDefined_DTC_Table[i];uint8_t st=(uint8_t)Dem_DTC_Stat[i];
        uint8_t data[4]={(uint8_t)dtc,(uint8_t)(dtc>>8),(uint8_t)(((dtc>>16)&0x3Fu)|((dtc>>24)&3u)<<6),
          (uint8_t)(((dtc>>26)&3u)|((st&0x3Fu)<<2))};
        if(!Com_QueueFault(data))broadcast&=~(1u<<i);
        break;
    }
    __set_PRIMASK(mask);
}
