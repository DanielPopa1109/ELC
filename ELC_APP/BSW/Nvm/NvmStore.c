#include "NvmStore.h"
#include <string.h>
#define MAGIC 0x32434C45u /* ELC2 */
#define COMMIT 0xA55A69C3u
static uint32_t image[256];
static uint16_t dataSize, position;
static uint32_t generation;
static int8_t active, target, migration;
static uint8_t phase;
static Nvm_Result result;
uint32_t Crc_CalculateCRC32(const void *data, uint16_t size)
{
    const uint8_t *p = data;
    uint32_t crc = 0xFFFFFFFFu;
    while (size--) {
        crc ^= *p++;
        for (unsigned b=0; b<8; ++b) crc=(crc>>1)^((crc&1u)?0xEDB88320u:0u);
    }
    return ~crc;
}
static int valid(uint8_t page, uint32_t *gen)
{
    if (MemIf_Read(page*NVM_STORE_PAGE,image,NVM_STORE_PAGE)) return 0;
    if (image[0]!=MAGIC || image[2]!=dataSize || image[255]!=COMMIT ||
        image[4]!=Crc_CalculateCRC32(image,16u) ||
        image[3]!=Crc_CalculateCRC32(&image[8],dataSize)) return 0;
    *gen=image[1]; return 1;
}
void NvmStore_Init(uint16_t size)
{
    uint32_t gen;
    active=-1; migration=-1; generation=0; phase=0; dataSize=size;
    result=NVM_DEFAULTS;
    if (size>NVM_STORE_MAX || (size&3u)) { result=NVM_FAILED; return; }
    for(uint8_t p=0;p<2;++p) if(valid(p,&gen) &&
        (active<0 || (int32_t)(gen-generation)>0)) {active=(int8_t)p;generation=gen;result=NVM_OK;}
}
int NvmStore_Read(void *dst)
{
    if (!dst || active<0 || phase) return -1;
    return MemIf_Read((uint32_t)active*NVM_STORE_PAGE+32u,dst,dataSize);
}
void NvmStore_AllowMigration(uint8_t page) { if(page<2u) migration=(int8_t)page; }
int NvmStore_Write(const void *src)
{
    if (!src || phase || !dataSize || (dataSize&3u) || dataSize>NVM_STORE_MAX) return -1;
    if(active>=0){
        uint32_t word;uint8_t equal=1;
        for(unsigned off=0;off<dataSize;off+=4u){
            if(MemIf_Read((uint32_t)active*1024u+32u+off,&word,4u)||memcmp(&word,(const uint8_t*)src+off,4u)){equal=0;break;}
        }
        if(equal){result=NVM_OK;return 0;}
    }
    target=(active>=0)?(int8_t)(1-active):migration;
    if(target<0) {result=NVM_MIGRATION_REQUIRED;return -1;}
    if(src!=&image[8]){memset(image,0xFF,sizeof image);memcpy(&image[8],src,dataSize);}
    else memset(image,0xFF,32u);
    image[0]=MAGIC;image[1]=generation+1u;image[2]=dataSize;

    image[3]=Crc_CalculateCRC32(&image[8],dataSize);
    image[4]=Crc_CalculateCRC32(image,16u);
    image[255]=COMMIT;
    phase=1;position=0;result=NVM_PENDING;return 0;
}
void NvmStore_MainFunction(void)
{
    uint32_t base=(uint32_t)target*NVM_STORE_PAGE, word;
    if (!phase) return;
    if (phase==1) {
        if(MemIf_Erase((uint8_t)target)) goto fail;
        phase=2;return;
    }
    /* Four words per invocation; commit is deliberately a separate last job. */
    if (phase==2) {
        for(unsigned n=0;n<4u && position<8u+dataSize/4u;++n,++position)
            if(MemIf_Program(base+position*4u,image[position])) goto fail;
        if(position==8u+dataSize/4u) {phase=3;position=0;}
        return;
    }
    if(phase==3) {
        for(unsigned n=0;n<4u && position<8u+dataSize/4u;++n,++position)
            if(MemIf_Read(base+position*4u,&word,4u) || word!=image[position]) goto fail;
        if(position==8u+dataSize/4u) phase=4;
        return;
    }
    if(MemIf_Program(base+1020u,COMMIT) || MemIf_Read(base+1020u,&word,4u) || word!=COMMIT) goto fail;
    active=target;generation++;result=NVM_OK;phase=0;return;
fail:
    phase=0;result=NVM_FAILED;
}
Nvm_Result NvmStore_GetResult(void) {return result;}

void *NvmStore_GetBuffer(void){return &image[8];}
