/* MemIf_Types.h - memory abstraction job result (AUTOSAR MemIf). ELC has one device (Fee),
 * so NvM calls Fee directly and the MemIf layer reduces to this type. */
#ifndef ELC_BSW_MEMIF_TYPES_H
#define ELC_BSW_MEMIF_TYPES_H

#include "Std_Types.h"

typedef enum
{
    MEMIF_JOB_OK = 0u,
    MEMIF_JOB_FAILED,
    MEMIF_JOB_PENDING,
    MEMIF_JOB_CANCELED,
    MEMIF_BLOCK_INCONSISTENT,
    MEMIF_BLOCK_INVALID,          /* no valid block found (erased device) */
    MEMIF_MIGRATION_REQUIRED      /* ELC: both banks hold legacy data that must not be erased */
} MemIf_JobResultType;

#endif /* ELC_BSW_MEMIF_TYPES_H */
