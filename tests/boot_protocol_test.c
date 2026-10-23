#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if BOOT_TEST_BLOCK==1
#define BOOT_START 0x08003C00u
#define BOOT_END 0x0800F7FCu
#define BOOT_LEGACY_ERASE 4
#else
#define BOOT_START 0x08000000u
#define BOOT_END 0x08003800u
#define BOOT_LEGACY_ERASE 6
#endif
#define BOOT_BLOCK BOOT_TEST_BLOCK
#include "../ELC_FBL/Core/Inc/boot_protocol.h"
static uint8_t flash[65536],response[8],raw[8];
static unsigned responses,programs,erasures,resets,updaters;
static int eraseFailure,programFailure,readbackFailure,finalizeFailure;
static BOOT_CODE int BootPort_Send(const uint8_t frame[8])
{
    memcpy(raw,frame,8);assert(frame[0]==BOOT_GATEWAY);responses++;
    if((frame[1]>>4)==0){memset(response,0,sizeof response);memcpy(response,frame+2,frame[1]);}
    return 0;
}
static BOOT_CODE int BootPort_Program(uint32_t address,uint32_t word)
{
    assert(address>=BOOT_START && address+4u<=((BOOT_END+3u)&~3u) && !(address&3u));programs++;
    if(programFailure)return -1;
    uint8_t *p=flash+(address-0x08000000u),*d=(uint8_t*)&word;
    for(unsigned i=0;i<4;++i){if((p[i]&d[i])!=d[i])return -1;p[i]&=d[i];}
    return readbackFailure?-1:0;
}
static BOOT_CODE int BootPort_Erase(uint32_t address,uint32_t size)
{
    erasures++;if(eraseFailure)return -1;
    assert((address>=BOOT_START && size<=BOOT_END-address)||(BOOT_BLOCK==1 && address==0x08003800u && size==1024u));
    memset(flash+(address-0x08000000u),255,size);return 0;
}
static BOOT_CODE uint32_t BootPort_Crc(uint32_t address,uint32_t size)
{return Boot_Crc(flash+(address-0x08000000u),size);}
static BOOT_CODE int BootPort_Finalize(uint32_t address,uint32_t size)
{(void)address;(void)size;return finalizeFailure?-1:0;}
static BOOT_CODE int BootPort_Reset(void){resets++;return 0;}
static BOOT_CODE void BootPort_Updater(void){updaters++;}
static void be32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void download(Boot_Context *c,uint32_t address,uint32_t size)
{uint8_t p[11]={0x34,0,0x44};be32(p+3,address);be32(p+7,size);Boot_Process(c,p,11,0);}
static void expectNrc(uint8_t sid,uint8_t code){assert(response[0]==0x7F && response[1]==sid && response[2]==code);}
int main(void)
{
    Boot_Context c;memset(flash,255,sizeof flash);Boot_Init(&c);
    assert(Boot_Crc((const uint8_t*)"123456789",9)==0xCBF43926u);
    download(&c,BOOT_START-4u,13);expectNrc(0x34,0x31);
    download(&c,BOOT_START,0);expectNrc(0x34,0x31);
    download(&c,BOOT_START,0xFFFFFFFFu);expectNrc(0x34,0x31);
    download(&c,BOOT_START+1u,13);expectNrc(0x34,0x31);
    download(&c,BOOT_START,13);assert(response[0]==0x74 && response[2]==240);
    uint8_t block[15]={0x36,1,0xC0,0x4F,0,0x20,1,0x3C,0,8,1,2,3,4,5};
    uint8_t exit[5]={0x37,0,0,0,0};Boot_Process(&c,exit,5,0);expectNrc(0x37,0x24);
    block[1]=2;Boot_Process(&c,block,15,0);expectNrc(0x36,0x73);block[1]=1;
    Boot_Process(&c,block,15,0);assert(response[0]==0x76 && c.received==13 && c.partialLen==1);
    unsigned before=programs;Boot_Process(&c,block,15,0);assert(response[0]==0x76 && programs==before && c.received==13);
    block[3]^=1;Boot_Process(&c,block,15,0);expectNrc(0x36,0x73);block[3]^=1;
    Boot_Process(&c,exit,5,0);expectNrc(0x37,0x72);assert(!c.verified);
    be32(exit+1,Boot_Crc(block+2,13));Boot_Process(&c,exit,5,0);assert(response[0]==0x77 && c.verified && c.exited);
    assert(!memcmp(flash+(BOOT_START-0x08000000u),block+2,13));
    uint8_t reset[2]={0x11,1};Boot_Process(&c,reset,2,0);assert(resets==1);
    Boot_Init(&c);Boot_Process(&c,block,15,0);expectNrc(0x36,0x24);
    uint8_t erase[12]={0x31,1,0,1};be32(erase+4,BOOT_START);be32(erase+8,BOOT_END-BOOT_START);
    eraseFailure=1;Boot_Process(&c,erase,12,0);expectNrc(0x31,0x72);eraseFailure=0;
    Boot_Process(&c,erase,12,0);assert(response[0]==0x71);
    download(&c,BOOT_START,13);programFailure=1;Boot_Process(&c,block,15,0);expectNrc(0x36,0x72);assert(!c.download);programFailure=0;
    Boot_Process(&c,erase,12,0);download(&c,BOOT_START,13);readbackFailure=1;Boot_Process(&c,block,15,0);expectNrc(0x36,0x72);readbackFailure=0;
    Boot_Process(&c,erase,12,0);download(&c,BOOT_START,13);Boot_Process(&c,block,15,0);finalizeFailure=1;
    Boot_Process(&c,exit,5,0);expectNrc(0x37,0x72);assert(!c.verified);finalizeFailure=0;
    Boot_Process(&c,exit,5,0);assert(response[0]==0x77);
    uint8_t check[16]={0x31,1,0,2};be32(check+4,BOOT_START);be32(check+8,13);be32(check+12,Boot_Crc(block+2,13));
    Boot_Process(&c,check,16,0);assert(response[0]==0x71 && response[3]==2);
    check[7]^=4;Boot_Process(&c,check,16,0);expectNrc(0x31,0x24);
    /* Normal-address legacy frames and wrong extended address are not accepted. */
    unsigned oldResponses=responses;uint8_t f[8]={0x57,2,0x10,2,0,0,0,0};Boot_Rx(&c,f,8,1);assert(responses==oldResponses);
    f[0]=0x58;Boot_Rx(&c,f,8,1);assert(response[0]==0x50);
    uint8_t ff[8]={0x58,0x10,11,0x34,0,0x44,8,0},cf[8]={0x58,0x21,0x3C,0,0,0,0,13};
    uint8_t req[11]={0x34,0,0x44};be32(req+3,BOOT_START);be32(req+7,13);
    memcpy(ff+3,req,5);memcpy(cf+2,req+5,6);
    Boot_Rx(&c,ff,8,10);assert(c.rxActive && raw[1]==0x30 && raw[2]==8 && raw[3]==5);
    cf[1]=0x22;Boot_Rx(&c,cf,8,15);assert(!c.rxActive);cf[1]=0x21;
    Boot_Rx(&c,ff,8,20);Boot_MainFunction(&c,1020);assert(!c.rxActive);
    Boot_Rx(&c,ff,8,1030);Boot_Rx(&c,cf,8,1035);assert(response[0]==0x74 && c.size==13);
    uint8_t version[3]={0x22,0xF1,0x80};Boot_Process(&c,version,3,1100);assert(c.txRemain==2 && raw[1]==0x10);
    uint8_t fc[8]={0x58,0x30,0,0,0,0,0,0};Boot_Rx(&c,fc,8,1105);assert(!c.txRemain && raw[1]==0x21 && raw[2]==33 && raw[3]==0xFF);
    Boot_Process(&c,version,3,1200);Boot_MainFunction(&c,2200);assert(!c.txRemain);
    Boot_Init(&c);uint8_t select[5]={0x31,1,2,0,2};Boot_Process(&c,select,5,0);
    if(BOOT_BLOCK==1)assert(updaters==1 && response[4]==2);else assert(updaters==0 && response[4]==2);
    /* Sequence number wraps through zero; BSC duplicate never programs twice. */
    memset(flash,255,sizeof flash);Boot_Init(&c);download(&c,BOOT_START,260);
    uint8_t one[3]={0x36,1,0xA5};
    for(unsigned i=0;i<260;++i){one[1]=(uint8_t)(i+1u);Boot_Process(&c,one,3,i);assert(response[0]==0x76);}
    assert(c.received==260 && c.nextBsc==5);
    puts("Boot: actual resident/SRAM protocol core limits, BSC/retry/wrap, CRC, flash errors, transport and handover assertions passed");
}
