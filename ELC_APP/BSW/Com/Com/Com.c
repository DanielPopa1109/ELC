#include "Com.h"
#include "main.h"
#include "../PduR/PduR.h"
#include "../../Sys/Crc/Crc.h"
#include <string.h>

/* LoadRequest 0x050: byte0 = alive[3:0] | dataId[6:4] | request[7], byte1 = low byte of the
 * reflected CRC32 over { alive, request, dataId } (ZGW GatewaySwc / DBC). */
#define COM_LOADREQUEST_DATAID   7u

/* Per TX I-PDU payload length, index == COM_TXPDU_*. */
static const uint8 Com_TxLength[COM_NUM_TX_PDUS] = { 1u, 8u, 8u, 4u, 8u, 8u, 8u, 8u, 8u, 8u, 1u, 4u };

typedef struct
{
    uint8  data[8];
    uint8  pending;
    uint8  inFlight;
    uint8  timer;        /* TX confirmation supervision */
    uint32 revision;     /* bumped on every new value */
    uint32 submitted;    /* revision handed to the lower layer */
} Com_TxPduStateType;

static Com_TxPduStateType Com_TxPdu[COM_NUM_TX_PDUS];
static uint16 Com_LoadStatusAge;
static boolean Com_RxAllowed;
static boolean Com_TxAllowed;

static volatile uint8  Com_RequestValue;
static volatile uint8  Com_RequestValid;
static volatile uint16 Com_RequestAge;
static volatile uint32 Com_InvalidStreak;
static volatile uint8  Com_VehicleStatus;
static volatile uint16 Com_VehicleAge;
static volatile uint8  Com_VehicleInvalid;

void Com_Init(void)
{
    (void)memset(Com_TxPdu, 0, sizeof(Com_TxPdu));
    Com_TxPdu[COM_TXPDU_LOADSTATUS].pending = 1u;
    Com_LoadStatusAge = COM_LOADSTATUS_PERIOD;
    Com_RxAllowed = TRUE;
    Com_TxAllowed = FALSE;
    Com_RequestValue = 0u;
    Com_RequestValid = 0u;
    Com_RequestAge = COM_RX_DEADLINE_TICKS;
    Com_InvalidStreak = 0u;
    Com_VehicleStatus = 0u;
    Com_VehicleAge = COM_RX_DEADLINE_TICKS;
    Com_VehicleInvalid = 0u;
}

void Com_SetRxAllowed(boolean Allowed)
{
    Com_RxAllowed = (Allowed != 0u) ? TRUE : FALSE;
}

void Com_SetTxAllowed(boolean Allowed)
{
    if ((Allowed != 0u) && (Com_TxAllowed == FALSE))
    {
        Com_TxPdu[COM_TXPDU_LOADSTATUS].pending = 1u;   /* announce the state when TX is (re)enabled */
    }
    Com_TxAllowed = (Allowed != 0u) ? TRUE : FALSE;
}

/* ---- Reception --------------------------------------------------------------- */
void Com_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    const uint8 *data;
    uint8 len;
    uint8 crcData[3];

    if (Com_RxAllowed == FALSE)
    {
        return;
    }
    data = (PduInfoPtr != NULL_PTR) ? PduInfoPtr->SduDataPtr : NULL_PTR;
    len = (PduInfoPtr != NULL_PTR) ? (uint8)PduInfoPtr->SduLength : 0u;

    if (RxPduId == COM_RXPDU_VEHICLESTATE)
    {
        if ((data != NULL_PTR) && (len == 1u) && ((data[0] & 0xF0u) == 0u) && ((data[0] & 0x0Fu) <= 6u))
        {
            Com_VehicleStatus = data[0] & 0x0Fu;
            Com_VehicleAge = 0u;
            Com_VehicleInvalid = 0u;
        }
        else
        {
            Com_VehicleInvalid = 1u;
            Com_VehicleStatus = 0u;
        }
        return;
    }
    if (RxPduId != COM_RXPDU_LOADREQUEST)
    {
        return;
    }

    if ((data == NULL_PTR) || (len != 2u))
    {
        Com_RequestValid = 0u;
        if (Com_InvalidStreak != UINT32_MAX) { ++Com_InvalidStreak; }
        return;
    }
    crcData[0] = data[0] & 0x0Fu;            /* alive */
    crcData[1] = (data[0] >> 7u) & 1u;       /* request */
    crcData[2] = (data[0] >> 4u) & 7u;       /* data id */
    if ((crcData[2] != COM_LOADREQUEST_DATAID) ||
        ((uint8)Crc_CalculateCRC32(crcData, 3u, 0u, TRUE) != data[1]))
    {
        Com_RequestValid = 0u;
        if (Com_InvalidStreak != UINT32_MAX) { ++Com_InvalidStreak; }
        return;
    }
    /* The alive nibble is protected by the CRC but not required to advance: ZGW increments it
     * while generating signals and COM repeats the PDU independently, so identical frames are legal. */
    Com_RequestValue = crcData[1];
    Com_RequestAge = 0u;
    Com_RequestValid = 1u;
    Com_InvalidStreak = 0u;
}

void Com_MainFunctionRx(void)
{
    if (Com_RequestAge < COM_RX_DEADLINE_TICKS) { ++Com_RequestAge; }
    if (Com_VehicleAge < COM_RX_DEADLINE_TICKS) { ++Com_VehicleAge; }
}

Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalDataPtr)
{
    if (SignalDataPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }
    if (SignalId == COM_SIGNAL_LOADREQUEST)
    {
        if ((Com_RequestValid == 0u) || (Com_RequestAge >= COM_RX_DEADLINE_TICKS)) { return E_NOT_OK; }
        *(uint8 *)SignalDataPtr = Com_RequestValue;
        return E_OK;
    }
    if (SignalId == COM_SIGNAL_VEHICLESTATE)
    {
        if ((Com_VehicleInvalid != 0u) || (Com_VehicleAge >= COM_RX_DEADLINE_TICKS)) { return E_NOT_OK; }
        *(uint8 *)SignalDataPtr = Com_VehicleStatus;
        return E_OK;
    }
    return E_NOT_OK;
}

uint8 Com_GetRxStatus(Com_SignalIdType SignalId)
{
    if (SignalId == COM_SIGNAL_LOADREQUEST)
    {
        return (Com_RequestAge >= COM_RX_DEADLINE_TICKS) ? COM_RXSTATUS_TIMEOUT : 0u;
    }
    if (SignalId == COM_SIGNAL_VEHICLESTATE)
    {
        return (uint8)(((Com_VehicleAge >= COM_RX_DEADLINE_TICKS) ? COM_RXSTATUS_TIMEOUT : 0u) |
                       ((Com_VehicleInvalid != 0u) ? COM_RXSTATUS_INVALID : 0u));
    }
    return COM_RXSTATUS_TIMEOUT;
}

uint32 Com_GetE2eErrorCount(void)
{
    return Com_InvalidStreak;
}

/* ---- Transmission ------------------------------------------------------------ */
Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void *SignalDataPtr)
{
    Com_TxPduStateType *pdu;
    uint32 primask;

    if ((SignalDataPtr == NULL_PTR) || (SignalId >= COM_NUM_TX_PDUS))
    {
        return E_NOT_OK;
    }
    pdu = &Com_TxPdu[SignalId];
    primask = __get_PRIMASK();
    __disable_irq();
    if (SignalId == COM_SIGNAL_LOADSTATUS)
    {
        uint8 status = *(const uint8 *)SignalDataPtr & 1u;
        if (status != pdu->data[0])
        {
            pdu->pending = 1u;
            pdu->revision++;
        }
        pdu->data[0] = status;
    }
    else if (SignalId == COM_SIGNAL_DTC_BROADCAST)
    {
        if ((pdu->pending != 0u) || (pdu->inFlight != 0u))
        {
            __set_PRIMASK(primask);
            return E_NOT_OK;                 /* one broadcast at a time; Dem retries */
        }
        (void)memcpy(pdu->data, SignalDataPtr, Com_TxLength[SignalId]);
        pdu->pending = 1u;
    }
    else
    {
        (void)memcpy(pdu->data, SignalDataPtr, Com_TxLength[SignalId]);
        pdu->revision++;
        pdu->pending = 1u;
    }
    __set_PRIMASK(primask);
    return E_OK;
}

void Com_MainFunctionTx(void)
{
    uint8 i;

    if (Com_LoadStatusAge < COM_LOADSTATUS_PERIOD) { ++Com_LoadStatusAge; }
    if (Com_TxAllowed == FALSE)
    {
        for (i = COM_TXPDU_TELEMETRY_FIRST; i < (COM_TXPDU_TELEMETRY_FIRST + COM_TXPDU_TELEMETRY_COUNT); ++i)
        {
            Com_TxPdu[i].pending = 0u;
        }
        return;
    }
    if (Com_LoadStatusAge >= COM_LOADSTATUS_PERIOD)
    {
        Com_TxPdu[COM_TXPDU_LOADSTATUS].pending = 1u;
    }

    for (i = 0u; i < COM_NUM_TX_PDUS; ++i)
    {
        Com_TxPduStateType *pdu = &Com_TxPdu[i];
        uint32 primask = __get_PRIMASK();

        __disable_irq();
        if (pdu->inFlight != 0u)
        {
            if ((pdu->timer != 0u) && (--pdu->timer == 0u))
            {
                pdu->inFlight = 0u;          /* confirmation lost: stay pending, retry */
            }
        }
        else if (pdu->pending != 0u)
        {
            PduInfoType info;

            info.SduDataPtr = pdu->data;
            info.MetaDataPtr = NULL_PTR;
            info.SduLength = Com_TxLength[i];
            if (PduR_ComTransmit(i, &info) == E_OK)
            {
                pdu->inFlight = 1u;
                pdu->submitted = pdu->revision;
                pdu->timer = COM_TX_TIMEOUT_TICKS;
            }
        }
        __set_PRIMASK(primask);
    }
}

void Com_TxConfirmation(PduIdType TxPduId)
{
    Com_TxPduStateType *pdu;
    uint32 primask;

    if (TxPduId >= COM_NUM_TX_PDUS)
    {
        return;
    }
    pdu = &Com_TxPdu[TxPduId];
    primask = __get_PRIMASK();
    __disable_irq();
    if (pdu->inFlight != 0u)
    {
        pdu->inFlight = 0u;
        /* A newer value queued while this one was on the bus stays pending. */
        if (pdu->submitted == pdu->revision)
        {
            pdu->pending = 0u;
        }
        if (TxPduId == COM_TXPDU_LOADSTATUS)
        {
            Com_LoadStatusAge = 0u;
        }
    }
    __set_PRIMASK(primask);
}
