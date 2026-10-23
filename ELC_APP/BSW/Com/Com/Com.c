#include "Com.h"
#include "main.h"
#include <string.h>
#include "../PduR/PduR.h"

/* ELC subset of ZGW COM config: 0x050/DLC2 RX, 0x051/DLC1 TX. */
#define COM_LOADREQUEST_TIMEOUT_TICKS 200u
#define COM_LOADSTATUS_PERIOD_TICKS   100u
#define COM_TELEMETRY_FIRST_ID 0x6EEu
#define COM_TELEMETRY_LAST_ID  0x6F7u
#define COM_TELEMETRY_COUNT    10u

typedef struct {
    uint8_t data[8];
    uint8_t pending;
    uint8_t inFlight;
    uint32_t revision,submitted;
} Com_TelemetryPduType;

static volatile uint8_t requestValue;
static volatile uint8_t requestValid;
static volatile uint16_t requestAge;
static volatile uint32_t invalidStreak;
static volatile uint8_t vehicleStatus;
static volatile uint16_t vehicleAge;
static volatile uint8_t invalidVehicleStatus;
static uint8_t statusInFlight;
static uint32_t statusRevision,statusSubmitted;
static uint8_t faultData[4],faultPending,faultInFlight;
static uint8_t statusValue;
static uint8_t statusEnabled;
static uint8_t statusPending;
static uint16_t statusAge;
static Com_TelemetryPduType telemetry[COM_TELEMETRY_COUNT];

void Com_Init(void)
{
    statusInFlight=faultPending=faultInFlight=0;statusRevision=statusSubmitted=0;
    requestValue = 0u;
    requestValid = 0u;
    requestAge = COM_LOADREQUEST_TIMEOUT_TICKS;
    invalidStreak = 0u;
    vehicleStatus = 0u;
    vehicleAge = COM_LOADREQUEST_TIMEOUT_TICKS;
    invalidVehicleStatus = 0u;
    statusValue = 0u;
    statusEnabled = 0u;
    statusPending = 1u;
    statusAge = COM_LOADSTATUS_PERIOD_TICKS;
    for (uint8_t i = 0u; i < COM_TELEMETRY_COUNT; ++i)
        memset(&telemetry[i],0,sizeof telemetry[i]);
}

void Com_RxIndication(uint8_t rxPduId, const uint8_t *data, uint8_t len)
{
    uint8_t alive, dataId, request;
    uint8_t crcData[3];
    uint32_t crc = 0xFFFFFFFFu;
    if (rxPduId == COM_RX_VEHICLESTATE)
    {
        if ((data != 0) && (len == 1u) && ((data[0] & 0xF0u) == 0u) &&
            ((data[0] & 0x0Fu) <= 6u))
        {
            vehicleStatus = data[0] & 0x0Fu;
            vehicleAge = 0u;
            invalidVehicleStatus = 0u;
        }
        else
        {
            invalidVehicleStatus = 1u;
            vehicleStatus = 0u;
        }
        return;
    }
    if ((rxPduId != COM_RX_LOADREQUEST) || (data == 0) || (len != 2u))
    {
        if (rxPduId == COM_RX_LOADREQUEST) requestValid = 0u;
        if (invalidStreak != UINT32_MAX) ++invalidStreak;
        return;
    }
    alive = data[0] & 0x0Fu;
    dataId = (data[0] >> 4u) & 7u;
    request = (data[0] >> 7u) & 1u;
    crcData[0] = alive;
    crcData[1] = request;
    crcData[2] = dataId;
    for (uint8_t i = 0u; i < 3u; ++i)
    {
        crc ^= crcData[i];
        for (uint8_t bit = 0u; bit < 8u; ++bit)
            crc = (crc >> 1u) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    if ((dataId != 7u) || ((uint8_t)~crc != data[1]))
    {
        requestValid = 0u;
        if (invalidStreak != UINT32_MAX) ++invalidStreak;
        return;
    }
    requestValue = request;
    requestAge = 0u;
    requestValid = 1u;
    invalidStreak = 0u;
}

void Com_MainFunctionRx(void)
{
    if (requestAge < COM_LOADREQUEST_TIMEOUT_TICKS) ++requestAge;
    if (vehicleAge < COM_LOADREQUEST_TIMEOUT_TICKS) ++vehicleAge;
}

void Com_SetLoadStatus(uint8_t status, uint8_t txEnabled)
{
    status &= 1u;
    if (status != statusValue) {statusPending = 1u;statusRevision++;}
    if ((txEnabled != 0u) && (statusEnabled == 0u)) statusPending = 1u;
    statusValue = status;
    statusEnabled = (txEnabled != 0u) ? 1u : 0u;
}

int Com_QueueTelemetry(uint16_t canId, const uint8_t *data, uint8_t len)
{
    uint8_t index;
    uint8_t expectedLen;
    if ((data == 0) || (canId < COM_TELEMETRY_FIRST_ID) ||
        (canId > COM_TELEMETRY_LAST_ID)) return -1;
    expectedLen = (canId == 0x6F0u) ? 4u :
                  (canId == 0x6F7u) ? 1u : 8u;
    if (len != expectedLen) return -1;
    index = (uint8_t)(canId - COM_TELEMETRY_FIRST_ID);
    uint32_t mask=__get_PRIMASK();__disable_irq();
    memcpy(telemetry[index].data,data,len);telemetry[index].revision++;
    telemetry[index].pending = 1u;__set_PRIMASK(mask);
    return 0;
}

void Com_MainFunctionTx(void)
{
	uint8_t i;
    if (statusAge < COM_LOADSTATUS_PERIOD_TICKS) ++statusAge;
	if (statusEnabled == 0u)
	{
		for (i = 0u; i < COM_TELEMETRY_COUNT; ++i)
			telemetry[i].pending = 0u;
		return;
	}
    if ((statusEnabled != 0u) && !statusInFlight &&
        ((statusPending != 0u) || (statusAge >= COM_LOADSTATUS_PERIOD_TICKS)))
    {
        if (PduR_ComTransmit(PDUR_TX_LOADSTATUS, &statusValue, 1u) == 0)
        {
            statusInFlight=1;statusSubmitted=statusRevision;
        }
    }
    if(faultPending && !faultInFlight && !PduR_ComTransmitFault(faultData))faultInFlight=1;
	for (i = 0u; i < COM_TELEMETRY_COUNT; ++i)
	{
		if (telemetry[i].pending != 0u && !telemetry[i].inFlight)
		{
			uint16_t id = (uint16_t)(COM_TELEMETRY_FIRST_ID + i);
			uint8_t len = (id == 0x6F0u) ? 4u :
							  (id == 0x6F7u) ? 1u : 8u;
			uint32_t mask=__get_PRIMASK();__disable_irq();
            if (PduR_ComTransmitTelemetry(id, telemetry[i].data, len) == 0) {
                telemetry[i].inFlight=1;telemetry[i].submitted=telemetry[i].revision;
            }
            __set_PRIMASK(mask);
		}
	}
}

int Com_GetLoadRequest(uint8_t *request)
{
    if ((request == 0) || (requestValid == 0u) ||
        (requestAge >= COM_LOADREQUEST_TIMEOUT_TICKS)) return -1;
    *request = requestValue;
    return 0;
}

uint32_t Com_GetInvalidFrameCount(void) { return invalidStreak; }

uint8_t Com_LoadRequestTimedOut(void)
{
    return (requestAge >= COM_LOADREQUEST_TIMEOUT_TICKS) ? 1u : 0u;
}

uint8_t Com_VehicleStatusTimedOut(void)
{
    return (vehicleAge >= COM_LOADREQUEST_TIMEOUT_TICKS) ? 1u : 0u;
}

int Com_GetVehicleStatus(uint8_t *status)
{
    if ((status == 0) || (invalidVehicleStatus != 0u) ||
        (vehicleAge >= COM_LOADREQUEST_TIMEOUT_TICKS)) return -1;
    *status = vehicleStatus;
    return 0;
}

uint8_t Com_HasInvalidVehicleStatus(void)
{
    return invalidVehicleStatus;
}

int Com_QueueFault(const uint8_t *data)
{
    if(!data||faultPending||faultInFlight)return -1;
    memcpy(faultData,data,4);faultPending=1;return 0;
}
void Com_TxConfirmation(uint16_t id,int result)
{
    uint32_t mask=__get_PRIMASK();__disable_irq();
    if(id==0x051u){statusInFlight=0;if(!result){statusAge=0;if(statusSubmitted==statusRevision)statusPending=0;}else statusPending=1;}
    else if(id==0x7FEu){faultInFlight=0;if(!result)faultPending=0;}
    else if(id>=0x6EEu && id<=0x6F7u){Com_TelemetryPduType *p=&telemetry[id-0x6EEu];p->inFlight=0;if(!result && p->submitted==p->revision)p->pending=0;}
    __set_PRIMASK(mask);
}
