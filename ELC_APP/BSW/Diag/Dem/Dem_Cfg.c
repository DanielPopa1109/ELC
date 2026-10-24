#include "Dem.h"
#include <stddef.h>

/* NvM RAM mirrors. Event status words use the low byte as UDS status; the upper bytes are
 * reserved (stored for image compatibility). */
Dem_FreezeFrame_t Dem_FF[DEM_NUMBER_OF_EVENTS];
uint32 Dem_DTC_Stat[DEM_NUMBER_OF_EVENTS];

static const Dem_EventConfigType Dem_Events[DEM_NUMBER_OF_EVENTS] =
{
    { 0x50u }, /* UV KL30 */
    { 0x51u }, /* OV KL30 */
    { 0x52u }, /* SHORT TO BATT L1 P1 */
    { 0x53u }, /* OVERCURRENT L1 */
    { 0x54u }, /* L1 LOCKED */
    { 0x55u }, /* CLS FAILURE L1 */
    { 0x56u }, /* UV L1 */
    { 0x57u }, /* MCU RESET */
    { 0x58u }, /* NTC ERROR */
    { 0x59u }, /* EXTERNAL CHARGER DETECTED */
    { 0x5Au }, /* LOAD REQUEST MSG MISSING */
    { 0x5Bu }, /* LOAD REQUEST E2E ERROR */
    { 0x5Cu }, /* SIGNAL INVALID LOAD REQUEST */
    { 0x5Du }, /* VEHICLE DATA TIMEOUT */
    { 0x5Eu }, /* VEHICLE STATUS INVALID */
    { 0x5Fu }, /* SHORT TO BATT L1 P2 */
    { 0x60u }, /* SHORT TO BATT L1 P3 */
    { 0x61u }  /* XCP USED */
};

static const Dem_FreezeFrameDidType Dem_FreezeFrameDids[DEM_NUMBER_OF_FF_DIDS] =
{
    { 0xF300u, (uint8)offsetof(Dem_FreezeFrame_t, occurrenceCnt), 2u },
    { 0xF301u, (uint8)offsetof(Dem_FreezeFrame_t, year),          1u },
    { 0xF302u, (uint8)offsetof(Dem_FreezeFrame_t, month),         1u },
    { 0xF303u, (uint8)offsetof(Dem_FreezeFrame_t, day),           1u },
    { 0xF304u, (uint8)offsetof(Dem_FreezeFrame_t, hour),          1u },
    { 0xF305u, (uint8)offsetof(Dem_FreezeFrame_t, minute),        1u },
    { 0xF306u, (uint8)offsetof(Dem_FreezeFrame_t, second),        1u },
    { 0xF307u, (uint8)offsetof(Dem_FreezeFrame_t, vehStatus),     1u },
    { 0xF308u, (uint8)offsetof(Dem_FreezeFrame_t, L1VFB),         2u },
    { 0xF309u, (uint8)offsetof(Dem_FreezeFrame_t, T30VFB),        2u },
    { 0xF30Au, (uint8)offsetof(Dem_FreezeFrame_t, L1NTC),         4u },   /* raw little-endian float */
    { 0xF30Bu, (uint8)offsetof(Dem_FreezeFrame_t, L1ISENSE),      4u }    /* raw little-endian float */
};

const Dem_ConfigType Dem_Config =
{
    Dem_Events, DEM_NUMBER_OF_EVENTS,
    Dem_FreezeFrameDids, DEM_NUMBER_OF_FF_DIDS,
    Dem_AppCapture
};
