/* Dem.h - Diagnostic Event Manager (ELC subset, AUTOSAR API shape).
 *
 * Monitors (SMon, CanH, XCP, EcuM) report PASSED/FAILED through Dem_SetEventStatus();
 * debouncing stays in the monitors, so PREPASSED/PREFAILED are rejected. One failed report
 * confirms the DTC; there is no calibrated aging threshold, so confirmed history is kept
 * until it is cleared. Status and freeze frames are persisted through NvM. */
#ifndef ELC_BSW_DEM_H
#define ELC_BSW_DEM_H

#include "Dem_Types.h"
#include "Dem_Cfg.h"

void Dem_Init(const Dem_ConfigType *ConfigPtr);
void Dem_MainFunction(void);   /* 5 ms: persistence, deferred reports, DTC broadcast */

Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus);
Std_ReturnType Dem_GetEventFailed(Dem_EventIdType EventId, boolean *EventFailed);
Std_ReturnType Dem_GetEventUdsStatus(Dem_EventIdType EventId, Dem_UdsStatusByteType *EventStatusByte);
Std_ReturnType Dem_GetDTCOfEvent(Dem_EventIdType EventId, Dem_DTCFormatType DTCFormat, Dem_DTCType *DTCOfEvent);
boolean Dem_IsEventFailed(Dem_EventIdType EventId);   /* convenience wrapper of Dem_GetEventFailed() */

Std_ReturnType Dem_SetOperationCycleState(Dem_OperationCycleIdType OperationCycleId,
                                          Dem_OperationCycleStateType CycleState);
Std_ReturnType Dem_SetDtcSetting(boolean Enabled);     /* UDS 0x85 */
boolean Dem_IsDtcSettingEnabled(void);
boolean Dem_IsPersistencePending(void);                /* changed DEM data is not yet durable (EcuM: no sleep) */

uint8 Dem_GetDTCStatusAvailabilityMask(void);
Std_ReturnType Dem_GetStatusOfDTC(Dem_DTCType DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin,
                                  Dem_UdsStatusByteType *DTCStatus);

/* DTC filter (UDS 0x19 0x01/0x02). */
Std_ReturnType Dem_SetDTCFilter(uint8 DTCStatusMask, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin);
Std_ReturnType Dem_GetNumberOfFilteredDTC(uint16 *NumberOfFilteredDTC);
Std_ReturnType Dem_GetNextFilteredDTC(Dem_DTCType *DTC, Dem_UdsStatusByteType *DTCStatus);

/* Freeze frame (UDS 0x19 0x04): writes numberOfIdentifiers followed by DID/data pairs. */
Std_ReturnType Dem_GetSnapshotDataByDTC(Dem_DTCType DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin,
                                        uint8 RecordNumber, uint8 *DestBuffer, uint16 *BufSize);

/* Extended data (UDS 0x19 0x06): record 0x01 (or 0xFF) returns record number + occurrence counter. */
Std_ReturnType Dem_GetExtendedDataRecordByDTC(Dem_DTCType DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin,
                                              uint8 RecordNumber, uint8 *DestBuffer, uint16 *BufSize);

/* UDS 0x14. Call repeatedly while DEM_CLEAR_PENDING is returned; the final result is
 * DEM_CLEAR_OK only after the cleared state is durable. */
Dem_ReturnClearDTCType Dem_ClearDTC(Dem_DTCType DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin);

#endif /* ELC_BSW_DEM_H */
