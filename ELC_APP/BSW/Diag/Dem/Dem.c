#include "Dem.h"
#include "../../Mem/Nvm/NvM.h"
#include "../../Com/Com/Com.h"
#include "main.h"
#include <string.h>

#define DEM_PERSIST_DELAY_TICKS  10u    /* coalesce status changes: 50 ms at 5 ms */
#define DEM_CLEAR_HOLD_TICKS     100u   /* an unclaimed clear result expires after 500 ms */

/* Clear job: WAIT = accepted, starts when the running NvM job ends; PENDING = RAM cleared, durable
 * commit outstanding; OK/FAILED/BUSY = result waiting to be collected by the next Dem_ClearDTC() call. */
enum { DEM_CLRSTATE_NONE = 0u, DEM_CLRSTATE_WAIT, DEM_CLRSTATE_PENDING, DEM_CLRSTATE_OK, DEM_CLRSTATE_FAILED,
       DEM_CLRSTATE_BUSY };

static const Dem_ConfigType *Dem_ConfigPtr;
static boolean Dem_Enabled;            /* UDS 0x85 DTC setting */
static uint8   Dem_Dirty;
static uint8   Dem_Writing;
static uint8   Dem_Clearing;           /* RAM cleared, durable commit outstanding */
static uint8   Dem_ClearState;
static uint8   Dem_ClearHold;
static Dem_DTCType Dem_ClearGroup;
static uint8   Dem_Coalesce;
static uint32  Dem_Revision;
static uint32  Dem_Submitted;
static uint32  Dem_Broadcast;          /* events whose first failure is still to be broadcast */
static uint32  Dem_DeferredFailed;     /* reports received while a clear was in progress */
static uint32  Dem_DeferredPassed;
static uint8   Dem_FilterMask;
static uint16  Dem_FilterIndex;

void Dem_Init(const Dem_ConfigType *ConfigPtr)
{
    Dem_ConfigPtr = (ConfigPtr != NULL_PTR) ? ConfigPtr : &Dem_Config;
    Dem_Enabled = TRUE;
    Dem_Dirty = 0u;
    Dem_Writing = 0u;
    Dem_Clearing = 0u;
    Dem_ClearState = DEM_CLRSTATE_NONE;
    Dem_ClearHold = 0u;
    Dem_Coalesce = 0u;
    Dem_Revision = 0u;
    Dem_Submitted = 0u;
    Dem_Broadcast = 0u;
    Dem_DeferredFailed = 0u;
    Dem_DeferredPassed = 0u;
    Dem_FilterMask = 0u;
    Dem_FilterIndex = 0u;
}

static sint16 Dem_FindEvent(Dem_DTCType dtc)
{
    uint16 i;

    if (Dem_ConfigPtr == NULL_PTR)
    {
        return -1;
    }
    for (i = 0u; i < Dem_ConfigPtr->numEvents; ++i)
    {
        if (Dem_ConfigPtr->events[i].dtc == dtc)
        {
            return (sint16)i;
        }
    }
    return -1;
}

static void Dem_CaptureFreezeFrame(uint16 index)
{
    uint16 count = Dem_FF[index].occurrenceCnt;

    if (Dem_ConfigPtr->captureFreezeFrame != NULL_PTR)
    {
        Dem_ConfigPtr->captureFreezeFrame(&Dem_FF[index]);
    }
    Dem_FF[index].occurrenceCnt = (count == UINT16_MAX) ? count : (uint16)(count + 1u);
}

Std_ReturnType Dem_SetEventStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus)
{
    uint32 primask;
    uint8 old;
    uint8 status;

    if ((Dem_ConfigPtr == NULL_PTR) || (EventId >= Dem_ConfigPtr->numEvents) ||
        ((EventStatus != DEM_EVENT_STATUS_PASSED) && (EventStatus != DEM_EVENT_STATUS_FAILED)))
    {
        return E_NOT_OK;
    }
    if (Dem_Enabled == FALSE)
    {
        return E_OK;                         /* DTC setting off: status is frozen */
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (Dem_Clearing != 0u)
    {
        if (EventStatus == DEM_EVENT_STATUS_FAILED)
        {
            Dem_DeferredFailed |= 1u << EventId;
            Dem_DeferredPassed &= ~(1u << EventId);
        }
        else
        {
            Dem_DeferredPassed |= 1u << EventId;
            Dem_DeferredFailed &= ~(1u << EventId);
        }
        __set_PRIMASK(primask);
        return E_OK;
    }

    old = (uint8)Dem_DTC_Stat[EventId];
    status = old;
    if (EventStatus == DEM_EVENT_STATUS_FAILED)
    {
        /* testFailed, thisCycle, pending, confirmed (one qualified failure), sinceLastClear;
         * the test is completed for both windows. */
        status = (uint8)((old & ~(DEM_UDS_STATUS_TNCSLC | DEM_UDS_STATUS_TNCTOC)) |
                         DEM_UDS_STATUS_TF | DEM_UDS_STATUS_TFTOC | DEM_UDS_STATUS_PDTC |
                         DEM_UDS_STATUS_CDTC | DEM_UDS_STATUS_TFSLC);
        if ((old & DEM_UDS_STATUS_TF) == 0u)
        {
            Dem_CaptureFreezeFrame(EventId);
            Dem_Broadcast |= 1u << EventId;
        }
    }
    else
    {
        status = (uint8)(old & ~(DEM_UDS_STATUS_TF | DEM_UDS_STATUS_TNCSLC | DEM_UDS_STATUS_TNCTOC));
    }
    if (status != old)
    {
        Dem_DTC_Stat[EventId] = status;
        if (Dem_Dirty == 0u)
        {
            Dem_Coalesce = DEM_PERSIST_DELAY_TICKS;
        }
        Dem_Dirty = 1u;
        Dem_Revision++;
    }
    __set_PRIMASK(primask);
    return E_OK;
}

Std_ReturnType Dem_GetEventUdsStatus(Dem_EventIdType EventId, Dem_UdsStatusByteType *EventStatusByte)
{
    if ((Dem_ConfigPtr == NULL_PTR) || (EventId >= Dem_ConfigPtr->numEvents) || (EventStatusByte == NULL_PTR))
    {
        return E_NOT_OK;
    }
    *EventStatusByte = (uint8)Dem_DTC_Stat[EventId];
    return E_OK;
}

Std_ReturnType Dem_GetEventFailed(Dem_EventIdType EventId, boolean *EventFailed)
{
    uint8 status;

    if ((EventFailed == NULL_PTR) || (Dem_GetEventUdsStatus(EventId, &status) != E_OK))
    {
        return E_NOT_OK;
    }
    *EventFailed = ((status & DEM_UDS_STATUS_TF) != 0u) ? TRUE : FALSE;
    return E_OK;
}

boolean Dem_IsEventFailed(Dem_EventIdType EventId)
{
    boolean failed = FALSE;

    (void)Dem_GetEventFailed(EventId, &failed);
    return failed;
}

Std_ReturnType Dem_GetDTCOfEvent(Dem_EventIdType EventId, Dem_DTCFormatType DTCFormat, Dem_DTCType *DTCOfEvent)
{
    if ((Dem_ConfigPtr == NULL_PTR) || (EventId >= Dem_ConfigPtr->numEvents) || (DTCOfEvent == NULL_PTR) ||
        (DTCFormat != DEM_DTC_FORMAT_UDS))
    {
        return E_NOT_OK;
    }
    *DTCOfEvent = Dem_ConfigPtr->events[EventId].dtc;
    return E_OK;
}

Std_ReturnType Dem_SetOperationCycleState(Dem_OperationCycleIdType OperationCycleId,
                                          Dem_OperationCycleStateType CycleState)
{
    uint16 i;

    if ((Dem_ConfigPtr == NULL_PTR) || (OperationCycleId != DEM_OPERATION_CYCLE_POWER))
    {
        return E_NOT_OK;
    }
    if (Dem_Clearing != 0u)
    {
        return E_NOT_OK;
    }
    for (i = 0u; i < Dem_ConfigPtr->numEvents; ++i)
    {
        uint8 old = (uint8)Dem_DTC_Stat[i];
        uint8 status = old;

        if (CycleState == DEM_CYCLE_STATE_START)
        {
            status = (uint8)((status & ~DEM_UDS_STATUS_TFTOC) | DEM_UDS_STATUS_TNCTOC);
        }
        else if ((status & (DEM_UDS_STATUS_TFTOC | DEM_UDS_STATUS_TNCTOC)) == 0u)
        {
            status = (uint8)(status & ~DEM_UDS_STATUS_PDTC);   /* tested and passed in this cycle */
        }
        if (status != old)
        {
            Dem_DTC_Stat[i] = status;
            Dem_Dirty = 1u;
            Dem_Revision++;
        }
    }
    /* No calibrated aging threshold exists: confirmed history remains until cleared. */
    return E_OK;
}

Std_ReturnType Dem_SetDtcSetting(boolean Enabled)
{
    Dem_Enabled = (Enabled != 0u) ? TRUE : FALSE;
    return E_OK;
}

boolean Dem_IsDtcSettingEnabled(void)
{
    return Dem_Enabled;
}

uint8 Dem_GetDTCStatusAvailabilityMask(void)
{
    return DEM_UDS_STATUS_AVAILABILITY_MASK;
}

boolean Dem_IsPersistencePending(void)
{
    return (boolean)((Dem_Dirty != 0u) || (Dem_Writing != 0u) || (Dem_Clearing != 0u));
}

Std_ReturnType Dem_GetStatusOfDTC(Dem_DTCType DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin,
                                  Dem_UdsStatusByteType *DTCStatus)
{
    sint16 index = Dem_FindEvent(DTC);

    if ((index < 0) || (DTCStatus == NULL_PTR) || (DTCFormat != DEM_DTC_FORMAT_UDS) ||
        (DTCOrigin != DEM_DTC_ORIGIN_PRIMARY_MEMORY))
    {
        return E_NOT_OK;
    }
    *DTCStatus = (uint8)Dem_DTC_Stat[index];
    return E_OK;
}

/* ---- DTC filter ------------------------------------------------------------------ */
Std_ReturnType Dem_SetDTCFilter(uint8 DTCStatusMask, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin)
{
    if ((DTCFormat != DEM_DTC_FORMAT_UDS) || (DTCOrigin != DEM_DTC_ORIGIN_PRIMARY_MEMORY))
    {
        return E_NOT_OK;
    }
    Dem_FilterMask = (uint8)(DTCStatusMask & DEM_UDS_STATUS_AVAILABILITY_MASK);
    Dem_FilterIndex = 0u;
    return E_OK;
}

Std_ReturnType Dem_GetNumberOfFilteredDTC(uint16 *NumberOfFilteredDTC)
{
    uint16 i;
    uint16 count = 0u;

    if ((Dem_ConfigPtr == NULL_PTR) || (NumberOfFilteredDTC == NULL_PTR))
    {
        return E_NOT_OK;
    }
    for (i = 0u; i < Dem_ConfigPtr->numEvents; ++i)
    {
        if (((uint8)Dem_DTC_Stat[i] & Dem_FilterMask) != 0u)
        {
            ++count;
        }
    }
    *NumberOfFilteredDTC = count;
    return E_OK;
}

Std_ReturnType Dem_GetNextFilteredDTC(Dem_DTCType *DTC, Dem_UdsStatusByteType *DTCStatus)
{
    if ((Dem_ConfigPtr == NULL_PTR) || (DTC == NULL_PTR) || (DTCStatus == NULL_PTR))
    {
        return E_NOT_OK;
    }
    while (Dem_FilterIndex < Dem_ConfigPtr->numEvents)
    {
        uint16 i = Dem_FilterIndex++;
        uint8 status = (uint8)Dem_DTC_Stat[i];

        if ((status & Dem_FilterMask) != 0u)
        {
            *DTC = Dem_ConfigPtr->events[i].dtc;
            *DTCStatus = status;
            return E_OK;
        }
    }
    return E_NOT_OK;
}

/* ---- Freeze frame ---------------------------------------------------------------- */
Std_ReturnType Dem_GetSnapshotDataByDTC(Dem_DTCType DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin,
                                        uint8 RecordNumber, uint8 *DestBuffer, uint16 *BufSize)
{
    sint16 index = Dem_FindEvent(DTC);
    Dem_FreezeFrame_t frame;
    const uint8 *raw = (const uint8 *)&frame;
    uint16 used = 1u;
    uint8 i;
    uint32 primask;

    if ((index < 0) || (DestBuffer == NULL_PTR) || (BufSize == NULL_PTR) || (DTCFormat != DEM_DTC_FORMAT_UDS) ||
        (DTCOrigin != DEM_DTC_ORIGIN_PRIMARY_MEMORY) || ((RecordNumber != 1u) && (RecordNumber != 0xFFu)) ||
        (Dem_FF[index].occurrenceCnt == 0u))
    {
        return E_NOT_OK;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    (void)memcpy(&frame, &Dem_FF[index], sizeof(frame));
    __set_PRIMASK(primask);

    if (*BufSize < 1u)
    {
        return E_NOT_OK;
    }
    DestBuffer[0] = Dem_ConfigPtr->numFreezeFrameDids;
    for (i = 0u; i < Dem_ConfigPtr->numFreezeFrameDids; ++i)
    {
        const Dem_FreezeFrameDidType *did = &Dem_ConfigPtr->freezeFrameDids[i];

        if ((uint16)(used + 2u + did->size) > *BufSize)
        {
            return E_NOT_OK;
        }
        DestBuffer[used++] = (uint8)(did->did >> 8);
        DestBuffer[used++] = (uint8)did->did;
        if (did->size == 2u)
        {   /* integers are big endian on the wire, stored little endian */
            DestBuffer[used++] = raw[did->offset + 1u];
            DestBuffer[used++] = raw[did->offset];
        }
        else
        {
            (void)memcpy(&DestBuffer[used], &raw[did->offset], did->size);
            used = (uint16)(used + did->size);
        }
    }
    *BufSize = used;
    return E_OK;
}

/* ---- Extended data (UDS 0x19 0x06) -------------------------------------------------- */
/* Record 0x01 is the occurrence counter (2 bytes, big endian). Writes the record number followed by its data. */
Std_ReturnType Dem_GetExtendedDataRecordByDTC(Dem_DTCType DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin,
                                              uint8 RecordNumber, uint8 *DestBuffer, uint16 *BufSize)
{
    sint16 index = Dem_FindEvent(DTC);

    if ((index < 0) || (DestBuffer == NULL_PTR) || (BufSize == NULL_PTR) || (*BufSize < DEM_EXTENDED_DATA_LEN) ||
        (DTCFormat != DEM_DTC_FORMAT_UDS) || (DTCOrigin != DEM_DTC_ORIGIN_PRIMARY_MEMORY) ||
        ((RecordNumber != DEM_EXTENDED_DATA_OCCURRENCE) && (RecordNumber != 0xFFu)) ||
        (Dem_FF[index].occurrenceCnt == 0u))
    {
        return E_NOT_OK;
    }
    DestBuffer[0] = DEM_EXTENDED_DATA_OCCURRENCE;
    DestBuffer[1] = (uint8)(Dem_FF[index].occurrenceCnt >> 8);
    DestBuffer[2] = (uint8)Dem_FF[index].occurrenceCnt;
    *BufSize = DEM_EXTENDED_DATA_LEN;
    return E_OK;
}

/* ---- Clear ------------------------------------------------------------------------- */
/* Clears the RAM image and requests the durable commit; reports and status changes are deferred
 * until the commit has finished (Dem_Clearing). */
static void Dem_StartClear(Dem_DTCType group)
{
    uint32 primask = __get_PRIMASK();
    uint16 i;

    __disable_irq();
    for (i = 0u; i < Dem_ConfigPtr->numEvents; ++i)
    {
        if ((group == DEM_DTC_GROUP_ALL_DTCS) || (group == Dem_ConfigPtr->events[i].dtc))
        {
            Dem_DTC_Stat[i] = DEM_UDS_STATUS_CLEARED;
            (void)memset(&Dem_FF[i], 0, sizeof(Dem_FF[i]));
            Dem_Broadcast &= ~(1u << i);
        }
    }
    Dem_Clearing = 1u;
    Dem_ClearState = DEM_CLRSTATE_PENDING;
    Dem_Dirty = 1u;
    Dem_Revision++;
    __set_PRIMASK(primask);
}

Dem_ReturnClearDTCType Dem_ClearDTC(Dem_DTCType DTC, Dem_DTCFormatType DTCFormat, Dem_DTCOriginType DTCOrigin)
{
    NvM_RequestResultType nvmResult = NVM_REQ_NOT_OK;

    if (DTCFormat != DEM_DTC_FORMAT_UDS)
    {
        return DEM_CLEAR_WRONG_DTC;
    }
    if (DTCOrigin != DEM_DTC_ORIGIN_PRIMARY_MEMORY)
    {
        return DEM_CLEAR_WRONG_DTCORIGIN;
    }
    switch (Dem_ClearState)
    {
    case DEM_CLRSTATE_WAIT:
    case DEM_CLRSTATE_PENDING:
        return DEM_CLEAR_PENDING;
    case DEM_CLRSTATE_OK:
        Dem_ClearState = DEM_CLRSTATE_NONE;
        return DEM_CLEAR_OK;
    case DEM_CLRSTATE_FAILED:
        Dem_ClearState = DEM_CLRSTATE_NONE;
        return DEM_CLEAR_MEMORY_ERROR;
    case DEM_CLRSTATE_BUSY:
        Dem_ClearState = DEM_CLRSTATE_NONE;
        return DEM_CLEAR_BUSY;
    default:
        break;
    }

    if ((Dem_ConfigPtr == NULL_PTR) || ((DTC != DEM_DTC_GROUP_ALL_DTCS) && (Dem_FindEvent(DTC) < 0)))
    {
        return DEM_CLEAR_WRONG_DTC;
    }
    if ((NvM_IsWriteAllowed() == 0u) || (Dem_Clearing != 0u) ||
        (NvM_GetErrorStatus(NVM_BLOCK_ID_ALL, &nvmResult) != E_OK))
    {
        return DEM_CLEAR_BUSY;
    }
    Dem_ClearGroup = DTC;
    Dem_ClearHold = 0u;
    if (nvmResult == NVM_REQ_PENDING)
    {
        Dem_ClearState = DEM_CLRSTATE_WAIT;   /* e.g. our own coalesced write: start when it has finished */
    }
    else
    {
        Dem_StartClear(DTC);
    }
    return DEM_CLEAR_PENDING;
}

/* ---- Main function ------------------------------------------------------------------ */
static void Dem_FinishClear(uint8 state)
{
    Dem_Clearing = 0u;
    Dem_ClearState = state;
    Dem_ClearHold = 0u;
}

void Dem_MainFunction(void)
{
    NvM_RequestResultType result = NVM_REQ_NOT_OK;
    uint32 primask;
    uint16 i;

    if (Dem_ConfigPtr == NULL_PTR)
    {
        return;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    (void)NvM_GetErrorStatus(NVM_BLOCK_ID_ALL, &result);

    if ((Dem_Writing != 0u) && (result != NVM_REQ_PENDING))
    {
        Dem_Writing = 0u;
        if (result == NVM_REQ_OK)
        {
            if (Dem_Revision == Dem_Submitted)
            {
                Dem_Dirty = 0u;
            }
            if (Dem_Clearing != 0u)
            {
                Dem_FinishClear(DEM_CLRSTATE_OK);
            }
        }
        else
        {
            if (Dem_Clearing != 0u)
            {
                Dem_FinishClear(DEM_CLRSTATE_FAILED);
            }
            Dem_Dirty = 0u;                  /* bounded: no endless retry of a failing write */
        }
    }
    if (Dem_ClearState == DEM_CLRSTATE_WAIT)
    {
        if (NvM_IsWriteAllowed() == 0u)
        {
            Dem_FinishClear(DEM_CLRSTATE_BUSY);   /* Flash work became unavailable while waiting */
        }
        else if (result != NVM_REQ_PENDING)
        {
            Dem_StartClear(Dem_ClearGroup);
        }
    }
    if ((Dem_ClearHold < DEM_CLEAR_HOLD_TICKS) && (Dem_ClearState >= DEM_CLRSTATE_OK) &&
        (++Dem_ClearHold >= DEM_CLEAR_HOLD_TICKS))
    {
        Dem_ClearState = DEM_CLRSTATE_NONE;
    }
    if ((Dem_Coalesce != 0u) && (Dem_Clearing == 0u))
    {
        Dem_Coalesce--;
    }
    if ((Dem_Dirty != 0u) && (Dem_Writing == 0u) && ((Dem_Coalesce == 0u) || (Dem_Clearing != 0u)) &&
        (result != NVM_REQ_PENDING))
    {
        if (NvM_WriteAll() == E_OK)
        {
            Dem_Submitted = Dem_Revision;
            Dem_Writing = 1u;
        }
        else if (Dem_Clearing != 0u)
        {
            Dem_FinishClear(DEM_CLRSTATE_FAILED);
            Dem_Dirty = 0u;
        }
    }
    if ((Dem_Clearing == 0u) && ((Dem_DeferredFailed != 0u) || (Dem_DeferredPassed != 0u)))
    {
        uint32 failed = Dem_DeferredFailed;
        uint32 passed = Dem_DeferredPassed;

        Dem_DeferredFailed = 0u;
        Dem_DeferredPassed = 0u;
        for (i = 0u; i < Dem_ConfigPtr->numEvents; ++i)
        {
            if (((failed | passed) & (1u << i)) != 0u)
            {
                (void)Dem_SetEventStatus(i, ((failed & (1u << i)) != 0u) ? DEM_EVENT_STATUS_FAILED
                                                                          : DEM_EVENT_STATUS_PASSED);
            }
        }
    }
    for (i = 0u; i < Dem_ConfigPtr->numEvents; ++i)
    {
        if ((Dem_Broadcast & (1u << i)) != 0u)
        {
            /* DTC broadcast 0x7FE: DTC and status packed as in the gateway's fault frame. */
            Dem_DTCType dtc = Dem_ConfigPtr->events[i].dtc;
            uint8 st = (uint8)Dem_DTC_Stat[i];
            uint8 data[4];

            data[0] = (uint8)dtc;
            data[1] = (uint8)(dtc >> 8);
            data[2] = (uint8)(((dtc >> 16) & 0x3Fu) | (((dtc >> 24) & 3u) << 6));
            data[3] = (uint8)(((dtc >> 26) & 3u) | ((st & 0x3Fu) << 2));
            if (Com_SendSignal(COM_SIGNAL_DTC_BROADCAST, data) == E_OK)
            {
                Dem_Broadcast &= ~(1u << i);
            }
            break;
        }
    }
    __set_PRIMASK(primask);
}
