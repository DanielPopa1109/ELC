#ifndef DEM_H
#define DEM_H

#include <stdint.h>

#define DEM_MAX_FF_DTC        18u

typedef struct __attribute__((packed))
{
    uint16_t occurrenceCnt;
    uint8_t  year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  vehStatus;
    uint16_t L1VFB;
    uint16_t T30VFB;
    float    L1NTC;
    float    L1ISENSE;
    uint16_t padding2;
    uint8_t  padding1;
} Dem_FreezeFrame_t;

extern Dem_FreezeFrame_t Dem_FF[DEM_MAX_FF_DTC];
extern const uint32_t Dem_PreDefined_DTC_Table[DEM_MAX_FF_DTC];
extern uint32_t Dem_DTC_Stat[DEM_MAX_FF_DTC];

extern void Dem_SetDtc(uint32_t id, uint8_t stat);
extern void Dem_CaptureFreezeFrame(uint8_t idx);
extern uint8_t Dem_GetDtcStatus(uint32_t id);
extern void Dem_main(void);
#define DEM_STATUS_MASK 0x7Fu
void Dem_Init(void);
void Dem_SetOperationCycle(uint8_t start);
int Dem_GetEvent(uint8_t index, uint32_t *dtc, uint8_t *status);
int Dem_GetSnapshot(uint32_t dtc, Dem_FreezeFrame_t *snapshot);
int Dem_ClearDTC(uint32_t group);
int Dem_GetClearResult(void); /* 1 pending, 0 durable success, -1 failure */
void Dem_SetDTCSetting(uint8_t enabled);
void Dem_AppCapture(Dem_FreezeFrame_t *snapshot);

#endif /* DEM_H */
