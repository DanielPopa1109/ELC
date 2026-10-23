#ifndef NVM_H
#define NVM_H
#include <stdint.h>
#include <string.h>
#include "crc.h"
#include "main.h"
#include "NvmStore.h"
#define NVM_NO_BLOCKS 4u
#define NVM_PARAMFLASH_START_ADDRESS 0x08003800u
#define NVM_DTC_START_ADDRESS 0x0800F800u
#define NVM_DTC_END_ADDRESS 0x0800FFFFu
typedef struct {uint8_t blockId;uint16_t blockSize;uint8_t padding1;uint32_t padding2;} Nvm_Header_t;
extern uint8_t Nvm_ParamFlashBlock[292];
extern uint8_t Nvm_ParameterIntegrity; /* 0 defaults/invalid, 1 legacy/unverified, 2 CRC validated */
void Nvm_ReadAll(void);
int Nvm_WriteAll(void);
int Nvm_WriteBlock(uint16_t id,const void *data);
int Nvm_ReadBlock(uint16_t id,void *dst);
void Nvm_MainFunction(void);
uint8_t Nvm_IsWriteAllowed(void);
Nvm_Result Nvm_GetErrorStatus(uint16_t id);
void Nvm_InitParamFlash(void);
void Nvm_FlashReadData(uint32_t addr,uint32_t *dst,uint16_t words);
#endif
