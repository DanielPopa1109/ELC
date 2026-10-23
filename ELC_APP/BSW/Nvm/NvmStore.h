#ifndef NVM_STORE_H
#define NVM_STORE_H
#include <stdint.h>
#define NVM_STORE_PAGE 1024u
#define NVM_STORE_MAX  988u
typedef enum { NVM_OK, NVM_PENDING, NVM_FAILED, NVM_DEFAULTS, NVM_MIGRATION_REQUIRED } Nvm_Result;
/* Port operations return zero only after readback verification. */
int MemIf_Read(uint32_t offset, void *dst, uint16_t size);
int MemIf_Erase(uint8_t page);
int MemIf_Program(uint32_t offset, uint32_t word);
void NvmStore_Init(uint16_t size);
void *NvmStore_GetBuffer(void);
int NvmStore_Read(void *dst);
int NvmStore_Write(const void *src);
void NvmStore_MainFunction(void);
Nvm_Result NvmStore_GetResult(void);
void NvmStore_AllowMigration(uint8_t page);
uint32_t Crc_CalculateCRC32(const void *data, uint16_t size);
#endif
