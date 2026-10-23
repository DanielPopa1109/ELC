#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "NvmStore.h"
static uint8_t flash[2048];
static int failErase,failProgram,badReadback;
int MemIf_Read(uint32_t off,void *dst,uint16_t size){assert(off+size<=2048);memcpy(dst,flash+off,size);return 0;}
int MemIf_Erase(uint8_t page){assert(page<2);if(failErase)return -1;memset(flash+page*1024,255,1024);return 0;}
int MemIf_Program(uint32_t off,uint32_t word)
{
    assert(off<=2044 && !(off&3));if(failProgram)return -1;
    uint8_t *p=(uint8_t*)&word;for(unsigned i=0;i<4;++i){assert((flash[off+i]&p[i])==p[i]);flash[off+i]&=p[i];}
    if(badReadback){flash[off]^=1;badReadback=0;}return 0;
}
static void drain(void){for(unsigned i=0;i<1000 && NvmStore_GetResult()==NVM_PENDING;++i)NvmStore_MainFunction();}
int main(void)
{
    uint32_t a[161],b[161],out[161];uint8_t baseline[2048];
    memset(a,0x12,sizeof a);memset(b,0x34,sizeof b);memset(flash,255,sizeof flash);
    assert(Crc_CalculateCRC32("123456789",9)==0xCBF43926u);
    NvmStore_Init(sizeof a);assert(NvmStore_GetResult()==NVM_DEFAULTS);
    assert(NvmStore_Read(out)<0);assert(NvmStore_Write(a)<0);
    NvmStore_AllowMigration(0);assert(!NvmStore_Write(a));drain();assert(NvmStore_GetResult()==NVM_OK);
    assert(!NvmStore_Read(out) && !memcmp(a,out,sizeof a));memcpy(baseline,flash,sizeof flash);
    /* Reboot at every worker boundary: the old or fully committed new snapshot survives. */
    for(unsigned cut=0;cut<90;++cut){
        memcpy(flash,baseline,sizeof flash);NvmStore_Init(sizeof a);assert(!NvmStore_Write(b));
        for(unsigned n=0;n<cut;++n)NvmStore_MainFunction();
        NvmStore_Init(sizeof a);assert(!NvmStore_Read(out));
        assert(!memcmp(out,a,sizeof a)||!memcmp(out,b,sizeof b));
    }
    memcpy(flash,baseline,sizeof flash);NvmStore_Init(sizeof a);failErase=1;
    assert(!NvmStore_Write(b));drain();assert(NvmStore_GetResult()==NVM_FAILED);failErase=0;
    NvmStore_Init(sizeof a);assert(!NvmStore_Read(out)&&!memcmp(a,out,sizeof a));
    assert(!NvmStore_Write(b));failProgram=1;drain();assert(NvmStore_GetResult()==NVM_FAILED);failProgram=0;
    NvmStore_Init(sizeof a);assert(!NvmStore_Write(b));badReadback=1;drain();assert(NvmStore_GetResult()==NVM_FAILED);
    NvmStore_Init(sizeof a);assert(!NvmStore_Read(out)&&!memcmp(a,out,sizeof a));
    assert(!NvmStore_Write(b));drain();assert(NvmStore_GetResult()==NVM_OK);
    flash[1024+36]^=1;NvmStore_Init(sizeof a);assert(!NvmStore_Read(out)&&!memcmp(a,out,sizeof a));
    /* A torn commit marker is never accepted. */
    memcpy(flash,baseline,sizeof flash);NvmStore_Init(sizeof a);assert(!NvmStore_Write(b));drain();
    flash[2047]=255;NvmStore_Init(sizeof a);assert(!NvmStore_Read(out)&&!memcmp(a,out,sizeof a));
    /* Compaction failure after several generations still preserves the active page. */
    for(unsigned generation=0;generation<16;++generation){b[0]=generation;assert(!NvmStore_Write(b));drain();assert(NvmStore_GetResult()==NVM_OK);}
    assert(!NvmStore_Read(out)&&!memcmp(b,out,sizeof b));
    uint8_t unchanged[2048];memcpy(unchanged,flash,sizeof flash);assert(!NvmStore_Write(b));assert(NvmStore_GetResult()==NVM_OK);assert(!memcmp(unchanged,flash,sizeof flash));
    puts("NvM: 90 restart cuts, alternating-page compaction, CRC/defaults, erase/program/readback and torn-commit assertions passed");
}
