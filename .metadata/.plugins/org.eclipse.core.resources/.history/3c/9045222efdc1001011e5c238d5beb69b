
#define NVM_NO_BLOCKS               2U //+1, first is not used
#define NVM_SIZE_HEADER_BYTES       8U // Data-flash write done in 8 bytes at a time
#include <stdint.h>


typedef struct
{
        uint8_t blockId;
        uint16_t blockSize;
        uint8_t padding1;
        uint32_t padding2;
}Nvm_Header_t;

typedef struct
{
        uint16_t blockSize;
        uint32_t blockAddress;
        uint16_t padding1;
}Nvm_NvStat_t;

typedef struct
{
        uint32_t* data;
        uint32_t crc;
}Nvm_Block_t;

extern uint32_t Nvm_CurrentAddress;
extern uint32_t Nvm_SectorSwitchActivated;
extern uint32_t Nvm_CurrentSector;
extern Nvm_Header_t Nvm_HeaderArr[NVM_NO_BLOCKS];
extern Nvm_NvStat_t Nvm_NvStatArr[NVM_NO_BLOCKS];
extern Nvm_Block_t Nvm_BlockDataList[NVM_NO_BLOCKS];
extern Nvm_Block_t Nvm_RomDefaults_BlockDataList[NVM_NO_BLOCKS];
extern uint8_t Nvm_WriteAllFinished;
extern uint8_t Nvm_ReadAllFinished;

extern void Nvm_SectorSwitch(void);
extern void Nvm_WriteBlock(uint16_t blockId, uint32_t *data);
extern void Nvm_FindCurrentAddress();
extern void Nvm_ReadAll(void);
extern void Nvm_WriteAll(void);
