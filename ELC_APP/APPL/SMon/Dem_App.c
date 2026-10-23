#include "Dem.h"
#include "SMon.h"
#include "CanH.h"
void Dem_AppCapture(Dem_FreezeFrame_t *ff)
{
    ff->year=CSBSDAT_Year;ff->month=CSBSDAT_Month;ff->day=CSBSDAT_Day;
    ff->hour=CSBSDAT_Hour;ff->minute=CSBSDAT_Minute;ff->second=CSBSDAT_Second;
    ff->vehStatus=CanH_VehicleStatus;ff->L1VFB=SMon_VfbL1;ff->T30VFB=SMon_VfbT30;
    ff->L1NTC=SMon_NTC_Temperature_L1;ff->L1ISENSE=SMon_ISenseL1_Float;
}
