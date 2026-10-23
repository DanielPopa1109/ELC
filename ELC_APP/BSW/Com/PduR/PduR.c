#include "PduR.h"
#include "../CanIf/CanIf.h"
#include "../Com/Com.h"
#include "../CanTp/CanTp.h"
#include "../../Diag/Dcm/Dcm.h"

static uint8_t initialized;

int PduR_DcmTransmit(const uint8_t *data,uint16_t len){return initialized?CanTp_Transmit(data,len):-1;}
void PduR_CanTpRxIndication(const uint8_t *data,uint16_t len,int result){if(initialized)Dcm_RxIndication(data,len,result);}
void PduR_CanTpTxConfirmation(int result){if(initialized)Dcm_TxConfirmation(result);}
void PduR_CanIfTxConfirmation(uint16_t id,int result){if(initialized)Com_TxConfirmation(id,result);}
int PduR_ComTransmitFault(const uint8_t *data){return initialized?CanIf_TransmitFault(data):-1;}

void PduR_Init(void) { initialized = 1u; }

int PduR_ComTransmit(uint8_t txPduId, const uint8_t *data, uint8_t len)
{
    if ((initialized == 0u) || (txPduId != PDUR_TX_LOADSTATUS)) return -1;
    return CanIf_Transmit(CANIF_TX_LOADSTATUS, data, len);
}

int PduR_ComTransmitTelemetry(uint16_t canId, const uint8_t *data, uint8_t len)
{
    if (initialized == 0u) return -1;
    return CanIf_TransmitTelemetry(canId, data, len);
}

void PduR_CanIfRxIndication(uint8_t rxPduId, const uint8_t *data, uint8_t len)
{
    if (initialized == 0u) return;
    if (rxPduId == PDUR_RX_LOADREQUEST)
        Com_RxIndication(COM_RX_LOADREQUEST, data, len);
    else if (rxPduId == PDUR_RX_VEHICLESTATE)
        Com_RxIndication(COM_RX_VEHICLESTATE, data, len);
}
