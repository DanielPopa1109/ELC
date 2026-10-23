/* ELC programming state machine, instantiated by resident FBL and SRAM updater.
 * No HAL, libc, function pointers, or external data dependencies in this core.
 * BOOT_CODE places every function into the SRAM blob in the updater instance.
 */
#ifndef ELC_BOOT_PROTOCOL_H
#define ELC_BOOT_PROTOCOL_H
#include <stdint.h>
#ifndef BOOT_CODE
#define BOOT_CODE
#endif
#define BOOT_RX_ID 0x710u
#define BOOT_TX_ID 0x710u
#define BOOT_TARGET 0x58u
#define BOOT_GATEWAY 0xF0u
#define BOOT_MAX_REQUEST 240u
typedef struct {
    uint8_t rx[BOOT_MAX_REQUEST],last[BOOT_MAX_REQUEST];
    uint8_t rxActive,rxSn,rxBlock,nextBsc,lastBsc,hasLast,partial[4],partialLen;
    uint8_t programming,download,exited,verified,txRemain,txWait,txTail[2];
    uint16_t rxLen,rxExpected,lastLen;
    uint32_t rxTime,txTime,start,size,received,writeAddress;
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
static BOOT_CODE int Boot_Respond(Boot_Context *c,const uint8_t *p,uint8_t len,uint32_t now)
{
    uint8_t f[8];for(unsigned i=0;i<8;++i)f[i]=0;
    f[0]=BOOT_GATEWAY;
    if(len>7u || !len)return -1;
    if(len<=6u){f[1]=len;for(unsigned i=0;i<len;++i)f[i+2]=p[i];return BootPort_Send(f);}
    f[1]=0x10;f[2]=len;for(unsigned i=0;i<5;++i)f[i+3]=p[i];
    if(BootPort_Send(f))return -1;
    c->txTail[0]=p[5];c->txTail[1]=p[6];c->txRemain=2;c->txWait=0;c->txTime=now;return 0;
}
static BOOT_CODE void Boot_Nrc(Boot_Context *c,uint8_t sid,uint8_t nrc,uint32_t now)
{uint8_t p[3];p[0]=0x7F;p[1]=sid;p[2]=nrc;(void)Boot_Respond(c,p,3,now);}
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
static BOOT_CODE void Boot_Process(Boot_Context *c,const uint8_t *p,uint16_t len,uint32_t now)
{
    if(!p || !len)return;
    uint8_t r[7],sid=p[0];uint16_t rid;
    r[0]=sid+0x40u;
    if(sid==0x10){
        if(len!=2){Boot_Nrc(c,sid,0x13,now);return;}
        if(p[1]!=2 && p[1]!=1){Boot_Nrc(c,sid,0x12,now);return;}
        c->programming=p[1]==2;r[1]=p[1];r[2]=0;r[3]=50;r[4]=1;r[5]=0xF4;Boot_Respond(c,r,6,now);return;
    }
    if(sid==0x3E){
        if(len!=2){Boot_Nrc(c,sid,0x13,now);return;}
        if((p[1]&0x7Fu)!=0){Boot_Nrc(c,sid,0x12,now);return;}
        r[1]=0;if(!p[1])Boot_Respond(c,r,2,now);return;
    }
    if(sid==0x22){
        if(len!=3){Boot_Nrc(c,sid,0x13,now);return;}
        r[1]=p[1];r[2]=p[2];
        if(p[1]!=0xF1){Boot_Nrc(c,sid,0x31,now);return;}
        if(p[2]==0x7E||p[2]==0x86)r[3]=c->programming?2:1;
        else if(p[2]==0x7F||p[2]==0)r[3]=BOOT_BLOCK;
        else if(p[2]==0x80||p[2]==1){r[3]=33;r[4]=33;r[5]=33;r[6]=0xFF;Boot_Respond(c,r,7,now);return;}
        else {Boot_Nrc(c,sid,0x31,now);return;}
        Boot_Respond(c,r,4,now);return;
    }
    if(!c->programming){Boot_Nrc(c,sid,0x7F,now);return;}
    if(sid==0x31){
        if(len<4){Boot_Nrc(c,sid,0x13,now);return;}
        if(p[1]!=1){Boot_Nrc(c,sid,0x12,now);return;}
        rid=(uint16_t)((p[2]<<8)|p[3]);r[1]=1;r[2]=p[2];r[3]=p[3];
        if(rid==0x0200){
            if(len!=5){Boot_Nrc(c,sid,0x13,now);return;}
            if(p[4]==BOOT_BLOCK){r[4]=p[4];Boot_Respond(c,r,5,now);return;}
            if(BOOT_BLOCK==1 && p[4]==2){r[4]=2;if(!Boot_Respond(c,r,5,now))BootPort_Updater();return;}
            Boot_Nrc(c,sid,0x31,now);return;
        }
        if((rid==0x0155||rid==7) && BOOT_BLOCK==1){
            if(len!=4){Boot_Nrc(c,sid,0x13,now);return;}
            if(!Boot_Respond(c,r,4,now))BootPort_Updater();
            return;
        }
        if(rid==1 || rid==BOOT_LEGACY_ERASE){
            uint32_t address=BOOT_START,size=BOOT_END-BOOT_START;
            if(rid==1){if(len!=12){Boot_Nrc(c,sid,0x13,now);return;}address=Boot_Read32(p+4);size=Boot_Read32(p+8);}
            else if(len!=4){Boot_Nrc(c,sid,0x13,now);return;}
            if(!Boot_Range(address,size)){Boot_Nrc(c,sid,0x31,now);return;}
            c->download=c->verified=c->hasLast=0;
            Boot_Nrc(c,sid,0x78,now);
            if(BootPort_Erase(address,size)){Boot_Nrc(c,sid,0x72,now);return;}
            Boot_Respond(c,r,4,now);return;
        }
        if(rid==5 && BOOT_BLOCK==1){
            if(len!=4){Boot_Nrc(c,sid,0x13,now);return;}
            Boot_Nrc(c,sid,0x78,now);
            if(BootPort_Erase(0x08003800u,1024u)){Boot_Nrc(c,sid,0x72,now);return;}
            Boot_Respond(c,r,4,now);return;
        }
        if(rid==2 || rid==0){
            if((rid==2 && len!=16)||(rid==0 && len!=8)){Boot_Nrc(c,sid,0x13,now);return;}
            if(!c->download||c->received!=c->size|| (rid==2 && (Boot_Read32(p+4)!=c->start || Boot_Read32(p+8)!=c->size))){Boot_Nrc(c,sid,0x24,now);return;}
            uint32_t expected=rid==2?Boot_Read32(p+12):((uint32_t)p[4]|((uint32_t)p[5]<<8)|((uint32_t)p[6]<<16)|((uint32_t)p[7]<<24));
            Boot_Nrc(c,sid,0x78,now);
            if(Boot_Flush(c)||BootPort_Crc(c->start,c->size)!=expected||BootPort_Finalize(c->start,c->size)){
                Boot_Nrc(c,sid,0x72,now);return;
            }
            c->verified=1;Boot_Respond(c,r,4,now);return;
        }
        Boot_Nrc(c,sid,0x31,now);return;
    }
    if(sid==0x34){
        if(len!=11){Boot_Nrc(c,sid,0x13,now);return;}
        if(p[1]!=0||p[2]!=0x44){Boot_Nrc(c,sid,0x31,now);return;}
        uint32_t address=Boot_Read32(p+3),size=Boot_Read32(p+7);
        if(!Boot_Range(address,size)||(address&3u)||(BOOT_BLOCK==2 && (address!=BOOT_START || size<8u))){Boot_Nrc(c,sid,0x31,now);return;}
        c->start=c->writeAddress=address;c->size=size;c->received=0;c->partialLen=0;
        c->hasLast=c->verified=c->exited=0;c->nextBsc=1;c->download=1;
        r[1]=0x10;r[2]=BOOT_MAX_REQUEST;Boot_Respond(c,r,3,now);return;
    }
    if(sid==0x36){
        if(len<3||len>BOOT_MAX_REQUEST){Boot_Nrc(c,sid,0x13,now);return;}
        if(!c->download||c->exited){Boot_Nrc(c,sid,0x24,now);return;}
        r[1]=p[1];
        if(c->hasLast && p[1]==c->lastBsc){
            int equal=len==c->lastLen;
            for(unsigned i=0;i<len && equal;++i)if(p[i]!=c->last[i])equal=0;
            if(equal)Boot_Respond(c,r,2,now);else Boot_Nrc(c,sid,0x73,now);return;
        }
        if(p[1]!=c->nextBsc){Boot_Nrc(c,sid,0x73,now);return;}
        if((uint32_t)(len-2u)>c->size-c->received){Boot_Nrc(c,sid,0x31,now);return;}
        for(unsigned i=2;i<len;++i){c->partial[c->partialLen++]=p[i];if(c->partialLen==4 && Boot_Flush(c)){Boot_Nrc(c,sid,0x72,now);return;}}
        c->received+=len-2u;c->nextBsc++;c->lastBsc=p[1];c->lastLen=len;c->hasLast=1;
        for(unsigned i=0;i<len;++i)c->last[i]=p[i];
        Boot_Respond(c,r,2,now);return;
    }
    if(sid==0x37){
        if(len!=1 && len!=5){Boot_Nrc(c,sid,0x13,now);return;}
        if(!c->download || c->received!=c->size){Boot_Nrc(c,sid,0x24,now);return;}
        if(Boot_Flush(c)){Boot_Nrc(c,sid,0x72,now);return;}
        if(len==5){
            Boot_Nrc(c,sid,0x78,now);
            if(BootPort_Crc(c->start,c->size)!=Boot_Read32(p+1)||BootPort_Finalize(c->start,c->size)){Boot_Nrc(c,sid,0x72,now);return;}
            c->verified=1;
        }
        c->exited=1;Boot_Respond(c,r,1,now);return;
    }
    if(sid==0x11){
        if(len!=2){Boot_Nrc(c,sid,0x13,now);return;}
        if(p[1]!=1){Boot_Nrc(c,sid,0x12,now);return;}
        if(BOOT_BLOCK==2 && !c->verified){Boot_Nrc(c,sid,0x22,now);return;}
        r[1]=1;if(!Boot_Respond(c,r,2,now) && BootPort_Reset())Boot_Nrc(c,sid,0x22,now);return;
    }
    Boot_Nrc(c,sid,0x11,now);
}
static BOOT_CODE void Boot_MainFunction(Boot_Context *c,uint32_t now)
{
    if(c->rxActive && (uint32_t)(now-c->rxTime)>=1000u)c->rxActive=0;
    if(c->txRemain && (uint32_t)(now-c->txTime)>=1000u)c->txRemain=0;
}
static BOOT_CODE void Boot_Rx(Boot_Context *c,const uint8_t *d,uint8_t len,uint32_t now)
{
    if(len<2||len>8||d[0]!=BOOT_TARGET)return;
    uint8_t type=d[1]>>4,n=d[1]&15u;
    if(type==3 && c->txRemain){
        if(len<4)return;
        if(n==1){if(++c->txWait>3)c->txRemain=0;else c->txTime=now;return;}
        if(n!=0){c->txRemain=0;return;}
        uint8_t f[8];for(unsigned i=0;i<8;++i)f[i]=0;f[0]=BOOT_GATEWAY;f[1]=0x21;f[2]=c->txTail[0];f[3]=c->txTail[1];
        BootPort_Send(f);c->txRemain=0;return;
    }
    if(c->txRemain)return;
    if(type==0){if(!n||n>6||len<2+n)return;c->rxActive=0;Boot_Process(c,d+2,n,now);return;}
    if(type==1){
        if(len!=8)return;
        c->rxExpected=(uint16_t)((n<<8)|d[2]);c->rxActive=0;
        uint8_t f[8];for(unsigned i=0;i<8;++i)f[i]=0;f[0]=BOOT_GATEWAY;
        if(c->rxExpected<=6 || c->rxExpected>BOOT_MAX_REQUEST){f[1]=0x32;BootPort_Send(f);return;}
        for(unsigned i=0;i<5;++i)c->rx[i]=d[i+3];
        c->rxLen=5;c->rxSn=1;c->rxBlock=0;c->rxTime=now;
        f[1]=0x30;f[2]=8;f[3]=5;if(!BootPort_Send(f))c->rxActive=1;return;
    }
    if(type==2 && c->rxActive){
        uint16_t remain=c->rxExpected-c->rxLen;uint8_t count=remain>6?6:(uint8_t)remain;
        if(n!=c->rxSn || len<2+count){c->rxActive=0;return;}
        for(unsigned i=0;i<count;++i)c->rx[c->rxLen++]=d[i+2];
        c->rxSn=(c->rxSn+1u)&15u;c->rxTime=now;
        if(c->rxLen==c->rxExpected){c->rxActive=0;Boot_Process(c,c->rx,c->rxLen,now);}
        else if(++c->rxBlock==8){uint8_t f[8];for(unsigned i=0;i<8;++i)f[i]=0;f[0]=BOOT_GATEWAY;f[1]=0x30;f[2]=8;f[3]=5;c->rxBlock=0;if(BootPort_Send(f))c->rxActive=0;}
    }
}
#endif
