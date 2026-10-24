#include "Dcm.h"
#include "DcmAppl.h"
#include "Dem.h"
#include "../../Com/PduR/PduR.h"
#include "../../Com/ComM/ComM.h"
#include <string.h>

typedef enum
{
    DCM_ST_IDLE = 0u,
    DCM_ST_READY,        /* complete request received, not yet dispatched */
    DCM_ST_PROC,         /* handler returned DCM_E_PENDING */
    DCM_ST_TX_WAIT,      /* response built, waiting for CanTp to accept it */
    DCM_ST_TX_ACTIVE     /* response in transmission */
} Dcm_StateType;

enum { DCM_POST_NONE = 0u, DCM_POST_RESET, DCM_POST_PROGRAMMING };

uint32 Dcm_ActiveSessionState __attribute__((section(".ncr.session")));

static const Dcm_ConfigType *Dcm_ConfigPtr;
static uint8  Dcm_RxBuffer[DCM_RX_BUFFER_LEN];
static uint8  Dcm_TxBuffer[DCM_TX_BUFFER_LEN];
static uint16 Dcm_RxLength;
static uint16 Dcm_RxExpected;
static uint16 Dcm_TxLength;
static uint16 Dcm_TxOffset;
static uint16 Dcm_PendingTimerMs;
static uint16 Dcm_S3Ticks;
static Dcm_StateType Dcm_State;
static uint8  Dcm_RxActive;
static uint8  Dcm_PostAction;
static uint8  Dcm_SuppressPositive;
static uint8  Dcm_TxIsPending;     /* the response in transmission is NRC 0x78 */
static uint8  Dcm_Sid;
static const Dcm_ServiceType *Dcm_Service;

/* ---- Session handling ------------------------------------------------------------------ */
static void Dcm_ReloadS3(void)
{
    Dcm_S3Ticks = (uint16)(DCM_S3_SERVER_MS / DCM_MAIN_PERIOD_MS);
}

static void Dcm_SetSession(uint8 session)
{
    Dcm_ActiveSessionState = session;
    Dcm_ReloadS3();
    if (session == DCM_SESSION_DEFAULT)
    {
        /* Leaving a non-default session ends CommunicationControl and ControlDTCSetting. */
        (void)ComM_CommunicationControl(COMM_CHANNEL_CAN, 0u, 3u);
        (void)Dem_SetDtcSetting(TRUE);
    }
    DcmAppl_SessionChanged(session);
}

static void Dcm_GoIdle(void)
{
    Dcm_State = DCM_ST_IDLE;
    Dcm_ReloadS3();
}

/* ---- Response path ------------------------------------------------------------------------ */
static void Dcm_TryTransmit(void)
{
    PduInfoType info;

    info.SduDataPtr = NULL_PTR;           /* payload is pulled through Dcm_CopyTxData() */
    info.MetaDataPtr = NULL_PTR;
    info.SduLength = Dcm_TxLength;
    Dcm_TxOffset = 0u;
    if (PduR_DcmTransmit(DCM_TXPDU_DIAG, &info) == E_OK)
    {
        Dcm_State = DCM_ST_TX_ACTIVE;
    }
}

static void Dcm_StartTx(void)
{
    Dcm_State = DCM_ST_TX_WAIT;
    Dcm_TryTransmit();
}

static void Dcm_SendNrc(uint8 nrc)
{
    Dcm_TxBuffer[0] = 0x7Fu;
    Dcm_TxBuffer[1] = Dcm_Sid;
    Dcm_TxBuffer[2] = nrc;
    Dcm_TxLength = 3u;
    Dcm_TxIsPending = (uint8)(nrc == DCM_NRC_RESPONSE_PENDING);
    if (nrc != DCM_NRC_RESPONSE_PENDING)
    {
        Dcm_PostAction = DCM_POST_NONE;
    }
    Dcm_StartTx();
}

static void Dcm_ExecutePostAction(uint8 action)
{
    Dcm_ActiveSessionState = (action == DCM_POST_PROGRAMMING) ? DCM_SESSION_PROGRAMMING : DCM_SESSION_DEFAULT;
    DcmAppl_ExecuteReset();
}

/* ---- Request dispatch ---------------------------------------------------------------------- */
static void Dcm_RunHandler(Dcm_OpStatusType opStatus)
{
    uint16 length = (uint16)(DCM_TX_BUFFER_LEN - 1u);
    uint8 rc = Dcm_Service->handler(opStatus, Dcm_RxBuffer, Dcm_RxLength, &Dcm_TxBuffer[1], &length);

    if (rc == DCM_E_PENDING)
    {
        Dcm_State = DCM_ST_PROC;
        return;
    }
    if (rc != 0u)
    {
        Dcm_SendNrc(rc);
        return;
    }
    Dcm_TxBuffer[0] = (uint8)(Dcm_Sid | 0x40u);
    Dcm_TxLength = (uint16)(length + 1u);
    if (Dcm_SuppressPositive != 0u)
    {
        uint8 action = Dcm_PostAction;

        Dcm_PostAction = DCM_POST_NONE;
        Dcm_GoIdle();
        if (action != DCM_POST_NONE)
        {
            Dcm_ExecutePostAction(action);
        }
        return;
    }
    Dcm_TxIsPending = 0u;
    Dcm_StartTx();
}

static void Dcm_Dispatch(void)
{
    uint8 i;

    Dcm_Sid = Dcm_RxBuffer[0];
    Dcm_SuppressPositive = 0u;
    Dcm_PostAction = DCM_POST_NONE;
    Dcm_Service = NULL_PTR;
    for (i = 0u; i < Dcm_ConfigPtr->numServices; ++i)
    {
        if (Dcm_ConfigPtr->services[i].sid == Dcm_Sid)
        {
            Dcm_Service = &Dcm_ConfigPtr->services[i];
            break;
        }
    }
    if (Dcm_Service == NULL_PTR)
    {
        Dcm_SendNrc(DCM_NRC_SERVICE_NOT_SUPPORTED);
        return;
    }
    if ((Dcm_Service->sessionMask & (1u << Dcm_ActiveSessionState)) == 0u)
    {
        Dcm_SendNrc(DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);
        return;
    }
    if (Dcm_RxLength < Dcm_Service->minLength)
    {
        Dcm_SendNrc(DCM_NRC_INCORRECT_LENGTH);
        return;
    }
    if (Dcm_Service->hasSubFunction != 0u)
    {
        Dcm_SuppressPositive = (uint8)((Dcm_RxBuffer[1] & DCM_SUPPRESS_POS_RSP_MSG_INDICATION_BIT) != 0u);
        Dcm_RxBuffer[1] &= (uint8)~DCM_SUPPRESS_POS_RSP_MSG_INDICATION_BIT;
    }
    Dcm_PendingTimerMs = DCM_PENDING_FIRST_MS;
    Dcm_RunHandler(DCM_INITIAL);
}

/* ---- UDS services --------------------------------------------------------------------------- */
static uint8 Dcm_SessionControl(Dcm_OpStatusType op, const uint8 *req, uint16 len, uint8 *resp, uint16 *respLen)
{
    uint8 sub = req[1];

    if (len != 2u) { return DCM_NRC_INCORRECT_LENGTH; }
    if ((sub < DCM_SESSION_DEFAULT) || (sub > DCM_SESSION_EXTENDED)) { return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED; }
    if (sub == DCM_SESSION_PROGRAMMING)
    {
        uint8 rc;

        if (Dcm_ActiveSessionState != DCM_SESSION_EXTENDED) { return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_SESSION; }
        rc = DcmAppl_PrepareReset(op, TRUE);
        if (rc != 0u) { return rc; }
        Dcm_PostAction = DCM_POST_PROGRAMMING;   /* the session becomes active in the bootloader after reset */
    }
    else if (op == DCM_INITIAL)
    {
        Dcm_SetSession(sub);
    }
    resp[0] = sub;
    resp[1] = (uint8)(DCM_P2_SERVER_MS >> 8);
    resp[2] = (uint8)DCM_P2_SERVER_MS;
    resp[3] = (uint8)((DCM_P2STAR_SERVER_MS / 10u) >> 8);   /* P2* in 10 ms units */
    resp[4] = (uint8)(DCM_P2STAR_SERVER_MS / 10u);
    *respLen = 5u;
    return 0u;
}

static uint8 Dcm_EcuReset(Dcm_OpStatusType op, const uint8 *req, uint16 len, uint8 *resp, uint16 *respLen)
{
    uint8 rc;

    if (len != 2u) { return DCM_NRC_INCORRECT_LENGTH; }
    if (req[1] != 1u) { return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED; }   /* hardReset only */
    rc = DcmAppl_PrepareReset(op, FALSE);
    if (rc != 0u) { return rc; }
    Dcm_PostAction = DCM_POST_RESET;
    resp[0] = 1u;
    *respLen = 1u;
    return 0u;
}

static uint8 Dcm_ClearDiagnosticInformation(Dcm_OpStatusType op, const uint8 *req, uint16 len, uint8 *resp,
                                            uint16 *respLen)
{
    Dem_DTCType group;

    (void)op; (void)resp;
    if (len != 4u) { return DCM_NRC_INCORRECT_LENGTH; }
    group = ((Dem_DTCType)req[1] << 16) | ((Dem_DTCType)req[2] << 8) | req[3];
    switch (Dem_ClearDTC(group, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY))
    {
    case DEM_CLEAR_OK:             *respLen = 0u; return 0u;
    case DEM_CLEAR_PENDING:        return DCM_E_PENDING;
    case DEM_CLEAR_WRONG_DTC:
    case DEM_CLEAR_WRONG_DTCORIGIN: return DCM_NRC_REQUEST_OUT_OF_RANGE;
    case DEM_CLEAR_MEMORY_ERROR:   return DCM_NRC_GENERAL_PROGRAMMING_FAILURE;
    default:                       return DCM_NRC_CONDITIONS_NOT_CORRECT;
    }
}

static uint8 Dcm_ReadDtcInformation(Dcm_OpStatusType op, const uint8 *req, uint16 len, uint8 *resp, uint16 *respLen)
{
    const uint8 sub = req[1];
    uint16 n = 0u;
    uint8 st;
    Dem_DTCType dtc;

    (void)op;
    resp[n++] = sub;
    if ((sub == 0x01u) || (sub == 0x02u))
    {
        uint16 count;

        if (len != 3u) { return DCM_NRC_INCORRECT_LENGTH; }
        resp[n++] = Dem_GetDTCStatusAvailabilityMask();
        (void)Dem_SetDTCFilter(req[2], DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);
        if (sub == 0x01u)
        {
            (void)Dem_GetNumberOfFilteredDTC(&count);
            resp[n++] = 0x01u;                                    /* DTCFormatIdentifier: ISO 14229-1 */
            resp[n++] = (uint8)(count >> 8);
            resp[n++] = (uint8)count;
        }
        else
        {
            while (Dem_GetNextFilteredDTC(&dtc, &st) == E_OK)
            {
                if ((uint16)(n + 4u) > *respLen) { return DCM_NRC_RESPONSE_TOO_LONG; }
                resp[n++] = (uint8)(dtc >> 16);
                resp[n++] = (uint8)(dtc >> 8);
                resp[n++] = (uint8)dtc;
                resp[n++] = st;
            }
        }
    }
    else if (sub == 0x04u)
    {
        const uint8 record = req[5];
        const Dem_DTCType wanted = ((Dem_DTCType)req[2] << 16) | ((Dem_DTCType)req[3] << 8) | req[4];
        uint16 e;
        boolean found = FALSE;

        if (len != 6u) { return DCM_NRC_INCORRECT_LENGTH; }
        if ((record != 1u) && (record != 0xFFu)) { return DCM_NRC_REQUEST_OUT_OF_RANGE; }
        /* ELC extension: DTC 0xFFFFFF returns the record of every DTC that has one. */
        for (e = 0u; e < DEM_NUMBER_OF_EVENTS; ++e)
        {
            uint16 size;

            (void)Dem_GetDTCOfEvent(e, DEM_DTC_FORMAT_UDS, &dtc);
            if ((wanted != DEM_DTC_GROUP_ALL_DTCS) && (wanted != dtc)) { continue; }
            if ((uint16)(n + 5u + DEM_SNAPSHOT_MAX_LEN) > *respLen) { return DCM_NRC_RESPONSE_TOO_LONG; }
            size = (uint16)(*respLen - (n + 5u));
            if (Dem_GetSnapshotDataByDTC(dtc, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, record,
                                         &resp[n + 5u], &size) != E_OK)
            {
                continue;
            }
            (void)Dem_GetStatusOfDTC(dtc, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, &st);
            resp[n] = (uint8)(dtc >> 16);
            resp[n + 1u] = (uint8)(dtc >> 8);
            resp[n + 2u] = (uint8)dtc;
            resp[n + 3u] = st;
            resp[n + 4u] = 1u;                                   /* DTCSnapshotRecordNumber */
            n = (uint16)(n + 5u + size);
            found = TRUE;
        }
        if (found == FALSE) { return DCM_NRC_REQUEST_OUT_OF_RANGE; }
    }
    else if (sub == 0x06u)
    {   /* reportDTCExtDataRecordByDTCNumber: DTC, status, record number and data (record 0x01: occurrence counter) */
        uint16 size = (uint16)(*respLen - 5u);

        if (len != 6u) { return DCM_NRC_INCORRECT_LENGTH; }
        dtc = ((Dem_DTCType)req[2] << 16) | ((Dem_DTCType)req[3] << 8) | req[4];
        if ((Dem_GetStatusOfDTC(dtc, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, &st) != E_OK) ||
            (Dem_GetExtendedDataRecordByDTC(dtc, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, req[5],
                                            &resp[5], &size) != E_OK))
        {
            return DCM_NRC_REQUEST_OUT_OF_RANGE;
        }
        resp[n++] = req[2];
        resp[n++] = req[3];
        resp[n++] = req[4];
        resp[n++] = st;
        n = (uint16)(n + size);
    }
    else
    {
        return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED;
    }
    *respLen = n;
    return 0u;
}

static const Dcm_DidType *Dcm_FindDid(uint16 did)
{
    uint8 i;

    for (i = 0u; i < Dcm_ConfigPtr->numDids; ++i)
    {
        if ((did & Dcm_ConfigPtr->dids[i].mask) == Dcm_ConfigPtr->dids[i].did)
        {
            return &Dcm_ConfigPtr->dids[i];
        }
    }
    return NULL_PTR;
}

static uint8 Dcm_ReadDataByIdentifier(Dcm_OpStatusType op, const uint8 *req, uint16 len, uint8 *resp, uint16 *respLen)
{
    uint16 n = 0u;
    uint16 i;

    (void)op;
    if ((len < 3u) || ((len & 1u) == 0u)) { return DCM_NRC_INCORRECT_LENGTH; }
    for (i = 1u; i < len; i += 2u)
    {
        const uint16 did = (uint16)(((uint16)req[i] << 8) | req[i + 1u]);
        const Dcm_DidType *entry = Dcm_FindDid(did);

        if ((entry == NULL_PTR) || ((entry->sessionMask & (1u << Dcm_ActiveSessionState)) == 0u))
        {
            return DCM_NRC_REQUEST_OUT_OF_RANGE;
        }
        if ((uint16)(n + 2u + entry->length) > *respLen) { return DCM_NRC_RESPONSE_TOO_LONG; }
        resp[n++] = req[i];
        resp[n++] = req[i + 1u];
        (void)entry->read(&resp[n]);
        n = (uint16)(n + entry->length);
    }
    *respLen = n;
    return 0u;
}

static uint8 Dcm_CommunicationControl(Dcm_OpStatusType op, const uint8 *req, uint16 len, uint8 *resp, uint16 *respLen)
{
    const uint8 type = req[2];

    (void)op;
    if (len != 3u) { return DCM_NRC_INCORRECT_LENGTH; }
    if (req[1] > 3u) { return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED; }
    /* communicationType: bits 1..0 normal(1)/NM(2)/both(3); bits 7..4 subnet 0 or 0xF (this network). */
    if (((type & 0x03u) == 0u) || (((type & 0xF0u) != 0u) && ((type & 0xF0u) != 0xF0u)) || ((type & 0x0Cu) != 0u))
    {
        return DCM_NRC_REQUEST_OUT_OF_RANGE;
    }
    (void)ComM_CommunicationControl(COMM_CHANNEL_CAN, req[1], (uint8)(type & 0x03u));
    resp[0] = req[1];
    *respLen = 1u;
    return 0u;
}

static uint8 Dcm_RoutineControl(Dcm_OpStatusType op, const uint8 *req, uint16 len, uint8 *resp, uint16 *respLen)
{
    const uint16 rid = (uint16)(((uint16)req[2] << 8) | req[3]);
    const Dcm_RoutineType *routine = NULL_PTR;
    uint8 i;
    uint8 rc;

    (void)op;
    if (req[1] != 1u) { return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED; }   /* startRoutine only */
    for (i = 0u; i < Dcm_ConfigPtr->numRoutines; ++i)
    {
        if (Dcm_ConfigPtr->routines[i].rid == rid)
        {
            routine = &Dcm_ConfigPtr->routines[i];
        }
    }
    if (routine == NULL_PTR) { return DCM_NRC_REQUEST_OUT_OF_RANGE; }
    if (len != (uint16)(4u + routine->optionLength)) { return DCM_NRC_INCORRECT_LENGTH; }
    rc = routine->start(&req[4]);
    if (rc != 0u) { return rc; }
    resp[0] = 1u;
    resp[1] = req[2];
    resp[2] = req[3];
    *respLen = 3u;
    return 0u;
}

static uint8 Dcm_TesterPresent(Dcm_OpStatusType op, const uint8 *req, uint16 len, uint8 *resp, uint16 *respLen)
{
    (void)op;
    if (len != 2u) { return DCM_NRC_INCORRECT_LENGTH; }
    if (req[1] != 0u) { return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED; }
    resp[0] = 0u;
    *respLen = 1u;
    return 0u;
}

static uint8 Dcm_ControlDtcSetting(Dcm_OpStatusType op, const uint8 *req, uint16 len, uint8 *resp, uint16 *respLen)
{
    (void)op;
    if (len != 2u) { return DCM_NRC_INCORRECT_LENGTH; }
    if ((req[1] != 1u) && (req[1] != 2u)) { return DCM_NRC_SUBFUNCTION_NOT_SUPPORTED; }
    (void)Dem_SetDtcSetting((req[1] == 1u) ? TRUE : FALSE);   /* 1 = on, 2 = off */
    resp[0] = req[1];
    *respLen = 1u;
    return 0u;
}

const Dcm_ServiceType Dcm_DefaultServices[DCM_DEFAULT_SERVICE_COUNT] =
{
    /* sid                                  sub  minLen sessions                     handler */
    { DCM_SID_DIAGNOSTIC_SESSION_CONTROL,   1u,  2u,    DCM_SESSION_MASK_ALL,        Dcm_SessionControl },
    { DCM_SID_ECU_RESET,                    1u,  2u,    DCM_SESSION_MASK_ALL,        Dcm_EcuReset },
    { DCM_SID_CLEAR_DIAGNOSTIC_INFORMATION, 0u,  4u,    DCM_SESSION_MASK_ALL,        Dcm_ClearDiagnosticInformation },
    { DCM_SID_READ_DTC_INFORMATION,         1u,  2u,    DCM_SESSION_MASK_ALL,        Dcm_ReadDtcInformation },
    { DCM_SID_READ_DATA_BY_IDENTIFIER,      0u,  3u,    DCM_SESSION_MASK_ALL,        Dcm_ReadDataByIdentifier },
    { DCM_SID_COMMUNICATION_CONTROL,        1u,  3u,    DCM_SESSION_MASK_EXTENDED,   Dcm_CommunicationControl },
    { DCM_SID_ROUTINE_CONTROL,              1u,  4u,    DCM_SESSION_MASK_EXTENDED,   Dcm_RoutineControl },
    { DCM_SID_TESTER_PRESENT,               1u,  2u,    DCM_SESSION_MASK_ALL,        Dcm_TesterPresent },
    { DCM_SID_CONTROL_DTC_SETTING,          1u,  2u,    DCM_SESSION_MASK_EXTENDED,   Dcm_ControlDtcSetting }
};

/* ---- Initialization and main function --------------------------------------------------------- */
void Dcm_Init(const Dcm_ConfigType *ConfigPtr)
{
    Dcm_ConfigPtr = (ConfigPtr != NULL_PTR) ? ConfigPtr : &Dcm_Config;
    Dcm_State = DCM_ST_IDLE;
    Dcm_RxActive = 0u;
    Dcm_RxLength = 0u;
    Dcm_PostAction = DCM_POST_NONE;
    Dcm_TxIsPending = 0u;
    Dcm_SetSession(DCM_SESSION_DEFAULT);
}

void Dcm_MainFunction(void)
{
    if (Dcm_ConfigPtr == NULL_PTR)
    {
        return;
    }
    DcmAppl_MainFunction();

    switch (Dcm_State)
    {
    case DCM_ST_IDLE:
        /* S3: a non-default session without requests falls back to the default session. */
        if ((Dcm_ActiveSessionState != DCM_SESSION_DEFAULT) && (Dcm_RxActive == 0u) &&
            (Dcm_S3Ticks != 0u) && (--Dcm_S3Ticks == 0u))
        {
            Dcm_SetSession(DCM_SESSION_DEFAULT);
        }
        break;
    case DCM_ST_READY:
        Dcm_Dispatch();
        break;
    case DCM_ST_PROC:
        Dcm_RunHandler(DCM_PENDING);
        if (Dcm_State == DCM_ST_PROC)
        {
            /* Still busy: keep the tester waiting with NRC 0x78 (P2 / P2* handling). */
            if (Dcm_PendingTimerMs <= DCM_MAIN_PERIOD_MS)
            {
                Dcm_PendingTimerMs = DCM_PENDING_RENEW_MS;
                Dcm_SendNrc(DCM_NRC_RESPONSE_PENDING);
            }
            else
            {
                Dcm_PendingTimerMs = (uint16)(Dcm_PendingTimerMs - DCM_MAIN_PERIOD_MS);
            }
        }
        break;
    case DCM_ST_TX_WAIT:
        Dcm_TryTransmit();
        break;
    default:
        break;
    }
}

/* ---- TP interface --------------------------------------------------------------------------------- */
BufReq_ReturnType Dcm_StartOfReception(PduIdType id, const PduInfoType *info, PduLengthType TpSduLength,
                                       PduLengthType *bufferSizePtr)
{
    (void)info;
    if ((id != DCM_RXPDU_DIAG) || (bufferSizePtr == NULL_PTR) || (TpSduLength == 0u))
    {
        return BUFREQ_E_NOT_OK;
    }
    if (Dcm_State != DCM_ST_IDLE)
    {
        return BUFREQ_E_BUSY;               /* a request is being processed: not accepted */
    }
    if (TpSduLength > DCM_RX_BUFFER_LEN)
    {
        return BUFREQ_E_OVFL;
    }
    Dcm_RxActive = 1u;
    Dcm_RxLength = 0u;
    Dcm_RxExpected = TpSduLength;
    *bufferSizePtr = DCM_RX_BUFFER_LEN;
    return BUFREQ_OK;
}

BufReq_ReturnType Dcm_CopyRxData(PduIdType id, const PduInfoType *info, PduLengthType *bufferSizePtr)
{
    if ((id != DCM_RXPDU_DIAG) || (Dcm_RxActive == 0u) || (info == NULL_PTR) || (bufferSizePtr == NULL_PTR) ||
        ((uint16)(Dcm_RxLength + info->SduLength) > Dcm_RxExpected))
    {
        return BUFREQ_E_NOT_OK;
    }
    if (info->SduLength != 0u)
    {
        if (info->SduDataPtr == NULL_PTR)
        {
            return BUFREQ_E_NOT_OK;
        }
        (void)memcpy(&Dcm_RxBuffer[Dcm_RxLength], info->SduDataPtr, info->SduLength);
        Dcm_RxLength = (uint16)(Dcm_RxLength + info->SduLength);
    }
    *bufferSizePtr = (uint16)(Dcm_RxExpected - Dcm_RxLength);
    return BUFREQ_OK;
}

void Dcm_TpRxIndication(PduIdType id, Std_ReturnType result)
{
    if ((id != DCM_RXPDU_DIAG) || (Dcm_RxActive == 0u))
    {
        return;
    }
    Dcm_RxActive = 0u;
    if ((result == E_OK) && (Dcm_RxLength == Dcm_RxExpected) && (Dcm_State == DCM_ST_IDLE))
    {
        Dcm_ReloadS3();
        Dcm_State = DCM_ST_READY;
    }
    else
    {
        Dcm_RxLength = 0u;
    }
}

BufReq_ReturnType Dcm_CopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                 PduLengthType *availableDataPtr)
{
    (void)retry;
    if ((id != DCM_TXPDU_DIAG) || (info == NULL_PTR) || (availableDataPtr == NULL_PTR) ||
        (Dcm_State != DCM_ST_TX_ACTIVE) || ((uint16)(Dcm_TxOffset + info->SduLength) > Dcm_TxLength))
    {
        return BUFREQ_E_NOT_OK;
    }
    if (info->SduLength != 0u)
    {
        if (info->SduDataPtr == NULL_PTR)
        {
            return BUFREQ_E_NOT_OK;
        }
        (void)memcpy(info->SduDataPtr, &Dcm_TxBuffer[Dcm_TxOffset], info->SduLength);
        Dcm_TxOffset = (uint16)(Dcm_TxOffset + info->SduLength);
    }
    *availableDataPtr = (uint16)(Dcm_TxLength - Dcm_TxOffset);
    return BUFREQ_OK;
}

void Dcm_TpTxConfirmation(PduIdType id, Std_ReturnType result)
{
    if ((id != DCM_TXPDU_DIAG) || (Dcm_State != DCM_ST_TX_ACTIVE))
    {
        return;
    }
    if (Dcm_TxIsPending != 0u)
    {
        Dcm_TxIsPending = 0u;
        Dcm_State = DCM_ST_PROC;            /* the pending operation continues */
        return;
    }
    {
        uint8 action = (result == E_OK) ? Dcm_PostAction : (uint8)DCM_POST_NONE;

        Dcm_PostAction = DCM_POST_NONE;
        Dcm_GoIdle();
        if (action != DCM_POST_NONE)
        {
            Dcm_ExecutePostAction(action);   /* reset only after the final response reached the bus */
        }
    }
}
