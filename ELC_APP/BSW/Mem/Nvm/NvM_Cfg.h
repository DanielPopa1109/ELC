/* NvM_Cfg.h - NvM block configuration. */
#ifndef ELC_BSW_NVM_CFG_H
#define ELC_BSW_NVM_CFG_H

#include "Std_Types.h"

typedef uint16 NvM_BlockIdType;

#define NVM_BLOCK_ID_ALL            0u   /* multi block request (ReadAll/WriteAll) */
#define NVM_BLOCK_ID_DEM_STATUS     1u   /* Dem_DTC_Stat  */
#define NVM_BLOCK_ID_DEM_FF         2u   /* Dem_FF        */
#define NVM_BLOCK_ID_BATTERY        3u   /* SMon_Battery  */
#define NVM_NUMBER_OF_BLOCKS        3u

typedef struct
{
    uint16 length;
    void  *ramBlock;
    void (*initCallback)(void);   /* default image; NULL means "zero fill" */
} NvM_BlockDescriptorType;

/* Index == BlockId - 1. */
extern const NvM_BlockDescriptorType NvM_BlockDescriptor[NVM_NUMBER_OF_BLOCKS];

/* Sum of the block lengths: the size of the stored snapshot. */
uint16 NvM_Cfg_GetSnapshotSize(void);

/* Application gate for Flash work (see NvM_IsWriteAllowed()). */
uint8 NvM_Cfg_IsWriteAllowed(void);

#endif /* ELC_BSW_NVM_CFG_H */
