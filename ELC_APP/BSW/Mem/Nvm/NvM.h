/* NvM.h - NVRAM manager (ELC subset).
 *
 * Three RAM blocks (DEM status, DEM freeze frames, battery model) form ONE stored snapshot, so
 * any block write stores all blocks atomically in the Fee journal. NvM_ReadAll() is executed
 * synchronously during start-up (before the scheduler runs). Write jobs are asynchronous:
 * NvM_WriteAll()/NvM_WriteBlock() start a Fee job, NvM_MainFunction() advances it and
 * NvM_GetErrorStatus() reports NVM_REQ_PENDING until it is committed.
 * Immutable defaults are RAM-only (init callbacks); the Flash image is never erased at start-up. */
#ifndef ELC_BSW_NVM_H
#define ELC_BSW_NVM_H

#include "Std_Types.h"
#include "NvM_Cfg.h"

typedef enum
{
    NVM_REQ_OK = 0u,
    NVM_REQ_NOT_OK,
    NVM_REQ_PENDING,
    NVM_REQ_INTEGRITY_FAILED,
    NVM_REQ_BLOCK_SKIPPED,
    NVM_REQ_NV_INVALIDATED,
    NVM_REQ_CANCELED,
    NVM_REQ_REDUNDANCY_FAILED,
    NVM_REQ_RESTORED_FROM_ROM,    /* no valid stored block: RAM holds the defaults */
    NVM_REQ_MIGRATION_REQUIRED    /* ELC: legacy journal occupies both pages; needs offline conversion */
} NvM_RequestResultType;

void NvM_Init(void);
void NvM_ReadAll(void);
Std_ReturnType NvM_WriteAll(void);
Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void *NvM_DstPtr);
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void *NvM_SrcPtr);
Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType BlockId, NvM_RequestResultType *RequestResultPtr);
void NvM_MainFunction(void);   /* 5 ms; Flash work only runs while NvM_IsWriteAllowed() */

/* ELC: a page erase stalls the CPU, so writes only progress while the load path is idle. */
uint8 NvM_IsWriteAllowed(void);

#endif /* ELC_BSW_NVM_H */
