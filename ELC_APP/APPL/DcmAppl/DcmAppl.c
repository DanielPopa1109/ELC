#include "DcmAppl.h"
#include "EcuM.h"
#include "SMon.h"
#include "NvM.h"
#include "Com.h"
#include <string.h>

#define DCMAPPL_VEHICLE_STATE_PROGRAMMING_ALLOWED 6u   /* vehicle state in which flashing is permitted */

extern float OS_XCP_CpuLoad;   /* freertos.c */

uint8 DcmAppl_LoadStatus = DCMAPPL_LOADSTATUS_NONE;
static uint32 DcmAppl_LoadTimerTicks;

static void DcmAppl_PutU32(uint8 *data, uint32 value)
{
    data[0] = (uint8)value;
    data[1] = (uint8)(value >> 8);
    data[2] = (uint8)(value >> 16);
    data[3] = (uint8)(value >> 24);
}

uint8 DcmAppl_ReadResetCounter(uint8 *data)     { DcmAppl_PutU32(data, EcuM_TotalResetCounter); return E_OK; }
uint8 DcmAppl_ReadTimeActive(uint8 *data)       { DcmAppl_PutU32(data, EcuM_TimeActive); return E_OK; }
uint8 DcmAppl_ReadTimeWithoutReset(uint8 *data) { DcmAppl_PutU32(data, EcuM_TimeWithoutReset); return E_OK; }

uint8 DcmAppl_ReadResetData(uint8 *data)
{
    data[0] = EcuM_ResetReason;
    data[1] = EcuM_ResetInfo;
    return E_OK;
}

uint8 DcmAppl_ReadCpuLoad(uint8 *data)          { data[0] = (uint8)OS_XCP_CpuLoad; return E_OK; }
uint8 DcmAppl_ReadLoadStatus(uint8 *data)       { data[0] = SMon_L1ST; return E_OK; }
uint8 DcmAppl_ReadActiveSession(uint8 *data)    { data[0] = (uint8)Dcm_ActiveSessionState; return E_OK; }
uint8 DcmAppl_ReadActiveBlock(uint8 *data)      { data[0] = 1u; return E_OK; }

uint8 DcmAppl_ReadSoftwareVersion(uint8 *data)
{
    data[0] = EcuM_SoftwareVersion[0];
    data[1] = EcuM_SoftwareVersion[1];
    data[2] = SMon_CalibGetVersion();               /* calibration version byte */
    data[3] = EcuM_SoftwareVersion[3];
    return E_OK;
}

uint8 DcmAppl_ReadSoftwareVersionFcd(uint8 *data)
{
    uint8 i;

    for (i = 0u; i < 3u; ++i)
    {
        data[i] = EcuM_SoftwareVersion[i];
        data[i + 3u] = DCMAPPL_FBL_SW_VERSION;
    }
    return E_OK;
}

/* ---- Routines ---------------------------------------------------------------------------- */
uint8 DcmAppl_RoutineHealRetry(const uint8 *option)
{
    (void)option;
    SMon_RetryCnt = 0u;
    SMon_LockSupply = 0u;
    return 0u;
}

uint8 DcmAppl_RoutineTimedLoad(const uint8 *option)
{
    if ((option[0] > 1u) || (option[1] == 0u))
    {
        return DCM_NRC_REQUEST_OUT_OF_RANGE;
    }
    DcmAppl_LoadStatus = option[0];
    DcmAppl_LoadTimerTicks = (uint32)option[1] * (1000u / DCM_MAIN_PERIOD_MS);   /* seconds */
    return 0u;
}

/* ---- Reset / programming session entry --------------------------------------------------- */
uint8 DcmAppl_PrepareReset(Dcm_OpStatusType opStatus, boolean programming)
{
    static boolean writeStarted;
    NvM_RequestResultType result;

    if (opStatus == DCM_INITIAL)
    {
        uint8 vehicleState = 0u;

        writeStarted = FALSE;
        if ((programming != 0u) &&
            ((Com_ReceiveSignal(COM_SIGNAL_VEHICLESTATE, &vehicleState) != E_OK) ||
             (vehicleState != DCMAPPL_VEHICLE_STATE_PROGRAMMING_ALLOWED)))
        {
            return DCM_NRC_CONDITIONS_NOT_CORRECT;
        }
    }
    if (writeStarted == FALSE)
    {
        /* Persist diagnostic data first; Flash work must not stall a running load output. */
        if (NvM_IsWriteAllowed() == 0u)
        {
            return DCM_NRC_CONDITIONS_NOT_CORRECT;
        }
        if (NvM_WriteAll() == E_OK)
        {
            writeStarted = TRUE;
        }
        else if ((NvM_GetErrorStatus(NVM_BLOCK_ID_ALL, &result) == E_OK) && (result == NVM_REQ_PENDING))
        {
            return DCM_E_PENDING;                    /* another write (DEM) is still running: start after it */
        }
        else
        {
            return DCM_NRC_CONDITIONS_NOT_CORRECT;
        }
    }
    if (NvM_GetErrorStatus(NVM_BLOCK_ID_ALL, &result) != E_OK)
    {
        return DCM_NRC_GENERAL_PROGRAMMING_FAILURE;
    }
    if (result == NVM_REQ_PENDING)
    {
        return DCM_E_PENDING;
    }
    return (result == NVM_REQ_OK) ? 0u : DCM_NRC_GENERAL_PROGRAMMING_FAILURE;
}

void DcmAppl_ExecuteReset(void)
{
    EcuM_PerformReset(0u, 0u);
}

void DcmAppl_SessionChanged(uint8 session)
{
    if (session == DCM_SESSION_DEFAULT)
    {
        DcmAppl_LoadStatus = DCMAPPL_LOADSTATUS_NONE;
        DcmAppl_LoadTimerTicks = 0u;
    }
}

void DcmAppl_MainFunction(void)
{
    if ((DcmAppl_LoadTimerTicks != 0u) && (--DcmAppl_LoadTimerTicks == 0u))
    {
        DcmAppl_LoadStatus = DCMAPPL_LOADSTATUS_NONE;
    }
}
