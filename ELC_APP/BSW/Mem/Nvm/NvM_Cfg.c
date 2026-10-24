#include "NvM.h"
#include "Dem_Cfg.h"
#include "SMon.h"

static void NvM_InitDemStatus(void)
{
    uint8 i;

    for (i = 0u; i < DEM_NUMBER_OF_EVENTS; ++i)
    {
        Dem_DTC_Stat[i] = DEM_UDS_STATUS_CLEARED;
    }
}

const NvM_BlockDescriptorType NvM_BlockDescriptor[NVM_NUMBER_OF_BLOCKS] =
{
    { (uint16)sizeof(Dem_DTC_Stat),  Dem_DTC_Stat,  NvM_InitDemStatus },
    { (uint16)sizeof(Dem_FF),        Dem_FF,        NULL_PTR },
    { (uint16)sizeof(SMon_Battery),  &SMon_Battery, NULL_PTR }
};

uint16 NvM_Cfg_GetSnapshotSize(void)
{
    return (uint16)(sizeof(Dem_DTC_Stat) + sizeof(Dem_FF) + sizeof(SMon_Battery));
}

/* Erasing a Flash page stalls the CPU: only while the load output is off and not commanded. */
uint8 NvM_Cfg_IsWriteAllowed(void)
{
    return (uint8)((SMon_L1ST == 0u) && (SMon_CalculatedCommand == 0u) && (SMon_CLSFlag != 1u));
}
