/* Dem_Types.h - Diagnostic Event Manager types (UDS DTC status per ISO 14229-1). */
#ifndef ELC_BSW_DEM_TYPES_H
#define ELC_BSW_DEM_TYPES_H

#include "Std_Types.h"

typedef uint16 Dem_EventIdType;
typedef uint32 Dem_DTCType;
typedef uint8  Dem_UdsStatusByteType;
typedef uint8  Dem_OperationCycleIdType;

typedef enum
{
    DEM_EVENT_STATUS_PASSED = 0u,
    DEM_EVENT_STATUS_FAILED,
    DEM_EVENT_STATUS_PREPASSED,   /* counter based debouncing is not configured (monitors debounce) */
    DEM_EVENT_STATUS_PREFAILED
} Dem_EventStatusType;

typedef enum
{
    DEM_CYCLE_STATE_START = 0u,
    DEM_CYCLE_STATE_END
} Dem_OperationCycleStateType;

typedef enum
{
    DEM_DTC_FORMAT_OBD = 0u,
    DEM_DTC_FORMAT_UDS,
    DEM_DTC_FORMAT_J1939
} Dem_DTCFormatType;

typedef uint16 Dem_DTCOriginType;
#define DEM_DTC_ORIGIN_PRIMARY_MEMORY 0x0001u

typedef enum
{
    DEM_CLEAR_OK = 0u,
    DEM_CLEAR_WRONG_DTC,
    DEM_CLEAR_WRONG_DTCORIGIN,
    DEM_CLEAR_FAILED,
    DEM_CLEAR_PENDING,        /* call Dem_ClearDTC() again until the result changes */
    DEM_CLEAR_BUSY,           /* clearing is not possible now (NvM busy / Flash work not allowed) */
    DEM_CLEAR_MEMORY_ERROR    /* the cleared state could not be stored durably */
} Dem_ReturnClearDTCType;

/* UDS status byte (ISO 14229-1 Annex D). */
#define DEM_UDS_STATUS_TF       0x01u   /* testFailed */
#define DEM_UDS_STATUS_TFTOC    0x02u   /* testFailedThisOperationCycle */
#define DEM_UDS_STATUS_PDTC     0x04u   /* pendingDTC */
#define DEM_UDS_STATUS_CDTC     0x08u   /* confirmedDTC */
#define DEM_UDS_STATUS_TNCSLC   0x10u   /* testNotCompletedSinceLastClear */
#define DEM_UDS_STATUS_TFSLC    0x20u   /* testFailedSinceLastClear */
#define DEM_UDS_STATUS_TNCTOC   0x40u   /* testNotCompletedThisOperationCycle */
#define DEM_UDS_STATUS_WIR      0x80u   /* warningIndicatorRequested (not supported) */
#define DEM_UDS_STATUS_AVAILABILITY_MASK 0x7Fu
#define DEM_UDS_STATUS_CLEARED  (DEM_UDS_STATUS_TNCSLC | DEM_UDS_STATUS_TNCTOC)

#define DEM_DTC_GROUP_ALL_DTCS  0x00FFFFFFu

/* Freeze frame as stored in the NvM block (24 bytes; layout is part of the Flash image). */
typedef struct __attribute__((packed))
{
    uint16 occurrenceCnt;
    uint8  year;
    uint8  month;
    uint8  day;
    uint8  hour;
    uint8  minute;
    uint8  second;
    uint8  vehStatus;
    uint16 L1VFB;
    uint16 T30VFB;
    float  L1NTC;
    float  L1ISENSE;
    uint16 padding2;
    uint8  padding1;
} Dem_FreezeFrame_t;

/* One data identifier of the freeze frame record. */
typedef struct
{
    uint16 did;
    uint8  offset;      /* byte offset inside Dem_FreezeFrame_t */
    uint8  size;        /* 1, 2 (big endian on the wire) or 4 (raw) */
} Dem_FreezeFrameDidType;

typedef struct
{
    Dem_DTCType dtc;
} Dem_EventConfigType;

typedef struct
{
    const Dem_EventConfigType     *events;
    uint16                         numEvents;
    const Dem_FreezeFrameDidType  *freezeFrameDids;
    uint8                          numFreezeFrameDids;
    void                         (*captureFreezeFrame)(Dem_FreezeFrame_t *frame);  /* fills everything but occurrenceCnt */
} Dem_ConfigType;

#endif /* ELC_BSW_DEM_TYPES_H */
