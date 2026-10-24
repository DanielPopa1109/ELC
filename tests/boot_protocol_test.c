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
{uint8_t p[11]={0x34,0,0x44};be32(p+3,address);be32(p+7,size);BootDcm_Process(c,p,11,0);}
#define expectNrc(sid,code) assert(response[0]==0x7F && response[1]==(sid) && response[2]==(code))
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
    uint8_t exit[5]={0x37,0,0,0,0};BootDcm_Process(&c,exit,5,0);expectNrc(0x37,0x24);
    block[1]=2;BootDcm_Process(&c,block,15,0);expectNrc(0x36,0x73);block[1]=1;
    BootDcm_Process(&c,block,15,0);assert(response[0]==0x76 && c.received==13 && c.partialLen==1);
    unsigned before=programs;BootDcm_Process(&c,block,15,0);assert(response[0]==0x76 && programs==before && c.received==13);
    block[3]^=1;BootDcm_Process(&c,block,15,0);expectNrc(0x36,0x73);block[3]^=1;
    BootDcm_Process(&c,exit,5,0);expectNrc(0x37,0x72);assert(!c.verified);
    be32(exit+1,Boot_Crc(block+2,13));BootDcm_Process(&c,exit,5,0);assert(response[0]==0x77 && c.verified && c.exited);
    assert(!memcmp(flash+(BOOT_START-0x08000000u),block+2,13));
    uint8_t reset[2]={0x11,1};BootDcm_Process(&c,reset,2,0);assert(resets==1);
    Boot_Init(&c);BootDcm_Process(&c,block,15,0);expectNrc(0x36,0x24);
    uint8_t erase[12]={0x31,1,0,1};be32(erase+4,BOOT_START);be32(erase+8,BOOT_END-BOOT_START);
    eraseFailure=1;BootDcm_Process(&c,erase,12,0);expectNrc(0x31,0x72);eraseFailure=0;
    BootDcm_Process(&c,erase,12,0);assert(response[0]==0x71 && response[3]==1 && response[4]==0);   /* FCD requires the routineStatusRecord 0 */
    download(&c,BOOT_START,13);programFailure=1;BootDcm_Process(&c,block,15,0);expectNrc(0x36,0x72);assert(!c.download);programFailure=0;
    BootDcm_Process(&c,erase,12,0);download(&c,BOOT_START,13);readbackFailure=1;BootDcm_Process(&c,block,15,0);expectNrc(0x36,0x72);readbackFailure=0;
    BootDcm_Process(&c,erase,12,0);download(&c,BOOT_START,13);BootDcm_Process(&c,block,15,0);finalizeFailure=1;
    BootDcm_Process(&c,exit,5,0);expectNrc(0x37,0x72);assert(!c.verified);finalizeFailure=0;
    BootDcm_Process(&c,exit,5,0);assert(response[0]==0x77);
    uint8_t check[16]={0x31,1,0,2};be32(check+4,BOOT_START);be32(check+8,13);be32(check+12,Boot_Crc(block+2,13));
    BootDcm_Process(&c,check,16,0);assert(response[0]==0x71 && response[3]==2 && response[4]==0);
    check[7]^=4;BootDcm_Process(&c,check,16,0);expectNrc(0x31,0x24);
    /* Normal-address legacy frames and wrong extended address are not accepted. */
    unsigned oldResponses=responses;uint8_t f[8]={0x57,2,0x10,2,0,0,0,0};BootTp_RxIndication(&c,f,8,1);assert(responses==oldResponses);
    f[0]=0x58;BootTp_RxIndication(&c,f,8,1);assert(response[0]==0x50);
    uint8_t ff[8]={0x58,0x10,11,0x34,0,0x44,8,0},cf[8]={0x58,0x21,0x3C,0,0,0,0,13};
    uint8_t req[11]={0x34,0,0x44};be32(req+3,BOOT_START);be32(req+7,13);
    memcpy(ff+3,req,5);memcpy(cf+2,req+5,6);
    BootTp_RxIndication(&c,ff,8,10);assert(c.rxActive && raw[1]==0x30 && raw[2]==8 && raw[3]==5);
    cf[1]=0x22;BootTp_RxIndication(&c,cf,8,15);assert(!c.rxActive);cf[1]=0x21;
    BootTp_RxIndication(&c,ff,8,20);Boot_MainFunction(&c,1020);assert(!c.rxActive);
    BootTp_RxIndication(&c,ff,8,1030);BootTp_RxIndication(&c,cf,8,1035);assert(response[0]==0x74 && c.size==13);
    uint8_t version[3]={0x22,0xF1,0x80};BootDcm_Process(&c,version,3,1100);assert(c.txRemain==2 && raw[1]==0x10);
    uint8_t fc[8]={0x58,0x30,0,0,0,0,0,0};BootTp_RxIndication(&c,fc,8,1105);assert(!c.txRemain && raw[1]==0x21 && raw[2]==33 && raw[3]==0xFF);
    BootDcm_Process(&c,version,3,1200);Boot_MainFunction(&c,2200);assert(!c.txRemain);
    Boot_Init(&c);uint8_t select[5]={0x31,1,2,0,2};BootDcm_Process(&c,select,5,0);
    if(BOOT_BLOCK==1)assert(updaters==1 && response[4]==2);else assert(updaters==0 && response[4]==2);
    /* Sequence number wraps through zero; BSC duplicate never programs twice. */
    memset(flash,255,sizeof flash);Boot_Init(&c);download(&c,BOOT_START,260);
    uint8_t one[3]={0x36,1,0xA5};
    for(unsigned i=0;i<260;++i){one[1]=(uint8_t)(i+1u);BootDcm_Process(&c,one,3,i);assert(response[0]==0x76);}
    assert(c.received==260 && c.nextBsc==5);
    /* ---- ISO 15765-2 conformance of the boot transport ---------------------------------------------------------------- */
    {
        unsigned before; uint8_t ffShort[8] = {0x58, 0x10, 6, 0x22, 0xF1, 0x80, 0, 0}, ffMin[8] = {0x58, 0x10, 7, 0x22, 0xF1, 0x80, 0, 0};
        uint8_t ffBig[8] = {0x58, 0x10, 241, 0, 0, 0, 0, 0}, cf1[8] = {0x58, 0x21, 0, 0, 0, 0, 0, 0}, version[3] = {0x22, 0xF1, 0x80};
        Boot_Init(&c); before = responses;
        BootTp_RxIndication(&c, ffShort, 8, 0); assert(responses == before && !c.rxActive);          /* FF_DL <= 6: invalid, ignored (no FC) */
        ffShort[2] = 0; BootTp_RxIndication(&c, ffShort, 8, 0); assert(responses == before && !c.rxActive);
        ffMin[2] = 7; BootTp_RxIndication(&c, ffMin, 8, 0); assert(responses == before + 1 && raw[1] == 0x30 && c.rxActive);   /* smallest legal FF */
        BootTp_RxIndication(&c, ffBig, 8, 1); assert(responses == before + 2 && raw[1] == 0x32 && !c.rxActive);               /* FC.OVFLW */
        ffBig[2] = 240; BootTp_RxIndication(&c, ffBig, 8, 2); assert(raw[1] == 0x30 && c.rxActive && c.rxExpected == 240);
        { uint8_t shortFf[6] = {0x58, 0x10, 20, 0, 0, 0}; Boot_Init(&c); before = responses; BootTp_RxIndication(&c, shortFf, 6, 3); assert(responses == before && !c.rxActive); }   /* FF must be 8 bytes */
        /* flow control of the gateway: the ZGW tester address 0x41 and our own address are accepted, others are not */
        Boot_Init(&c); BootDcm_Process(&c, version, 3, 10); assert(c.txRemain == 2 && raw[1] == 0x10);
        { uint8_t fc57[8] = {0x57, 0x30, 0, 0, 0, 0, 0, 0}; BootTp_RxIndication(&c, fc57, 8, 11); assert(c.txRemain == 2); }
        { uint8_t fc41[8] = {0x41, 0x30, 0, 0, 0, 0, 0, 0}; BootTp_RxIndication(&c, fc41, 8, 12); assert(!c.txRemain && raw[1] == 0x21 && raw[2] == 33 && raw[3] == 0xFF); }
        Boot_Init(&c); BootDcm_Process(&c, version, 3, 20);
        { uint8_t wait[8] = {0x41, 0x31, 0, 0, 0, 0, 0, 0}; for (unsigned i = 0; i < 3; ++i) { BootTp_RxIndication(&c, wait, 8, 21); } assert(c.txRemain == 2); BootTp_RxIndication(&c, wait, 8, 22); assert(!c.txRemain); }   /* N_WFTmax 3 */
        Boot_Init(&c); BootDcm_Process(&c, version, 3, 30);
        { uint8_t ovfl[8] = {0x41, 0x32, 0, 0, 0, 0, 0, 0}; BootTp_RxIndication(&c, ovfl, 8, 31); assert(!c.txRemain); }
        Boot_Init(&c); BootDcm_Process(&c, version, 3, 40);
        { uint8_t bad[8] = {0x41, 0x33, 0, 0, 0, 0, 0, 0}; BootTp_RxIndication(&c, bad, 8, 41); assert(!c.txRemain); }                  /* reserved flow status */
        Boot_Init(&c); BootDcm_Process(&c, version, 3, 50);
        Boot_MainFunction(&c, 50 + 999); assert(c.txRemain == 2); Boot_MainFunction(&c, 50 + 1000); assert(!c.txRemain);            /* N_Bs */
        (void)cf1;
    }

    /* ---- ISO 14229-1: sub-function suppression, NRC order and the S3 timer ------------------------------------------------ */
    {
        unsigned before; uint8_t p[16];
        Boot_Init(&c);
        p[0] = 0x10; p[1] = 0x81; before = responses; BootDcm_Process(&c, p, 2, 0); assert(responses == before && !c.programming);   /* suppressed 10 01 */
        p[1] = 0x82; BootDcm_Process(&c, p, 2, 0); assert(responses == before && c.programming);                                   /* suppressed 10 02 */
        p[1] = 0x02; BootDcm_Process(&c, p, 2, 0); assert(responses == before + 1 && !memcmp(response, "\x50\x02\x00\x32\x01\xF4", 6));
        p[1] = 0x03; BootDcm_Process(&c, p, 2, 0); assert(!memcmp(response, "\x50\x03\x00\x32\x01\xF4", 6) && !c.programming && c.extended);   /* FCD opens every flash with 10 03 */
        p[0] = 0x22; p[1] = 0xF1; p[2] = 0x86; BootDcm_Process(&c, p, 3, 0); assert(response[0] == 0x62 && response[3] == 3);
        p[0] = 0x10; p[1] = 0x02; BootDcm_Process(&c, p, 2, 0); assert(c.programming && !c.extended);
        p[1] = 0x00; BootDcm_Process(&c, p, 2, 0); expectNrc(0x10, 0x12);
        BootDcm_Process(&c, p, 1, 0); expectNrc(0x10, 0x13);
        p[0] = 0x3E; p[1] = 0x80; before = responses; BootDcm_Process(&c, p, 2, 0); assert(responses == before);
        p[1] = 0x00; BootDcm_Process(&c, p, 2, 0); assert(responses == before + 1 && response[0] == 0x7E && response[1] == 0);
        p[1] = 0x01; BootDcm_Process(&c, p, 2, 0); expectNrc(0x3E, 0x12);
        p[1] = 0x81; BootDcm_Process(&c, p, 2, 0); expectNrc(0x3E, 0x12);                                                          /* NRCs are never suppressed */
        BootDcm_Process(&c, p, 1, 0); expectNrc(0x3E, 0x13);
        /* suppressed erase: only the NRC 0x78 is sent, the routine still runs */
        { uint8_t erase81[12] = {0x31, 0x81, 0, 1}; be32(erase81 + 4, BOOT_START); be32(erase81 + 8, BOOT_END - BOOT_START);
          unsigned erased = erasures; before = responses; BootDcm_Process(&c, erase81, 12, 0);
          assert(erasures == erased + 1 && responses == before + 1 && response[0] == 0x7F && response[2] == 0x78); }
        /* unsupported services answer 0x11 in every session; supported ones are refused with 0x7F in the default session */
        p[0] = 0x99; BootDcm_Process(&c, p, 1, 0); expectNrc(0x99, 0x11);
        p[0] = 0x27; BootDcm_Process(&c, p, 2, 0); expectNrc(0x27, 0x11);
        p[0] = 0x85; p[1] = 0x02; BootDcm_Process(&c, p, 2, 0); assert(response[0] == 0xC5 && response[1] == 2);                    /* FCD: ControlDTCSetting off */
        p[1] = 0x82; before = responses; BootDcm_Process(&c, p, 2, 0); assert(responses == before);
        p[1] = 0x00; BootDcm_Process(&c, p, 2, 0); expectNrc(0x85, 0x12);
        BootDcm_Process(&c, p, 1, 0); expectNrc(0x85, 0x13);
        p[0] = 0x28; p[1] = 0x01; p[2] = 0x03; BootDcm_Process(&c, p, 3, 0); assert(response[0] == 0x68 && response[1] == 1);        /* FCD: enableRxAndDisableTx */
        p[1] = 0x81; before = responses; BootDcm_Process(&c, p, 3, 0); assert(responses == before);
        p[1] = 0x04; BootDcm_Process(&c, p, 3, 0); expectNrc(0x28, 0x12);
        p[1] = 0x01; p[2] = 0x00; BootDcm_Process(&c, p, 3, 0); expectNrc(0x28, 0x31);
        BootDcm_Process(&c, p, 2, 0); expectNrc(0x28, 0x13);
        p[0] = 0x10; p[1] = 0x01; BootDcm_Process(&c, p, 2, 0); assert(!c.programming);
        p[0] = 0x99; BootDcm_Process(&c, p, 1, 0); expectNrc(0x99, 0x11);
        p[0] = 0x34; BootDcm_Process(&c, p, 1, 0); expectNrc(0x34, 0x7F);
        { unsigned resetsBefore = resets; p[0] = 0x11; p[1] = 0x01; BootDcm_Process(&c, p, 2, 0);                                   /* reset is not tied to the programming session */
          if (BOOT_BLOCK == 1) { assert(response[0] == 0x51 && resets == resetsBefore + 1); } else { expectNrc(0x11, 0x22); } }
        p[0] = 0x31; p[1] = 0x01; p[2] = 0x02; p[3] = 0x00; BootDcm_Process(&c, p, 4, 0); expectNrc(0x31, 0x7F);
        p[0] = 0x22; p[1] = 0xF1; p[2] = 0x86; BootDcm_Process(&c, p, 3, 0); assert(response[0] == 0x62 && response[3] == 1);        /* readable in the default session */
        /* S3Server: runs once the tester has spoken, TesterPresent restarts it, a new 10 02 re-enters programming */
        Boot_Init(&c); Boot_MainFunction(&c, 1000000u); assert(c.programming);                                                     /* no tester yet: stays */
        p[0] = 0x3E; p[1] = 0x00; BootDcm_Process(&c, p, 2, 1000u);
        Boot_MainFunction(&c, 1000u + BOOT_S3_MS - 1u); assert(c.programming);
        Boot_MainFunction(&c, 1000u + BOOT_S3_MS); assert(!c.programming);
        p[0] = 0x34; BootDcm_Process(&c, p, 1, 7000u); expectNrc(0x34, 0x7F);
        p[0] = 0x10; p[1] = 0x02; BootDcm_Process(&c, p, 2, 7000u); assert(c.programming && response[0] == 0x50 && response[1] == 2);
        p[0] = 0x3E; p[1] = 0x80; BootDcm_Process(&c, p, 2, 9000u); Boot_MainFunction(&c, 9000u + BOOT_S3_MS - 1u); assert(c.programming);
        Boot_MainFunction(&c, 9000u + BOOT_S3_MS); assert(!c.programming);
    }

    /* ---- FCD flash sequence (FCD.pyw _execute_target_flash_preamble / _execute_payload) and the response checks it makes ------------ */
    {
        uint8_t q[12], image[8] = {1, 2, 3, 4, 5, 6, 7, 8}, tr[10] = {0x36, 1, 1, 2, 3, 4, 5, 6, 7, 8}, ex[5] = {0x37};
        unsigned before;
        memset(flash, 255, sizeof flash);
        Boot_Init(&c);                                             /* the node waits in the FBL (for example without a valid application) */
        q[0] = 0x10; q[1] = 0x03; BootDcm_Process(&c, q, 2, 0); assert(response[0] == 0x50 && response[1] == 3);
        q[0] = 0x28; q[1] = 0x01; q[2] = 0x03; BootDcm_Process(&c, q, 3, 0); assert(response[0] == 0x68);
        q[0] = 0x85; q[1] = 0x02; BootDcm_Process(&c, q, 2, 0); assert(response[0] == 0xC5);
        q[0] = 0x10; q[1] = 0x02; BootDcm_Process(&c, q, 2, 0); assert(response[0] == 0x50 && response[1] == 2 && c.programming);
        q[0] = 0x31; q[1] = 0x01; q[2] = 0x02; q[3] = 0x00; q[4] = BOOT_BLOCK; BootDcm_Process(&c, q, 5, 0);   /* select the block that is flashed */
        assert(!memcmp(response, "\x71\x01\x02\x00", 4) && response[4] == BOOT_BLOCK);
        q[0] = 0x31; q[1] = 0x01; q[2] = 0x01; q[3] = 0x55; BootDcm_Process(&c, q, 4, 0);                   /* "start FBL RAM updater" is also sent to the updater */
        assert(!memcmp(response, "\x71\x01\x01\x55\x00", 5));
        q[0] = 0x31; q[1] = 0x01; q[2] = 0x00; q[3] = 0x01; be32(q + 4, BOOT_START); be32(q + 8, 8);
        BootDcm_Process(&c, q, 12, 0); assert(!memcmp(response, "\x71\x01\x00\x01\x00", 5));                   /* erase: FCD compares the payload with 00 */
        q[0] = 0x34; q[1] = 0x00; q[2] = 0x44; be32(q + 3, BOOT_START); be32(q + 7, 8); BootDcm_Process(&c, q, 11, 0);
        assert(!memcmp(response, "\x74\x10\xF0", 3));                                                          /* max block length 240: 238 data bytes per TransferData */
        BootDcm_Process(&c, tr, 10, 0); assert(!memcmp(response, "\x76\x01", 2));
        be32(ex + 1, Boot_Crc(image, 8)); BootDcm_Process(&c, ex, 5, 0); assert(response[0] == 0x77 && c.verified);
        before = resets; q[0] = 0x11; q[1] = 0x01; BootDcm_Process(&c, q, 2, 0); assert(response[0] == 0x51 && resets == before + 1);
        /* the software version DID is a 9 byte response (APP x.y.z, FBL x.y.z): first frame plus one consecutive frame */
        Boot_Init(&c); q[0] = 0x22; q[1] = 0xF1; q[2] = 0x01; BootDcm_Process(&c, q, 3, 100);
        assert(c.txRemain == 4 && raw[1] == 0x10 && raw[2] == 9 && raw[3] == 0x62 && raw[4] == 0xF1 && raw[5] == 0x01 && raw[6] == BOOT_SW_VERSION && raw[7] == BOOT_SW_VERSION);
        { uint8_t fc[8] = {BOOT_FC_ADDRESS, 0x30, 0, 0, 0, 0, 0, 0}; BootTp_RxIndication(&c, fc, 8, 110);
          assert(!c.txRemain && raw[1] == 0x21 && raw[2] == BOOT_SW_VERSION && raw[5] == BOOT_SW_VERSION && raw[6] == 0); }
        /* S3Server also ends the extended session */
        Boot_Init(&c); q[0] = 0x10; q[1] = 0x03; BootDcm_Process(&c, q, 2, 1000u); Boot_MainFunction(&c, 1000u + BOOT_S3_MS - 1u); assert(c.extended);
        Boot_MainFunction(&c, 1000u + BOOT_S3_MS); assert(!c.extended && !c.programming);
        q[0] = 0x22; q[1] = 0xF1; q[2] = 0x86; BootDcm_Process(&c, q, 3, 7000u); assert(response[3] == 1);
    }
    puts("Boot: actual resident/SRAM protocol core limits, BSC/retry/wrap, CRC, flash errors, transport, FCD flash sequence and handover assertions passed");
}
