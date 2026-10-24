/* ELC programming protocol, instantiated by the resident FBL and by the SRAM updater.
 *
 * Layers (no RTE; no HAL, libc, function pointers or external data dependencies, so that every
 * function can be placed into the SRAM blob via BOOT_CODE):
 *   BootTp_*   ISO 15765-2 transport, classic CAN, extended addressing, physical request/response
 *   BootDcm_*  ISO 14229-1 services 10 11 22 28 31 34 36 37 3E 85 and the programming state machine
 *   BootPort_* hardware ports provided by the including file (CAN send, Flash program/erase,
 *              CRC, finalize, reset, updater hand-over)
 *
 * The service set and the routine response format follow what FCD sends to a node in front of and
 * during a flash (10 03, 28 01 03, 85 02, 10 02, 31 01 0200/0155/0001, 34, 36, 37, 11 01) and what the
 * F93 reference FBLs answer: every routine response carries the routineStatusRecord 0x00.
 */
#ifndef ELC_BOOT_PROTOCOL_H
#define ELC_BOOT_PROTOCOL_H
#include <stdint.h>
#ifndef BOOT_CODE
#define BOOT_CODE
#endif
#define BOOT_RX_ID 0x710u
#define BOOT_TX_ID 0x710u
#define BOOT_TARGET 0x58u          /* N_TA of requests, consecutive frames and single frames for this ECU */
#define BOOT_FC_ADDRESS 0x41u      /* ZGW addresses its flow control with its tester address */
#define BOOT_GATEWAY 0xF0u         /* N_TA of everything this ECU transmits */
#define BOOT_MAX_REQUEST 240u
#define BOOT_SW_VERSION 33u        /* major = minor = patch, as in the application (EcuM_SoftwareVersion) */
#ifndef BOOT_S3_MS
#define BOOT_S3_MS 5000u           /* S3Server: no request for this long ends a non-default session (0 = never) */
#endif
typedef struct {
    uint8_t rx[BOOT_MAX_REQUEST],last[BOOT_MAX_REQUEST];
    uint8_t rxActive,rxSn,rxBlock,nextBsc,lastBsc,hasLast,partial[4],partialLen;
    uint8_t programming,extended,download,exited,verified,txRemain,txWait,txTail[6],suppress,s3Armed;
    uint16_t rxLen,rxExpected,lastLen;
    uint32_t rxTime,txTime,s3Time,start,size,received,writeAddress;
} Boot_Context;
static BOOT_CODE int BootPort_Send(const uint8_t frame[8]);
static BOOT_CODE int BootPort_Program(uint32_t address,uint32_t word);
static BOOT_CODE int BootPort_Erase(uint32_t address,uint32_t size);
static BOOT_CODE int BootPort_Finalize(uint32_t address,uint32_t size);
static BOOT_CODE int BootPort_Reset(void);
static BOOT_CODE uint32_t BootPort_Crc(uint32_t address,uint32_t size);
static BOOT_CODE void BootPort_Updater(void);
static BOOT_CODE uint32_t Boot_Read32(const uint8_t *p)
{return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static BOOT_CODE uint32_t Boot_Crc(const uint8_t *p,uint32_t size)
{
    uint32_t crc=0xFFFFFFFFu;
    while(size--){crc^=*p++;for(unsigned b=0;b<8;++b)crc=(crc>>1)^((crc&1u)?0xEDB88320u:0u);}
    return ~crc;
}
static BOOT_CODE void Boot_Init(Boot_Context *c)
{
    uint8_t *p=(uint8_t*)c;for(unsigned i=0;i<sizeof *c;++i)p[i]=0;
    c->programming=1;c->nextBsc=1;
}

/* ---- ISO 15765-2 transmit: SF up to 6 bytes, or FF plus one CF (7..11 byte responses) ---------------------- */
static BOOT_CODE int BootTp_Transmit(Boot_Context *c,const uint8_t *p,uint8_t len,uint32_t now)
{
    uint8_t f[8];for(unsigned i=0;i<8;++i)f[i]=0;
    f[0]=BOOT_GATEWAY;
    if(len>11u || !len)return -1;
    if(len<=6u){f[1]=len;for(unsigned i=0;i<len;++i)f[i+2]=p[i];return BootPort_Send(f);}
    f[1]=0x10;f[2]=len;for(unsigned i=0;i<5;++i)f[i+3]=p[i];
    if(BootPort_Send(f))return -1;
    c->txRemain=(uint8_t)(len-5u);for(unsigned i=0;i<c->txRemain;++i)c->txTail[i]=p[5u+i];
    c->txWait=0;c->txTime=now;return 0;
}

/* ---- ISO 14229-1 responses --------------------------------------------------------------------------------- */
/* Positive responses honour suppressPosRspMsgIndicationBit; negative responses (including NRC 0x78) never do. */
static BOOT_CODE int BootDcm_Respond(Boot_Context *c,const uint8_t *p,uint8_t len,uint32_t now)
{
    if(c->suppress && p[0]!=0x7Fu)return 0;
    return BootTp_Transmit(c,p,len,now);
}
static BOOT_CODE void BootDcm_Nrc(Boot_Context *c,uint8_t sid,uint8_t nrc,uint32_t now)
{uint8_t p[3];p[0]=0x7F;p[1]=sid;p[2]=nrc;(void)BootTp_Transmit(c,p,3,now);}
static BOOT_CODE int Boot_Range(uint32_t address,uint32_t size)
{return size && address>=BOOT_START && address<BOOT_END && size<=BOOT_END-address;}
static BOOT_CODE int Boot_Flush(Boot_Context *c)
{
    if(!c->partialLen)return 0;
    uint32_t w=0xFFFFFFFFu;
    for(unsigned i=0;i<c->partialLen;++i)w=(w&~(255u<<(8u*i)))|((uint32_t)c->partial[i]<<(8u*i));
    if(BootPort_Program(c->writeAddress,w)){c->download=0;return -1;}
    c->writeAddress+=4;c->partialLen=0;return 0;
}

/* Service dispatch. Order of checks: service supported (0x11), session (0x7F), length (0x13),
 * sub-function (0x12), then service specific conditions. */
static BOOT_CODE void BootDcm_Process(Boot_Context *c,const uint8_t *p,uint16_t len,uint32_t now)
{
    if(!p || !len)return;
    uint8_t r[9],sid=p[0],sub=len>1u?(uint8_t)(p[1]&0x7Fu):0u;uint16_t rid;
    r[0]=sid+0x40u;
    c->suppress=(sid==0x10||sid==0x11||sid==0x28||sid==0x31||sid==0x3E||sid==0x85) && len>1u && (p[1]&0x80u);
    c->s3Time=now;c->s3Armed=1;                                              /* S3Server runs once the tester has spoken */
    if(sid==0x10){
        if(len!=2){BootDcm_Nrc(c,sid,0x13,now);return;}
        if(sub<1u || sub>3u){BootDcm_Nrc(c,sid,0x12,now);return;}
        c->programming=sub==2;c->extended=sub==3;r[1]=sub;r[2]=0;r[3]=50;r[4]=1;r[5]=0xF4;(void)BootDcm_Respond(c,r,6,now);return;
    }
    if(sid==0x3E){
        if(len!=2){BootDcm_Nrc(c,sid,0x13,now);return;}
        if(sub!=0){BootDcm_Nrc(c,sid,0x12,now);return;}
        r[1]=0;(void)BootDcm_Respond(c,r,2,now);return;
    }
    if(sid==0x28){                                                          /* the FBL has diagnostic traffic only: nothing to switch */
        if(len!=3){BootDcm_Nrc(c,sid,0x13,now);return;}
        if(sub>3u){BootDcm_Nrc(c,sid,0x12,now);return;}
        if((p[2]&3u)==0u || (p[2]&0xFCu)){BootDcm_Nrc(c,sid,0x31,now);return;}
        r[1]=sub;(void)BootDcm_Respond(c,r,2,now);return;
    }
    if(sid==0x85){                                                          /* the FBL owns no DTC monitoring */
        if(len!=2){BootDcm_Nrc(c,sid,0x13,now);return;}
        if(sub!=1u && sub!=2u){BootDcm_Nrc(c,sid,0x12,now);return;}
        r[1]=sub;(void)BootDcm_Respond(c,r,2,now);return;
    }
    if(sid==0x22){
        if(len!=3){BootDcm_Nrc(c,sid,0x13,now);return;}
        r[1]=p[1];r[2]=p[2];
        if(p[1]!=0xF1){BootDcm_Nrc(c,sid,0x31,now);return;}
        if(p[2]==0x7E||p[2]==0x86)r[3]=c->programming?2:(c->extended?3:1);
        else if(p[2]==0x7F||p[2]==0)r[3]=BOOT_BLOCK;
        else if(p[2]==0x80){r[3]=33;r[4]=33;r[5]=33;r[6]=0xFF;(void)BootDcm_Respond(c,r,7,now);return;}
        else if(p[2]==1){for(unsigned i=3;i<9;++i)r[i]=BOOT_SW_VERSION;(void)BootDcm_Respond(c,r,9,now);return;}   /* APP x.y.z, FBL x.y.z as ZGW/FCD expect */
        else {BootDcm_Nrc(c,sid,0x31,now);return;}
        (void)BootDcm_Respond(c,r,4,now);return;
    }
    if(sid!=0x31 && sid!=0x34 && sid!=0x36 && sid!=0x37 && sid!=0x11){BootDcm_Nrc(c,sid,0x11,now);return;}
    if(!c->programming && sid!=0x11){BootDcm_Nrc(c,sid,0x7F,now);return;}
    if(sid==0x31){
        if(len<4){BootDcm_Nrc(c,sid,0x13,now);return;}
        if(sub!=1){BootDcm_Nrc(c,sid,0x12,now);return;}
        rid=(uint16_t)((p[2]<<8)|p[3]);r[1]=1;r[2]=p[2];r[3]=p[3];r[4]=0;      /* r[4]: routineStatusRecord 0 = completed */
        if(rid==0x0200){
            if(len!=5){BootDcm_Nrc(c,sid,0x13,now);return;}
            if(p[4]==BOOT_BLOCK){r[4]=p[4];(void)BootDcm_Respond(c,r,5,now);return;}
            if(BOOT_BLOCK==1 && p[4]==2){r[4]=2;if(!BootDcm_Respond(c,r,5,now))BootPort_Updater();return;}
            BootDcm_Nrc(c,sid,0x31,now);return;
        }
        if(rid==0x0155||rid==7){                                            /* FCD sends 0155 after the select; the updater just confirms */
            if(len!=4){BootDcm_Nrc(c,sid,0x13,now);return;}
            if(!BootDcm_Respond(c,r,5,now) && BOOT_BLOCK==1)BootPort_Updater();
            return;
        }
        if(rid==1 || rid==BOOT_LEGACY_ERASE){
            uint32_t address=BOOT_START,size=BOOT_END-BOOT_START;
            if(rid==1){if(len!=12){BootDcm_Nrc(c,sid,0x13,now);return;}address=Boot_Read32(p+4);size=Boot_Read32(p+8);}
            else if(len!=4){BootDcm_Nrc(c,sid,0x13,now);return;}
            if(!Boot_Range(address,size)){BootDcm_Nrc(c,sid,0x31,now);return;}
            c->download=c->verified=c->hasLast=0;
            BootDcm_Nrc(c,sid,0x78,now);
            if(BootPort_Erase(address,size)){BootDcm_Nrc(c,sid,0x72,now);return;}
            (void)BootDcm_Respond(c,r,5,now);return;
        }
        if(rid==5 && BOOT_BLOCK==1){
            if(len!=4){BootDcm_Nrc(c,sid,0x13,now);return;}
            BootDcm_Nrc(c,sid,0x78,now);
            if(BootPort_Erase(0x08003800u,1024u)){BootDcm_Nrc(c,sid,0x72,now);return;}
            (void)BootDcm_Respond(c,r,5,now);return;
        }
        if(rid==2 || rid==0){
            if((rid==2 && len!=16)||(rid==0 && len!=8)){BootDcm_Nrc(c,sid,0x13,now);return;}
            if(!c->download||c->received!=c->size|| (rid==2 && (Boot_Read32(p+4)!=c->start || Boot_Read32(p+8)!=c->size))){BootDcm_Nrc(c,sid,0x24,now);return;}
            uint32_t expected=rid==2?Boot_Read32(p+12):((uint32_t)p[4]|((uint32_t)p[5]<<8)|((uint32_t)p[6]<<16)|((uint32_t)p[7]<<24));
            BootDcm_Nrc(c,sid,0x78,now);
            if(Boot_Flush(c)||BootPort_Crc(c->start,c->size)!=expected||BootPort_Finalize(c->start,c->size)){
                BootDcm_Nrc(c,sid,0x72,now);return;
            }
            c->verified=1;(void)BootDcm_Respond(c,r,5,now);return;
        }
        BootDcm_Nrc(c,sid,0x31,now);return;
    }
    if(sid==0x34){
        if(len!=11){BootDcm_Nrc(c,sid,0x13,now);return;}
        if(p[1]!=0||p[2]!=0x44){BootDcm_Nrc(c,sid,0x31,now);return;}
        uint32_t address=Boot_Read32(p+3),size=Boot_Read32(p+7);
        if(!Boot_Range(address,size)||(address&3u)||(BOOT_BLOCK==2 && (address!=BOOT_START || size<8u))){BootDcm_Nrc(c,sid,0x31,now);return;}
        c->start=c->writeAddress=address;c->size=size;c->received=0;c->partialLen=0;
        c->hasLast=c->verified=c->exited=0;c->nextBsc=1;c->download=1;
        r[1]=0x10;r[2]=BOOT_MAX_REQUEST;(void)BootDcm_Respond(c,r,3,now);return;
    }
    if(sid==0x36){
        if(len<3||len>BOOT_MAX_REQUEST){BootDcm_Nrc(c,sid,0x13,now);return;}
        if(!c->download||c->exited){BootDcm_Nrc(c,sid,0x24,now);return;}
        r[1]=p[1];
        if(c->hasLast && p[1]==c->lastBsc){
            int equal=len==c->lastLen;
            for(unsigned i=0;i<len && equal;++i)if(p[i]!=c->last[i])equal=0;
            if(equal)(void)BootDcm_Respond(c,r,2,now);else BootDcm_Nrc(c,sid,0x73,now);return;
        }
        if(p[1]!=c->nextBsc){BootDcm_Nrc(c,sid,0x73,now);return;}
        if((uint32_t)(len-2u)>c->size-c->received){BootDcm_Nrc(c,sid,0x31,now);return;}
        for(unsigned i=2;i<len;++i){c->partial[c->partialLen++]=p[i];if(c->partialLen==4 && Boot_Flush(c)){BootDcm_Nrc(c,sid,0x72,now);return;}}
        c->received+=len-2u;c->nextBsc++;c->lastBsc=p[1];c->lastLen=len;c->hasLast=1;
        for(unsigned i=0;i<len;++i)c->last[i]=p[i];
        (void)BootDcm_Respond(c,r,2,now);return;
    }
    if(sid==0x37){
        if(len!=1 && len!=5){BootDcm_Nrc(c,sid,0x13,now);return;}
        if(!c->download || c->received!=c->size){BootDcm_Nrc(c,sid,0x24,now);return;}
        if(Boot_Flush(c)){BootDcm_Nrc(c,sid,0x72,now);return;}
        if(len==5){
            BootDcm_Nrc(c,sid,0x78,now);
            if(BootPort_Crc(c->start,c->size)!=Boot_Read32(p+1)||BootPort_Finalize(c->start,c->size)){BootDcm_Nrc(c,sid,0x72,now);return;}
            c->verified=1;
        }
        c->exited=1;(void)BootDcm_Respond(c,r,1,now);return;
    }
    /* sid == 0x11 (allowed in every session, like the F93 FBLs: a reset must not fail because S3 ran out) */
    if(len!=2){BootDcm_Nrc(c,sid,0x13,now);return;}
    if(sub!=1){BootDcm_Nrc(c,sid,0x12,now);return;}
    if(BOOT_BLOCK==2 && !c->verified){BootDcm_Nrc(c,sid,0x22,now);return;}
    r[1]=1;if(!BootDcm_Respond(c,r,2,now) && BootPort_Reset())BootDcm_Nrc(c,sid,0x22,now);
}

/* ---- ISO 15765-2 receive and timers ---------------------------------------------------------------------------- */
static BOOT_CODE void Boot_MainFunction(Boot_Context *c,uint32_t now)
{
    if(c->rxActive && (uint32_t)(now-c->rxTime)>=1000u)c->rxActive=0;       /* N_Cr */
    if(c->txRemain && (uint32_t)(now-c->txTime)>=1000u)c->txRemain=0;       /* N_Bs */
#if BOOT_S3_MS
    if(c->s3Armed && (c->programming||c->extended) && (uint32_t)(now-c->s3Time)>=BOOT_S3_MS){c->programming=c->extended=c->s3Armed=0;}   /* S3Server: default session */
#endif
}
static BOOT_CODE void BootTp_RxIndication(Boot_Context *c,const uint8_t *d,uint8_t len,uint32_t now)
{
    if(len<2||len>8)return;
    uint8_t type=d[1]>>4,n=d[1]&15u;
    if(d[0]!=BOOT_TARGET && !(type==3 && d[0]==BOOT_FC_ADDRESS))return;
    if(type==3 && c->txRemain){
        if(len<4)return;
        if(n==1){if(++c->txWait>3)c->txRemain=0;else c->txTime=now;return;}
        if(n!=0){c->txRemain=0;return;}
        uint8_t f[8];for(unsigned i=0;i<8;++i)f[i]=0;f[0]=BOOT_GATEWAY;f[1]=0x21;for(unsigned i=0;i<c->txRemain;++i)f[2u+i]=c->txTail[i];
        BootPort_Send(f);c->txRemain=0;return;
    }
    if(c->txRemain)return;
    if(type==0){if(!n||n>6||len<2+n)return;c->rxActive=0;BootDcm_Process(c,d+2,n,now);return;}
    if(type==1){
        if(len!=8)return;
        c->rxExpected=(uint16_t)((n<<8)|d[2]);c->rxActive=0;
        if(c->rxExpected<=6)return;                                         /* FF_DL fits a single frame: invalid, ignored */
        uint8_t f[8];for(unsigned i=0;i<8;++i)f[i]=0;f[0]=BOOT_GATEWAY;
        if(c->rxExpected>BOOT_MAX_REQUEST){f[1]=0x32;BootPort_Send(f);return;}   /* FC.OVFLW */
        for(unsigned i=0;i<5;++i)c->rx[i]=d[i+3];
        c->rxLen=5;c->rxSn=1;c->rxBlock=0;c->rxTime=now;
        f[1]=0x30;f[2]=8;f[3]=5;if(!BootPort_Send(f))c->rxActive=1;return;
    }
    if(type==2 && c->rxActive){
        uint16_t remain=c->rxExpected-c->rxLen;uint8_t count=remain>6?6:(uint8_t)remain;
        if(n!=c->rxSn || len<2+count){c->rxActive=0;return;}
        for(unsigned i=0;i<count;++i)c->rx[c->rxLen++]=d[i+2];
        c->rxSn=(c->rxSn+1u)&15u;c->rxTime=now;
        if(c->rxLen==c->rxExpected){c->rxActive=0;BootDcm_Process(c,c->rx,c->rxLen,now);}
        else if(++c->rxBlock==8){uint8_t f[8];for(unsigned i=0;i<8;++i)f[i]=0;f[0]=BOOT_GATEWAY;f[1]=0x30;f[2]=8;f[3]=5;c->rxBlock=0;if(BootPort_Send(f))c->rxActive=0;}
    }
}
#endif
