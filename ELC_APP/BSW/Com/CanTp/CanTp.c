#include "CanTp.h"
#include "../CanIf/CanIf.h"
#include "../PduR/PduR.h"
#include <string.h>
enum {IDLE,SEND,FIRST_CONF,WAIT_FC,CF_SEND,CF_CONF,SF_CONF};
static uint8_t rxBuffer[CANTP_RX_MAX],rxSn,rxBlock,rxActive,fcPending,fcInFlight;
static uint16_t rxLength,rxExpected,rxTimer;
static const uint8_t *txData;
static uint16_t txLength,txOffset,txTimer;
static uint8_t txState,txSn,txBlock,txBs,txWait,txSt,txDelay;
static uint8_t earlyFc[3],earlyFcValid;
static void rxAbort(void){rxActive=0;fcPending=0;rxTimer=0;PduR_CanTpRxIndication(0,0,-1);}
static void txDone(int result){txState=IDLE;txData=0;earlyFcValid=0;PduR_CanTpTxConfirmation(result);}
void CanTp_Init(void){rxActive=fcPending=fcInFlight=earlyFcValid=0;txState=IDLE;txData=0;}
int CanTp_IsIdle(void){return txState==IDLE && !rxActive && !fcInFlight && !fcPending;}
void CanTp_Cancel(void){if(rxActive)rxAbort();if(txState!=IDLE)txDone(-1);}
int CanTp_Transmit(const uint8_t *data,uint16_t len)
{
    if(!data||!len||len>4095u||txState!=IDLE||fcInFlight||fcPending)return -1;
    txData=data;txLength=len;txOffset=0;txState=SEND;txTimer=200;txSn=1;txWait=0;return 0;
}
static void flow(const uint8_t *d)
{
    if(d[0]==0x30u){
        if(d[2]>0x7Fu && (d[2]<0xF1u||d[2]>0xF9u)){txDone(-1);return;}
        txBs=d[1];txSt=(d[2]>=0xF1u)?1u:(uint8_t)((d[2]+4u)/5u);
        txBlock=0;txDelay=txSt;txState=CF_SEND;txTimer=200;
    }else if(d[0]==0x31u && ++txWait<=3u)txTimer=200;
    else txDone(-1);
}
void CanTp_RxIndication(const uint8_t *d,uint8_t len)
{
    if(!d||len<2u||len>8u||d[0]!=CANTP_RX_ADDRESS)return;
    uint8_t type=d[1]>>4,n=d[1]&15u;
    if(type==3){
        if(len<4u)return;
        if(txState==WAIT_FC)flow(d+1);
        else if(txState==FIRST_CONF || (txState==CF_CONF && txBs && txBlock+1u>=txBs)){
            memcpy(earlyFc,d+1,3);earlyFcValid=1;
        }
        return;
    }
    if(txState!=IDLE)return;
    if(type==0){
        if(!n||n>6u||len<2u+n)return;
        if(rxActive)rxAbort();
        PduR_CanTpRxIndication(d+2,n,0);return;
    }
    if(type==1){
        if(len!=8u)return;
        uint16_t total=(uint16_t)((n<<8)|d[2]);
        if(rxActive)rxAbort();
        if(total<=6u||total>CANTP_RX_MAX){fcPending=2;return;}
        rxExpected=total;rxLength=5;memcpy(rxBuffer,d+3,5);rxSn=1;rxBlock=0;
        rxActive=1;rxTimer=200;fcPending=1;return;
    }
    if(type==2 && rxActive){
        if(fcPending||fcInFlight||n!=rxSn||len<3u){rxAbort();return;}
        uint16_t remain=rxExpected-rxLength;uint8_t count=remain>6u?6u:(uint8_t)remain;
        if(len<2u+count){rxAbort();return;}
        memcpy(rxBuffer+rxLength,d+2,count);rxLength+=count;rxSn=(rxSn+1u)&15u;rxTimer=200;
        if(rxLength==rxExpected){rxActive=0;PduR_CanTpRxIndication(rxBuffer,rxLength,0);}
        else if(++rxBlock==8u){rxBlock=0;fcPending=1;}
    }
}
void CanTp_TxConfirmation(int result)
{
    if(fcInFlight){fcInFlight=0;if(result){rxAbort();return;}rxTimer=200;return;}
    if(txState!=FIRST_CONF && txState!=CF_CONF && txState!=SF_CONF)return;
    if(result){txDone(-1);return;}
    if(txState==SF_CONF){txDone(0);return;}
    if(txState==FIRST_CONF){txOffset=5;txState=WAIT_FC;}
    else {
        uint16_t remain=txLength-txOffset;txOffset+=remain>6u?6u:remain;txSn=(txSn+1u)&15u;
        if(txOffset==txLength){txDone(0);return;}
        txBlock++;txDelay=txSt;
        txState=(txBs && txBlock>=txBs)?WAIT_FC:CF_SEND;
    }
    txTimer=200;
    if(txState==WAIT_FC && earlyFcValid){earlyFcValid=0;flow(earlyFc);}
}
void CanTp_MainFunction(void)
{
    uint8_t f[8]={CANTP_TX_ADDRESS,0,0,0,0,0,0,0};
    if(rxActive && rxTimer && !--rxTimer)rxAbort();
    if(txState!=IDLE && txTimer && !--txTimer){txDone(-1);return;}
    if(fcPending && !fcInFlight){
        f[1]=(fcPending==1u)?0x30u:0x32u;f[2]=8;f[3]=5;
        if(!CanIf_TransmitDiagnostic(f,8)){fcPending=0;fcInFlight=1;}return;
    }
    if(fcInFlight)return;
    if(txState==SEND){
        if(txLength<=6u){f[1]=(uint8_t)txLength;memcpy(f+2,txData,txLength);}
        else {f[1]=0x10u|(uint8_t)(txLength>>8);f[2]=(uint8_t)txLength;memcpy(f+3,txData,5);}
        if(!CanIf_TransmitDiagnostic(f,8)){txState=(txLength<=6u)?SF_CONF:FIRST_CONF;txTimer=200;}
    } else if(txState==CF_SEND){
        if(txDelay){txDelay--;return;}
        uint16_t remain=txLength-txOffset;uint8_t count=remain>6u?6u:(uint8_t)remain;
        f[1]=0x20u|txSn;memcpy(f+2,txData+txOffset,count);
        if(!CanIf_TransmitDiagnostic(f,8)){txState=CF_CONF;txTimer=200;}
    }
}
