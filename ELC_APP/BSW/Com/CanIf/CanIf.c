#include "CanIf.h"
#include "../Can/Can.h"
#include "../PduR/PduR.h"
#include "../CanNm/CanNm.h"
#include "../CanTp/CanTp.h"
#include <string.h>

static uint8_t initialized;
static uint8_t applicationTxEnabled;
static uint8_t normalRxEnabled=1,normalTxEnabled=1,nmRxEnabled=1;
static uint8_t xcpData[8],xcpLength,xcpPending,xcpInFlight;

void CanIf_Init(void) { applicationTxEnabled = 0u; initialized = 1u;normalRxEnabled=normalTxEnabled=nmRxEnabled=1;xcpPending=xcpInFlight=0; }
void CanIf_SetCommunicationControl(uint8_t control,uint8_t communicationType)
{
    normalRxEnabled=control<2u;normalTxEnabled=(control&1u)==0;
    nmRxEnabled=communicationType==3u?normalRxEnabled:1u;
}
void CanIf_MainFunction(void)
{
    if(initialized && applicationTxEnabled && normalTxEnabled && xcpPending && !xcpInFlight &&
       !Can_Write(0x7C1u,xcpData,xcpLength))xcpInFlight=1;
}
void CanIf_SetApplicationTxEnabled(uint8_t enabled)
{
    applicationTxEnabled = enabled ? 1u : 0u;
}

int CanIf_Transmit(uint8_t txPduId, const uint8_t *data, uint8_t len)
{
    if ((initialized == 0u) || (applicationTxEnabled == 0u) || !normalTxEnabled ||
        (txPduId != CANIF_TX_LOADSTATUS) || (len != 1u)) return -1;
    return Can_Write(0x051u, data, len);
}

int CanIf_TransmitTelemetry(uint16_t canId, const uint8_t *data, uint8_t len)
{
    uint8_t expectedLen;
    if ((initialized == 0u) || (applicationTxEnabled == 0u) || !normalTxEnabled ||
        (data == 0) || (canId < 0x6EEu) || (canId > 0x6F7u)) return -1;
    expectedLen = (canId == 0x6F0u) ? 4u :
                  (canId == 0x6F7u) ? 1u : 8u;
    if (len != expectedLen) return -1;
    return Can_Write(canId, data, len);
}

void CanIf_RxIndication(uint16_t canId, const uint8_t *data, uint8_t len)
{
    if ((initialized == 0u) || (data == 0) || (len > 8u)) return;
    if(!normalRxEnabled && (canId==0x050u || canId==0x3A0u))return;
    if (canId == CANTP_RX_ID)
        CanTp_RxIndication(data,len);
    else if (canId == 0x050u)
        PduR_CanIfRxIndication(CANIF_RX_LOADREQUEST, data, len);
    else if (canId == 0x3A0u)
        PduR_CanIfRxIndication(CANIF_RX_VEHICLESTATE, data, len);
    else if (canId == 0x3FFu && nmRxEnabled)
        CanNm_RxIndication(data, len);
}

int CanIf_TransmitDiagnostic(const uint8_t *data,uint8_t len)
{return initialized?Can_Write(CANTP_TX_ID,data,len):-1;}
int CanIf_TransmitFault(const uint8_t *data)
{return initialized && applicationTxEnabled && normalTxEnabled?Can_Write(0x7FEu,data,4):-1;}
int CanIf_TransmitXcp(const uint8_t *data,uint8_t len)
{
    if(!initialized||!applicationTxEnabled||!normalTxEnabled||!data||len>8||xcpPending)return -1;
    memcpy(xcpData,data,len);xcpLength=len;xcpPending=1;return 0;
}
void CanIf_TxConfirmation(uint16_t id,int result)
{
    if(id==CANTP_TX_ID)CanTp_TxConfirmation(result);
    else if(id==0x7C1u){xcpInFlight=0;if(!result)xcpPending=0;}
    else PduR_CanIfTxConfirmation(id,result);
}
