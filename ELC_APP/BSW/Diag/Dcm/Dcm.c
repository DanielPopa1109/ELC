#include "Dcm.h"
#include "Dem.h"
#include "Nvm.h"
#include "EcuM.h"
#include "SMon.h"
#include "FreeRTOSConfig.h"
#include "../../Com/PduR/PduR.h"
#include "../../Com/Com/Com.h"
#include "../../Com/CanIf/CanIf.h"
#include <string.h>
#include <stdbool.h>
uint8_t Dcm_SWV[4]={33,33,33,0xFF},Dcm_LoadStatus=0xFF,Dcm_CC,Dcm_CDTCS;
uint32_t Dcm_ActiveSessionState __attribute__((section(".ncr.session")));
uint32_t Dcm_LoadTimer,Dcm_SessionCounter,Dcm_MainCounter;
static uint8_t request[256],response[DCM_19_04_MAX_PAYLOAD];
static uint16_t requestLength,responseLength,pendingTimer;
static uint8_t requestReady,txWaiting,txActive,pending,resetAction,resetReady,suppress;
static uint8_t pendingSid;
static void send(uint16_t len){responseLength=len;txWaiting=1;}
static void nrc(uint8_t sid,uint8_t code){response[0]=0x7F;response[1]=sid;response[2]=code;send(3);}
void Dcm_Init(void)
{
    requestReady=txWaiting=txActive=pending=resetAction=resetReady=0;
    Dcm_ActiveSessionState=1;Dcm_LoadStatus=0xFF;Dcm_CC=Dcm_CDTCS=0;Dcm_LoadTimer=0;Dcm_SessionCounter=1000;CanIf_SetCommunicationControl(0,3);
}
void Dcm_RxIndication(const uint8_t *data,uint16_t len,int result)
{
    if(result||!data||!len||len>sizeof request)return;
    if(requestReady||txWaiting||txActive||pending||resetAction)return;
    memcpy(request,data,len);requestLength=len;requestReady=1;Dcm_SessionCounter=1000;
}
void Dcm_TxConfirmation(int result)
{
    txActive=0;
    if(result){resetAction=resetReady=0;txWaiting=0;return;}
    if(resetReady){
        if(resetAction==2)Dcm_ActiveSessionState=2;
        else Dcm_ActiveSessionState=1;
        resetReady=0;resetAction=0;EcuM_PerformReset(0,0);
    }
}
static bool Dcm_PutU8(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint8_t v)
{
    if ((*len + 1u) > maxLen)
    {
        return false;
    }

    buf[*len] = v;
    *len += 1u;
    return true;
}

static bool Dcm_PutU16BE(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint16_t v)
{
    if ((*len + 2u) > maxLen)
    {
        return false;
    }

    buf[*len + 0u] = (uint8_t)(v >> 8);
    buf[*len + 1u] = (uint8_t)(v);
    *len += 2u;
    return true;
}

static bool Dcm_PutU24BE(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint32_t v)
{
    if ((*len + 3u) > maxLen)
    {
        return false;
    }

    buf[*len + 0u] = (uint8_t)(v >> 16);
    buf[*len + 1u] = (uint8_t)(v >> 8);
    buf[*len + 2u] = (uint8_t)(v);
    *len += 3u;
    return true;
}

static bool Dcm_PutFloatRaw(uint8_t *buf, uint16_t *len, uint16_t maxLen, float v)
{
    if ((*len + 4u) > maxLen)
    {
        return false;
    }

    memcpy(&buf[*len], &v, sizeof(float));
    *len += 4u;
    return true;
}

static bool Dcm_PutDidU8(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint16_t did, uint8_t value)
{
    return Dcm_PutU16BE(buf, len, maxLen, did) &&
           Dcm_PutU8(buf, len, maxLen, value);
}

static bool Dcm_PutDidU16(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint16_t did, uint16_t value)
{
    return Dcm_PutU16BE(buf, len, maxLen, did) &&
           Dcm_PutU16BE(buf, len, maxLen, value);
}

static bool Dcm_PutDidFloat(uint8_t *buf, uint16_t *len, uint16_t maxLen, uint16_t did, float value)
{
    return Dcm_PutU16BE(buf, len, maxLen, did) &&
           Dcm_PutFloatRaw(buf, len, maxLen, value);
}

/* Encode one snapshot record in a standards-oriented way:
 *
 *  DTC[3]
 *  statusOfDTC[1]
 *  snapshotRecordNumber[1]
 *  numberOfIdentifiers[1]
 *  DID[2] data...
 *  DID[2] data...
 *  ...
 *
 * Returns false on buffer overflow.
 */
static bool Dcm_EncodeOneSnapshotRecord(uint8_t *payload,
                                        uint16_t *len,
                                        uint16_t maxLen,
                                        uint32_t dtc,
                                        uint8_t status,
                                        uint8_t snapshotRecordNumber,
                                        const Dem_FreezeFrame_t *ff)
{
    const uint8_t numberOfIdentifiers = 12u;

    if ((payload == NULL) || (len == NULL) || (ff == NULL))
    {
        return false;
    }

    if (!Dcm_PutU24BE(payload, len, maxLen, dtc)) return false;
    if (!Dcm_PutU8(payload, len, maxLen, status)) return false;
    if (!Dcm_PutU8(payload, len, maxLen, snapshotRecordNumber)) return false;
    if (!Dcm_PutU8(payload, len, maxLen, numberOfIdentifiers)) return false;

    if (!Dcm_PutDidU16(payload, len, maxLen, DCM_DID_FF_OCCURRENCE_COUNT, ff->occurrenceCnt)) return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_YEAR,             ff->year))          return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_MONTH,            ff->month))         return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_DAY,              ff->day))           return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_HOUR,             ff->hour))          return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_MINUTE,           ff->minute))        return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_SECOND,           ff->second))        return false;
    if (!Dcm_PutDidU8 (payload, len, maxLen, DCM_DID_FF_VEH_STATUS,       ff->vehStatus)) return false;
    if (!Dcm_PutDidU16(payload, len, maxLen, DCM_DID_FF_L1_MV,            ff->L1VFB))            return false;
    if (!Dcm_PutDidU16(payload, len, maxLen, DCM_DID_FF_T30_MV,           ff->T30VFB))           return false;
    if (!Dcm_PutDidFloat(payload, len, maxLen, DCM_DID_FF_NTC_C,          ff->L1NTC))return false;
    if (!Dcm_PutDidFloat(payload, len, maxLen, DCM_DID_FF_ISENSE_A,       ff->L1ISENSE))        return false;

    return true;
}


static int readDid(uint16_t did,uint16_t *length)
{
    uint16_t n=*length;uint32_t value=0;uint8_t count=0;
    if(n+7u>sizeof response)return -1;
    response[n++]=(uint8_t)(did>>8);response[n++]=(uint8_t)did;
    /* Legacy ELC low-byte DIDs retained under F0xx/F1xx; F101 remains reset count.
       FCD's conflicting F101 version query is documented externally. */
    if((did>>8)!=0xF1u && (did>>8)!=0xF0u)return -1;
    switch(did&255u){
    case 1:value=EcuM_TotalResetCounter;count=4;break;
    case 3:value=EcuM_TimeActive;count=4;break;
    case 4:value=EcuM_TimeWithoutReset;count=4;break;
    case 5:response[n++]=EcuM_ResetReason;response[n++]=EcuM_ResetInfo;break;
    case 6:response[n++]=(uint8_t)OS_XCP_CpuLoad;break;
    case 0x1D:response[n++]=SMon_L1ST;break;
    case 0x80:response[n++]=Dcm_SWV[0];response[n++]=Dcm_SWV[1];response[n++]=Nvm_ParamFlashBlock[290];response[n++]=Dcm_SWV[3];break;
    case 0x7E:case 0x86:response[n++]=(uint8_t)Dcm_ActiveSessionState;break;
    case 0x7F:case 0:response[n++]=1;break;
    default:return -1;
    }
    for(unsigned i=0;i<count;++i)response[n++]=(uint8_t)(value>>(8u*i));
    *length=n;return 0;
}
static void readDtc(void)
{
    uint8_t sub=request[1];uint16_t n=0;response[n++]=0x59;response[n++]=sub;
    if(sub==2 || sub==1){
        if(requestLength!=3){nrc(0x19,0x13);return;}
        response[n++]=DEM_STATUS_MASK;uint16_t count=0;
        for(uint8_t i=0;i<DEM_MAX_FF_DTC;++i){uint32_t dtc;uint8_t st;Dem_GetEvent(i,&dtc,&st);
            if(st & request[2] & DEM_STATUS_MASK){count++;if(sub==2){
                response[n++]=(uint8_t)(dtc>>16);response[n++]=(uint8_t)(dtc>>8);response[n++]=(uint8_t)dtc;response[n++]=st;
            }}
        }
        if(sub==1){response[n++]=1;response[n++]=(uint8_t)(count>>8);response[n++]=(uint8_t)count;}
        send(n);return;
    }
    if(sub==4){
        if(requestLength!=6){nrc(0x19,0x13);return;}
        uint32_t wanted=((uint32_t)request[2]<<16)|((uint32_t)request[3]<<8)|request[4];
        if(request[5]!=1 && request[5]!=0xFF){nrc(0x19,0x31);return;}
        for(uint8_t i=0;i<DEM_MAX_FF_DTC;++i){uint32_t dtc;uint8_t st;Dem_FreezeFrame_t ff;Dem_GetEvent(i,&dtc,&st);
            if((wanted==0xFFFFFFu||wanted==dtc) && !Dem_GetSnapshot(dtc,&ff)){
                if(!Dcm_EncodeOneSnapshotRecord(response,&n,sizeof response,dtc,st,1,&ff)){nrc(0x19,0x14);return;}
            }
        }
        if(n==2){nrc(0x19,0x31);return;}send(n);return;
    }
    nrc(0x19,0x12);
}
static void dispatch(void)
{
    uint8_t sid=request[0],sub=requestLength>1?(request[1]&0x7Fu):0;
    suppress=requestLength>1 && (request[1]&0x80u);
    switch(sid){
    case 0x10:
        if(requestLength!=2){nrc(sid,0x13);break;}
        if(sub!=1 && sub!=2 && sub!=3){nrc(sid,0x12);break;}
        if(sub==2){
            uint8_t state=0;
            if(Dcm_ActiveSessionState!=3){nrc(sid,0x7F);break;}
            if(Com_GetVehicleStatus(&state)||state!=6){nrc(sid,0x22);break;}
            if(!Nvm_IsWriteAllowed() || Nvm_WriteAll()){nrc(sid,0x22);break;}
            resetAction=2;pending=2;pendingSid=sid;pendingTimer=900;nrc(sid,0x78);break;
        }
        Dcm_ActiveSessionState=sub;if(sub==1){Dcm_CC=Dcm_CDTCS=0;CanIf_SetCommunicationControl(0,3);Dem_SetDTCSetting(1);Dcm_LoadStatus=0xFF;Dcm_LoadTimer=0;}
        response[0]=0x50;response[1]=sub;response[2]=0;response[3]=50;response[4]=1;response[5]=0xF4;
        if(!suppress)send(6);
        break;
    case 0x3E:
        if(requestLength!=2){nrc(sid,0x13);break;}if(sub){nrc(sid,0x12);break;}
        response[0]=0x7E;response[1]=0;if(!suppress)send(2);break;
    case 0x22:{
        if(requestLength<3 || !(requestLength&1u)){nrc(sid,0x13);break;}
        uint16_t n=1;response[0]=0x62;int bad=0;
        for(unsigned i=1;i<requestLength;i+=2)if(readDid((uint16_t)((request[i]<<8)|request[i+1]),&n)){bad=1;break;}
        if(bad)nrc(sid,0x31);else send(n);break;}
    case 0x19:
        if(requestLength<2){nrc(sid,0x13);break;}readDtc();break;
    case 0x14:{
        if(requestLength!=4){nrc(sid,0x13);break;}
        uint32_t group=((uint32_t)request[1]<<16)|((uint32_t)request[2]<<8)|request[3];
        if(!Nvm_IsWriteAllowed()){nrc(sid,0x22);break;}
        int rc=Dem_ClearDTC(group);if(rc){nrc(sid,rc==-2?0x22:0x31);break;}
        pending=1;pendingSid=sid;pendingTimer=900;nrc(sid,0x78);break;}
    case 0x28:
        if(requestLength!=3){nrc(sid,0x13);break;}
        if(Dcm_ActiveSessionState!=3){nrc(sid,0x7F);break;}
        if(sub>3){nrc(sid,0x12);break;}
        if(request[2]!=1 && request[2]!=3){nrc(sid,0x31);break;}
        Dcm_CC=sub;CanIf_SetCommunicationControl(sub,request[2]);response[0]=0x68;response[1]=sub;if(!suppress)send(2);break;
    case 0x85:
        if(requestLength!=2){nrc(sid,0x13);break;}
        if(Dcm_ActiveSessionState!=3){nrc(sid,0x7F);break;}
        if(sub!=1 && sub!=2){nrc(sid,0x12);break;}
        Dcm_CDTCS=(sub==2);Dem_SetDTCSetting(sub==1);response[0]=0xC5;response[1]=sub;if(!suppress)send(2);break;
    case 0x11:
        if(requestLength!=2){nrc(sid,0x13);break;}
        if(sub!=1){nrc(sid,0x12);break;}
        if(!Nvm_IsWriteAllowed() || Nvm_WriteAll()){nrc(sid,0x22);break;}
        resetAction=1;pending=2;pendingSid=sid;pendingTimer=900;nrc(sid,0x78);break;
    case 0x31:{
        if(requestLength<4){nrc(sid,0x13);break;}
        if(sub!=1){nrc(sid,0x12);break;}
        if(Dcm_ActiveSessionState!=3){nrc(sid,0x7F);break;}
        uint16_t rid=(uint16_t)((request[2]<<8)|request[3]);
        if(rid==0x0040u){if(requestLength!=4){nrc(sid,0x13);break;}SMon_RetryCnt=0;SMon_LockSupply=0;}
        else if(rid==0x0042u){
            if(requestLength!=6){nrc(sid,0x13);break;}
            if(request[4]>1 || !request[5]){nrc(sid,0x31);break;}
            Dcm_LoadStatus=request[4];Dcm_LoadTimer=request[5]*1000u;
        }else{nrc(sid,0x31);break;}
        response[0]=0x71;response[1]=1;response[2]=request[2];response[3]=request[3];if(!suppress)send(4);break;}
    default:nrc(sid,0x11);break;
    }
}
void Dcm_main(void)
{
    Dcm_MainCounter++;
    if(Dcm_LoadTimer && !--Dcm_LoadTimer)Dcm_LoadStatus=0xFF;
    if(!pending && !txActive && !txWaiting && Dcm_SessionCounter && !--Dcm_SessionCounter){
        Dcm_ActiveSessionState=1;Dcm_CC=Dcm_CDTCS=0;CanIf_SetCommunicationControl(0,3);Dem_SetDTCSetting(1);Dcm_LoadStatus=0xFF;Dcm_LoadTimer=0;
    }
    if(pending && !txActive && !txWaiting){
        int state=pending==1?Dem_GetClearResult():(Nvm_GetErrorStatus(0)==NVM_PENDING?1:Nvm_GetErrorStatus(0)==NVM_OK?0:-1);
        if(state<=0){
            uint8_t sid=pendingSid;pending=0;
            if(state<0){resetAction=0;nrc(sid,0x72);}
            else if(sid==0x14){response[0]=0x54;send(1);}
            else {response[0]=sid+0x40;response[1]=(sid==0x10)?2:1;resetReady=1;send(2);}
        } else if(pendingTimer==0){nrc(pendingSid,0x78);pendingTimer=900;}
        else --pendingTimer;
    }
    if(requestReady && !pending && !txWaiting && !txActive){requestReady=0;dispatch();}
    if(txWaiting && !PduR_DcmTransmit(response,responseLength)){txWaiting=0;txActive=1;}
}
