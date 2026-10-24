/* Dem_Cfg.h - event ids. Event id == DTC - 0x50 (DTC numbers 0x50..0x61, unchanged ELC values). */
#ifndef ELC_BSW_DEM_CFG_H
#define ELC_BSW_DEM_CFG_H

#include "Dem_Types.h"

#define DEM_NUMBER_OF_EVENTS             18u

#define DEM_EVENT_UV_KL30                0u   /* DTC 0x50 */
#define DEM_EVENT_OV_KL30                1u   /* 0x51 */
#define DEM_EVENT_SHORT_TO_BATT_L1_P1    2u   /* 0x52 */
#define DEM_EVENT_OVERCURRENT_L1         3u   /* 0x53 */
#define DEM_EVENT_L1_LOCKED              4u   /* 0x54 */
#define DEM_EVENT_CLS_FAILURE_L1         5u   /* 0x55 */
#define DEM_EVENT_UV_L1                  6u   /* 0x56 */
#define DEM_EVENT_MCU_RESET              7u   /* 0x57 */
#define DEM_EVENT_NTC_ERROR              8u   /* 0x58 */
#define DEM_EVENT_EXTERNAL_CHARGER       9u   /* 0x59 */
#define DEM_EVENT_LOADREQUEST_MISSING    10u  /* 0x5A */
#define DEM_EVENT_LOADREQUEST_E2E        11u  /* 0x5B */
#define DEM_EVENT_LOADREQUEST_INVALID    12u  /* 0x5C */
#define DEM_EVENT_VEHICLEDATA_TIMEOUT    13u  /* 0x5D */
#define DEM_EVENT_VEHICLESTATUS_INVALID  14u  /* 0x5E */
#define DEM_EVENT_SHORT_TO_BATT_L1_P2    15u  /* 0x5F */
#define DEM_EVENT_SHORT_TO_BATT_L1_P3    16u  /* 0x60 */
#define DEM_EVENT_XCP_USED               17u  /* 0x61 */

#define DEM_OPERATION_CYCLE_POWER        0u

/* Data identifiers of the freeze frame record (UDS 0x19 0x04). */
#define DEM_DID_FF_OCCURRENCE_COUNT      0xF300u
#define DEM_NUMBER_OF_FF_DIDS            12u
/* numberOfIdentifiers byte + 12 x DID(2) + data (2+7x1+2+2+4+4 = 21) */
#define DEM_SNAPSHOT_MAX_LEN             46u

/* Extended data record of UDS 0x19 0x06: record 0x01 = occurrence counter (record number + 2 data bytes). */
#define DEM_EXTENDED_DATA_OCCURRENCE     0x01u
#define DEM_EXTENDED_DATA_LEN            3u

extern const Dem_ConfigType Dem_Config;

/* Application capture callback: fills the monitor-specific freeze frame fields. */
extern void Dem_AppCapture(Dem_FreezeFrame_t *snapshot);

/* NvM RAM mirror blocks (NvM block 1 and 2, see NvM_Cfg.c). */
extern Dem_FreezeFrame_t Dem_FF[DEM_NUMBER_OF_EVENTS];
extern uint32 Dem_DTC_Stat[DEM_NUMBER_OF_EVENTS];

#endif /* ELC_BSW_DEM_CFG_H */
