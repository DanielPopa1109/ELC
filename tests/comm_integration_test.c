#include <windows.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "Nvm.h"
#include "Dem.h"
#include "Dcm.h"
#include "SMon.h"
#include "EcuM.h"
#include "../ELC_APP/BSW/Com/Can/Can.h"
#include "../ELC_APP/BSW/Com/CanTp/CanTp.h"
#include "../ELC_APP/BSW/Com/CanIf/CanIf.h"
#include "../ELC_APP/BSW/Com/CanNm/CanNm.h"
#include "../ELC_APP/BSW/Com/CanSM/CanSM.h"
#include "../ELC_APP/BSW/Com/ComM/ComM.h"
#include "../ELC_APP/BSW/Com/Com/Com.h"
#include "../ELC_APP/BSW/Com/PduR/PduR.h"
uint32_t mockPrimask;
static CAN_TypeDef registers;
CAN_HandleTypeDef hcan={&registers};
static uint32_t now,hardwarePending;
static int flashFailure,eraseFailure,readbackFailure,unlocked,resetCount;
static unsigned programCount,eraseCount,canCount;
static uint8_t lastFrame[8],lastLength;
static uint16_t lastId;
static uint8_t uds[1200];static uint16_t udsLength,udsExpected;
static unsigned pendingResponses,positiveClears;
static int autoFc=1,pauseNvm;
float OS_XCP_CpuLoad=23;
uint32_t EcuM_TotalResetCounter=0x12345678,EcuM_TimeActive=1234,EcuM_TimeWithoutReset=234;
uint8_t EcuM_ResetReason=2,EcuM_ResetInfo=3;
void EcuM_PerformReset(uint8_t reason,uint8_t info){(void)reason;(void)info;resetCount++;}
void Dem_AppCapture(Dem_FreezeFrame_t *ff){memset(ff,0,sizeof *ff);ff->year=26;ff->month=10;ff->day=19;ff->L1VFB=12000;ff->T30VFB=12500;ff->L1NTC=24.5f;ff->L1ISENSE=2.25f;}
void CanH_RxIndication(uint16_t id,const uint8_t *data,uint8_t len){(void)id;(void)data;(void)len;}
uint32_t HAL_GetTick(void){return now;}
int HAL_CAN_GetState(CAN_HandleTypeDef *h){(void)h;return HAL_CAN_STATE_LISTENING;}
int HAL_CAN_IsTxMessagePending(CAN_HandleTypeDef *h,uint32_t mailbox){(void)h;return !!(hardwarePending&mailbox);}
int HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *h,uint32_t mailbox){(void)h;hardwarePending&=~mailbox;return 0;}
static void collected(void)
{
    if(udsLength==udsExpected){
        if(udsLength==3 && uds[0]==0x7F && uds[2]==0x78)pendingResponses++;
        if(udsLength==1 && uds[0]==0x54)positiveClears++;
    }
}
int HAL_CAN_AddTxMessage(CAN_HandleTypeDef *h,CAN_TxHeaderTypeDef *header,uint8_t *data,uint32_t *mailbox)
{
    (void)h;
    for(unsigned i=0;i<3;++i)if(!(hardwarePending&(1u<<i))){
        hardwarePending|=1u<<i;*mailbox=1u<<i;lastId=(uint16_t)header->StdId;lastLength=(uint8_t)header->DLC;
        memcpy(lastFrame,data,lastLength);canCount++;
        if(lastId==0x710){
            assert(data[0]==0xF0);uint8_t type=data[1]>>4;
            if(type==0){udsLength=udsExpected=data[1];memcpy(uds,data+2,udsLength);collected();}
            if(type==1){udsExpected=(uint16_t)(((data[1]&15u)<<8)|data[2]);udsLength=5;memcpy(uds,data+3,5);}
            if(type==2){uint16_t count=udsExpected-udsLength;if(count>6)count=6;memcpy(uds+udsLength,data+2,count);udsLength+=count;collected();}
        }
        return HAL_OK;
    }
    return HAL_ERROR;
}
int HAL_FLASH_Unlock(void){unlocked=1;return 0;}
int HAL_FLASH_Lock(void){unlocked=0;return 0;}
int HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *e,uint32_t *error)
{
    (void)error;assert(unlocked && e->PageAddress>=0x0800F800u && e->PageAddress+1024u*e->NbPages<=0x08010000u);
    eraseCount++;if(eraseFailure)return 1;memset((void*)(uintptr_t)e->PageAddress,255,1024u*e->NbPages);return 0;
}
int HAL_FLASH_Program(uint32_t type,uint32_t address,uint64_t value)
{
    (void)type;assert(unlocked && address>=0x0800F800u && address<=0x0800FFFCu && !(address&3u));
    programCount++;if(flashFailure)return 1;
    uint32_t *p=(uint32_t*)(uintptr_t)address;assert((*p&(uint32_t)value)==(uint32_t)value);*p&=(uint32_t)value;
    if(readbackFailure){*p^=1;readbackFailure=0;}return 0;
}
static void completeOne(int success)
{
    for(unsigned i=0;i<3;++i)if(hardwarePending&(1u<<i)){
        hardwarePending&=~(1u<<i);registers.TSR=success?(2u<<(8u*i)):0;Can_MainFunction();break;
    }
}
static void tick(void)
{
    now+=5;completeOne(1);Can_MainFunction();CanIf_MainFunction();
    if(autoFc && udsExpected>6 && udsLength==5){uint8_t fc[8]={0x58,0x30,0,0,0,0,0,0};CanTp_RxIndication(fc,8);}
    CanTp_MainFunction();Dcm_main();Dem_main();if(!pauseNvm && CanTp_IsIdle())Nvm_MainFunction();
}
static void ticks(unsigned n){while(n--)tick();}
static void request(const uint8_t *p,uint8_t len)
{
    uint8_t f[8]={0x58,0,0,0,0,0,0,0};assert(len<=6);f[1]=len;memcpy(f+2,p,len);
    udsLength=udsExpected=0;Can_QueueRx(0x710,f,8);ticks(8);
    for(unsigned n=0;n<300 && !CanTp_IsIdle();++n)tick();
}
static uint8_t requestCrc(uint8_t alive,uint8_t command)
{uint8_t p[3]={alive,command,7};return (uint8_t)Crc_CalculateCRC32(p,3);}
static void networkTick(void){CanNm_MainFunction();ComM_MainFunction();CanSM_MainFunction();Com_MainFunctionRx();Com_MainFunctionTx();}
static uint32_t legacyCrc(const void *data,unsigned bytes)
{
    const uint32_t *p=data;uint32_t crc=0xFFFFFFFFu;
    for(unsigned w=0;w<bytes/4u;++w){crc^=p[w];for(unsigned b=0;b<32;++b)crc=(crc<<1)^((crc&0x80000000u)?0x04C11DB7u:0);}
    return crc;
}
static unsigned appendLegacy(unsigned offset,uint8_t id,const void *data,uint16_t size)
{
    Nvm_Header_t h={0};h.blockId=id;h.blockSize=size;uint32_t crc=legacyCrc(data,size);
    uint8_t *base=(uint8_t*)0x0800F800u;memcpy(base+offset,&h,8);memcpy(base+offset+8,data,size);memcpy(base+offset+8+size,&crc,4);
    return offset+16u+size;
}
int main(void)
{
    void *mapped=VirtualAlloc((void*)0x08000000u,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(mapped==(void*)0x08000000u);
    memset(mapped,255,65536);Can_Init();CanIf_Init();CanTp_Init();PduR_Init();Com_Init();CanNm_Init();CanSM_Init();ComM_Init();Dcm_Init();
    mockPrimask=1;Nvm_ReadAll();Nvm_InitParamFlash();assert(mockPrimask==1 && !programCount && !eraseCount);mockPrimask=0;Dem_Init();
    for(unsigned i=0;i<DEM_MAX_FF_DTC;++i)assert(Dem_DTC_Stat[i]==0x50);
    assert(!Nvm_WriteAll());ticks(110);assert(Nvm_GetErrorStatus(0)==NVM_OK && !unlocked);
    uint8_t parameters[292];memcpy(parameters,Nvm_ParamFlashBlock,292);
    memcpy((void*)0x08003800u,parameters,292);Nvm_InitParamFlash();assert(Nvm_ParameterIntegrity==1);
    uint32_t marker=0x324C4143u;memcpy(parameters+4,&marker,4);uint32_t paramCrc=Crc_CalculateCRC32(parameters+8,284);memcpy(parameters,&paramCrc,4);
    memcpy((void*)0x08003800u,parameters,292);Nvm_InitParamFlash();assert(Nvm_ParameterIntegrity==2);
    ((uint8_t*)0x08003800u)[96]^=1;Nvm_InitParamFlash();assert(Nvm_ParameterIntegrity==0 && ((uint8_t*)0x08003800u)[96]!=parameters[96]);
    unsigned writes=programCount;assert(!Nvm_WriteAll());assert(Nvm_GetErrorStatus(0)==NVM_OK && programCount==writes);
    assert(Nvm_WriteBlock(0,uds)<0 && Nvm_ReadBlock(7,uds)<0);
    uint8_t nm=0x40,command[2]={0xF1,requestCrc(1,1)},value=0;
    CanIf_RxIndication(0x3FF,&nm,0);networkTick();assert(CanSM_GetCurrentComMode()!=CANSM_FULL_COMMUNICATION);
    CanIf_RxIndication(0x3FF,&nm,1);networkTick();assert(CanSM_GetCurrentComMode()==CANSM_FULL_COMMUNICATION);
    CanIf_RxIndication(0x050,command,2);assert(!Com_GetLoadRequest(&value)&&value==1);
    CanIf_RxIndication(0x050,command,2);assert(!Com_GetLoadRequest(&value)); /* ZGW repeats are legal */
    command[1]^=1;CanIf_RxIndication(0x050,command,2);assert(Com_GetLoadRequest(&value)<0);
    command[1]^=1;CanIf_RxIndication(0x050,command,2);
    for(unsigned i=0;i<200;++i)networkTick();assert(Com_LoadRequestTimedOut() && Com_GetLoadRequest(&value)<0);
    assert(CanNm_GetState()==CANNM_TIMED_OUT && CanSM_GetCurrentComMode()!=CANSM_FULL_COMMUNICATION);
    CanIf_RxIndication(0x3FF,&nm,1);networkTick();registers.ESR=CAN_ESR_BOFF;CanSM_MainFunction();assert(CanSM_GetCurrentComMode()!=CANSM_FULL_COMMUNICATION);
    registers.ESR=0;CanSM_MainFunction();assert(CanSM_GetCurrentComMode()==CANSM_FULL_COMMUNICATION);
    nm=0;CanIf_RxIndication(0x3FF,&nm,1);networkTick();assert(CanNm_GetState()==CANNM_NETWORK_RELEASED);
    /* Driver ownership, physical success/error and timeout retries. */
    nm=0x40;CanIf_RxIndication(0x3FF,&nm,1);networkTick();Com_SetLoadStatus(1,1);Com_MainFunctionTx();
    assert(lastId==0x051 && lastLength==1 && lastFrame[0]==1);unsigned count=canCount;
    Com_MainFunctionTx();assert(canCount==count);completeOne(0);Com_MainFunctionTx();assert(canCount==count+1);completeOne(1);
    ticks(10);
    uint8_t telemetry[8]={0};assert(!Can_Write(0x6F6,telemetry,8));assert(Can_Write(0x6F6,telemetry,8)<0);
    now+=101;Can_MainFunction();assert(!hardwarePending);
    assert(!CanIf_TransmitTelemetry(0x6F5,telemetry,8));assert(!CanIf_TransmitTelemetry(0x6F6,telemetry,8));
    uint8_t concurrent[7]={0x62,0xF1,0x80,33,33,33,0xFF};udsLength=udsExpected=0;
    assert(!CanTp_Transmit(concurrent,7));CanTp_MainFunction();assert(hardwarePending==7);ticks(15);assert(CanTp_IsIdle() && udsLength==7);
    const uint8_t ext[]={0x10,3};request(ext,2);assert(udsLength==6 && uds[0]==0x50 && uds[1]==3 && uds[3]==50);
    const uint8_t did[]={0x22,0xF1,1};request(did,3);assert(udsLength==7 && uds[0]==0x62 && uds[3]==0x78 && uds[6]==0x12);
    const uint8_t cc[]={0x28,1,3};request(cc,3);assert(uds[0]==0x68 && Dcm_CC==1);
    uint8_t live[2]={0xF2,requestCrc(2,1)};CanIf_RxIndication(0x050,live,2);assert(!Com_GetLoadRequest(&value));
    count=canCount;Com_SetLoadStatus(0,1);Com_MainFunctionTx();assert(canCount==count);
    const uint8_t ccEnable[]={0x28,0,3};request(ccEnable,3);assert(uds[0]==0x68 && !Dcm_CC);
    assert(!CanIf_TransmitXcp((const uint8_t*)"12345678",8));CanIf_MainFunction();assert(lastId==0x7C1);completeOne(0);CanIf_MainFunction();assert(lastId==0x7C1);completeOne(1);
    const uint8_t badDid[]={0x22,0xF2,1};request(badDid,3);assert(uds[0]==0x7F && uds[2]==0x31);
    const uint8_t bad[]={0x28,0};request(bad,2);assert(uds[0]==0x7F && uds[2]==0x13);
    const uint8_t unknown[]={0x99};request(unknown,1);assert(uds[0]==0x7F && uds[2]==0x11);
    /* Actual transport reassembly: multi-DID request FF + CF. */
    uint8_t ff[8]={0x58,0x10,7,0x22,0xF1,1,0xF1,3},cf[8]={0x58,0x21,0xF1,4,0,0,0,0};
    Can_QueueRx(0x710,ff,8);ticks(3);assert(lastFrame[1]==0x30);Can_QueueRx(0x710,cf,8);ticks(30);assert(udsLength==19 && uds[0]==0x62);
    Can_QueueRx(0x710,ff,8);ticks(3);cf[1]=0x22;udsLength=udsExpected=0;Can_QueueRx(0x710,cf,8);ticks(5);assert(!udsLength);
    /* DEM qualified events, occurrence, snapshots, operation cycles and persistence. */
    Dem_SetDtc(0x50,0x2F);Dem_SetDtc(0x50,0x2F);assert(Dem_GetDtcStatus(0x50)==0x2F && Dem_FF[0].occurrenceCnt==1);
    ticks(110);writes=programCount;Dem_SetDtc(0x50,0x2F);ticks(10);assert(programCount==writes);
    const uint8_t dtcRead[]={0x19,2,0xFF};request(dtcRead,3);assert(uds[0]==0x59 && uds[2]==DEM_STATUS_MASK);
    const uint8_t snapshot[]={0x19,4,0,0,0x50,1};request(snapshot,6);ticks(50);assert(uds[0]==0x59 && uds[1]==4 && udsLength>40);
    assert(uds[2]==0 && uds[3]==0 && uds[4]==0x50 && uds[6]==1 && uds[7]==12);
    Dem_SetDtc(0x50,0x2E);Dem_SetOperationCycle(0);Dem_SetOperationCycle(1);uint32_t dtc;uint8_t status;assert(!Dem_GetEvent(0,&dtc,&status)&& (status&0x40) && !(status&2));
    ticks(110);memset(Dem_DTC_Stat,0,sizeof Dem_DTC_Stat);memset(Dem_FF,0,sizeof Dem_FF);Nvm_ReadAll();Dem_Init();assert(Dem_FF[0].occurrenceCnt==1);
    /* Clear waits for actual NvM completion, then emits exactly one positive response. */
    const uint8_t clear[]={0x14,0xFF,0xFF,0xFF};unsigned cleared=positiveClears;
    SMon_L1ST=1;request(clear,4);assert(uds[0]==0x7F && uds[2]==0x22);SMon_L1ST=0;
    pauseNvm=1;request(clear,4);assert(pendingResponses && positiveClears==cleared);unsigned pendingBefore=pendingResponses;ticks(920);assert(pendingResponses>pendingBefore && Dcm_ActiveSessionState==3);pauseNvm=0;ticks(110);assert(positiveClears==cleared+1);
    assert(Dem_DTC_Stat[0]==0x50 && Dem_FF[0].occurrenceCnt==0);
    Dem_SetDtc(0x50,0x2F);ticks(110);flashFailure=1;request(clear,4);ticks(110);
    assert(uds[0]==0x7F && uds[1]==0x14 && uds[2]==0x72 && positiveClears==cleared+1 && !unlocked);flashFailure=0;
    Nvm_ReadAll();Dem_Init();assert(Dem_FF[0].occurrenceCnt==1); /* failed clear never became durable */
    /* ISO-TP response FC timeout recovers the same real connection. */
    autoFc=0;request(did,3);ticks(210);assert(CanTp_IsIdle());autoFc=1;
    request(ext,2);assert(uds[0]==0x50);ticks(1001);assert(Dcm_ActiveSessionState==1 && !Dcm_CC && !Dcm_CDTCS);
    /* Actual transport BS/STmin, wait frames and timeout handling. */
    autoFc=0;uint8_t payload[20];memset(payload,0xA5,sizeof payload);udsExpected=udsLength=0;
    assert(!CanTp_Transmit(payload,20));ticks(2);uint8_t flow[8]={0x58,0x31,0,0,0,0,0,0};CanTp_RxIndication(flow,8);
    flow[1]=0x30;flow[2]=1;flow[3]=10;CanTp_RxIndication(flow,8);count=canCount;ticks(2);assert(canCount==count);ticks(1);assert(canCount==count+1);ticks(1);count=canCount;ticks(4);assert(canCount==count);
    flow[2]=0;flow[3]=0;CanTp_RxIndication(flow,8);ticks(10);assert(CanTp_IsIdle());autoFc=1;
    uint8_t overflow[8]={0x58,0x11,1,0,0,0,0,0};CanTp_RxIndication(overflow,8);ticks(2);assert(lastFrame[1]==0x32);ticks(3);
    /* Actual legacy record migration: never erase the marker needed for recovery. */
    uint8_t *nv=(uint8_t*)0x0800F800u;memset(nv,255,2048);uint32_t magic=0xB6B6B6B6u;memcpy(nv,&magic,4);memcpy(nv+4,&magic,4);
    Dem_DTC_Stat[0]=0x2F;Dem_FF[0].occurrenceCnt=7;SMon_Battery.SoC_pct=67.5f;
    unsigned end=appendLegacy(8,1,Dem_DTC_Stat,sizeof Dem_DTC_Stat);end=appendLegacy(end,2,Dem_FF,sizeof Dem_FF);end=appendLegacy(end,3,&SMon_Battery,sizeof SMon_Battery);assert(end<1024);
    Nvm_ReadAll();Dem_Init();assert(Dem_FF[0].occurrenceCnt==7 && SMon_Battery.SoC_pct==67.5f);
    assert(!Nvm_WriteAll());Nvm_MainFunction();Nvm_MainFunction(); /* simulate power cut during migration */
    memset(Dem_FF,0,sizeof Dem_FF);Nvm_ReadAll();assert(Dem_FF[0].occurrenceCnt==7);
    assert(!Nvm_WriteAll());for(unsigned i=0;i<110;++i)Nvm_MainFunction();assert(Nvm_GetErrorStatus(0)==NVM_OK);
    memset(Dem_FF,0,sizeof Dem_FF);Nvm_ReadAll();assert(Dem_FF[0].occurrenceCnt==7);
    /* Legacy latest record crossing both pages cannot be migrated destructively. */
    memset(nv,255,2048);memcpy(nv,&magic,4);memcpy(nv+4,&magic,4);
    end=appendLegacy(8,1,Dem_DTC_Stat,sizeof Dem_DTC_Stat);end=appendLegacy(end,2,Dem_FF,sizeof Dem_FF);end=appendLegacy(end,3,&SMon_Battery,sizeof SMon_Battery);end=appendLegacy(end,2,Dem_FF,sizeof Dem_FF);assert(end>1024);
    uint8_t beforeMigration[2048];memcpy(beforeMigration,nv,2048);Nvm_ReadAll();assert(Nvm_WriteAll()<0 && Nvm_GetErrorStatus(0)==NVM_MIGRATION_REQUIRED && !memcmp(beforeMigration,nv,2048));
    assert(!resetCount && !mockPrimask && !unlocked);
    puts("Stack: real Can/CanIf/CanTp/PduR/Com/NM/DCM/DEM/NvM paths and failure assertions passed");
}
