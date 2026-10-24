#include "NvM.h"
#include "../Fee/Fee.h"
#include "../Fls/Fls.h"
#include "main.h"
#include <string.h>

/* Pre-Fee ("legacy") journal: marker words, then records of
 *   [blockId:1][pad:1][size:2][pad:4] data[size] crc32[4] pad[4]  (16 + size bytes),
 * CRC = STM32 hardware CRC over little-endian words, no final XOR. */
#define NVM_LEGACY_MARKER        0xB6B6B6B6u
#define NVM_LEGACY_HEADER_SIZE   8u
#define NVM_LEGACY_RECORD_EXTRA  16u
#define NVM_LEGACY_CRC_POLY      0x04C11DB7u

static uint8 *NvM_Snapshot;   /* Fee staging buffer holding the concatenated blocks */

static void NvM_CopyBlocks(boolean load)
{
    uint8 *p = NvM_Snapshot;
    uint8 i;

    for (i = 0u; i < NVM_NUMBER_OF_BLOCKS; ++i)
    {
        const NvM_BlockDescriptorType *d = &NvM_BlockDescriptor[i];

        if (load != FALSE)
        {
            (void)memcpy(d->ramBlock, p, d->length);
        }
        else
        {
            (void)memcpy(p, d->ramBlock, d->length);
        }
        p += d->length;
    }
}

static void NvM_RestoreDefaults(uint8 index)
{
    const NvM_BlockDescriptorType *d = &NvM_BlockDescriptor[index];

    if (d->initCallback != NULL_PTR)
    {
        d->initCallback();
    }
    else
    {
        (void)memset(d->ramBlock, 0, d->length);
    }
}

static uint32 NvM_LegacyCrc(const uint8 *data, uint16 size)
{
    uint32 crc = 0xFFFFFFFFu;
    uint16 w;
    uint8 bit;

    for (w = 0u; w < (size / 4u); ++w)
    {
        uint32 word;
        (void)memcpy(&word, &data[4u * w], 4u);
        crc ^= word;
        for (bit = 0u; bit < 32u; ++bit)
        {
            crc = (crc << 1u) ^ (((crc & 0x80000000u) != 0u) ? NVM_LEGACY_CRC_POLY : 0u);
        }
    }
    return crc;
}

void NvM_Init(void)
{
    NvM_Snapshot = NULL_PTR;
}

void NvM_ReadAll(void)
{
    uint32 protectedPages = 0u;
    uint32 latest[NVM_NUMBER_OF_BLOCKS + 1u] = {0u};
    uint32 marker[2];
    uint32 off;
    uint8 i;

    Fee_Init(NvM_Cfg_GetSnapshotSize());
    NvM_Snapshot = Fee_GetBuffer();
    if (Fee_Read(NvM_Snapshot) == E_OK)
    {
        NvM_CopyBlocks(TRUE);
        return;
    }
    for (i = 0u; i < NVM_NUMBER_OF_BLOCKS; ++i)
    {
        NvM_RestoreDefaults(i);
    }

    /* No valid Fee block. Read a legacy journal without erasing anything; only a page that
     * holds no recoverable latest record may later be reclaimed. */
    (void)Fls_Read(FLS_NVM_BASE_ADDRESS, (uint8 *)marker, 8u);
    if ((marker[0] == NVM_LEGACY_MARKER) && (marker[1] == marker[0]))
    {
        protectedPages |= 1u;                /* legacy recovery needs the page-zero start marker */
        for (off = 8u; (off + NVM_LEGACY_RECORD_EXTRA) <= FLS_NVM_SIZE;)
        {
            uint8 header[NVM_LEGACY_HEADER_SIZE];
            uint32 crc;
            uint16 size;
            uint8 id;
            const NvM_BlockDescriptorType *d;

            (void)Fls_Read(FLS_NVM_BASE_ADDRESS + off, header, NVM_LEGACY_HEADER_SIZE);
            id = header[0];
            if (id == 0xFFu)
            {
                break;
            }
            size = (uint16)(header[2] | ((uint16)header[3] << 8));
            if ((id == 0u) || (id > NVM_NUMBER_OF_BLOCKS) || (size != NvM_BlockDescriptor[id - 1u].length) ||
                ((off + NVM_LEGACY_RECORD_EXTRA + size) > FLS_NVM_SIZE))
            {
                break;
            }
            d = &NvM_BlockDescriptor[id - 1u];
            (void)Fls_Read(FLS_NVM_BASE_ADDRESS + off + 8u, NvM_Snapshot, size);
            (void)Fls_Read(FLS_NVM_BASE_ADDRESS + off + 8u + size, (uint8 *)&crc, 4u);
            if (NvM_LegacyCrc(NvM_Snapshot, size) == crc)
            {
                (void)memcpy(d->ramBlock, NvM_Snapshot, size);
                latest[id] = off;
            }
            off += NVM_LEGACY_RECORD_EXTRA + size;
        }
        for (i = 1u; i <= NVM_NUMBER_OF_BLOCKS; ++i)
        {
            if (latest[i] != 0u)
            {
                protectedPages |= 1u << (latest[i] / FLS_PAGE_SIZE);
                protectedPages |= 1u << ((latest[i] + 15u + NvM_BlockDescriptor[i - 1u].length) / FLS_PAGE_SIZE);
            }
        }
    }
    else
    {
        /* Unknown, non-erased contents are retained for recovery. */
        uint8 page;
        for (page = 0u; page < FEE_PAGE_COUNT; ++page)
        {
            for (off = 0u; off < FLS_PAGE_SIZE; off += 4u)
            {
                uint32 word;
                (void)Fls_Read(FLS_NVM_BASE_ADDRESS + ((uint32)page * FLS_PAGE_SIZE) + off, (uint8 *)&word, 4u);
                if (word != 0xFFFFFFFFu)
                {
                    protectedPages |= 1u << page;
                }
            }
        }
    }
    if ((protectedPages & 1u) == 0u)
    {
        Fee_AllowMigration(0u);
    }
    else if ((protectedPages & 2u) == 0u)
    {
        Fee_AllowMigration(1u);
    }
}

Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void *NvM_SrcPtr)
{
    const NvM_BlockDescriptorType *d;
    uint32 primask;
    Std_ReturnType rc;

    if ((NvM_Snapshot == NULL_PTR) || (BlockId == 0u) || (BlockId > NVM_NUMBER_OF_BLOCKS) ||
        (NvM_SrcPtr == NULL_PTR) || (Fee_GetJobResult() == MEMIF_JOB_PENDING))
    {
        return E_NOT_OK;
    }
    d = &NvM_BlockDescriptor[BlockId - 1u];
    primask = __get_PRIMASK();
    __disable_irq();
    if (NvM_SrcPtr != d->ramBlock)
    {
        (void)memcpy(d->ramBlock, NvM_SrcPtr, d->length);
    }
    NvM_CopyBlocks(FALSE);
    rc = Fee_Write(NvM_Snapshot);
    __set_PRIMASK(primask);
    return rc;
}

Std_ReturnType NvM_WriteAll(void)
{
    return NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, NvM_BlockDescriptor[0].ramBlock);
}

Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void *NvM_DstPtr)
{
    uint16 offset = 0u;
    uint8 i;

    if ((NvM_Snapshot == NULL_PTR) || (BlockId == 0u) || (BlockId > NVM_NUMBER_OF_BLOCKS) ||
        (NvM_DstPtr == NULL_PTR) || (Fee_GetJobResult() == MEMIF_JOB_PENDING) || (Fee_Read(NvM_Snapshot) != E_OK))
    {
        return E_NOT_OK;
    }
    for (i = 0u; i < (BlockId - 1u); ++i)
    {
        offset = (uint16)(offset + NvM_BlockDescriptor[i].length);
    }
    (void)memcpy(NvM_DstPtr, NvM_Snapshot + offset, NvM_BlockDescriptor[BlockId - 1u].length);
    return E_OK;
}

Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType BlockId, NvM_RequestResultType *RequestResultPtr)
{
    if ((RequestResultPtr == NULL_PTR) || (BlockId > NVM_NUMBER_OF_BLOCKS))
    {
        return E_NOT_OK;
    }
    switch (Fee_GetJobResult())
    {
    case MEMIF_JOB_OK:             *RequestResultPtr = NVM_REQ_OK; break;
    case MEMIF_JOB_PENDING:        *RequestResultPtr = NVM_REQ_PENDING; break;
    case MEMIF_BLOCK_INVALID:      *RequestResultPtr = NVM_REQ_RESTORED_FROM_ROM; break;
    case MEMIF_MIGRATION_REQUIRED: *RequestResultPtr = NVM_REQ_MIGRATION_REQUIRED; break;
    case MEMIF_JOB_CANCELED:       *RequestResultPtr = NVM_REQ_CANCELED; break;
    case MEMIF_BLOCK_INCONSISTENT: *RequestResultPtr = NVM_REQ_INTEGRITY_FAILED; break;
    default:                       *RequestResultPtr = NVM_REQ_NOT_OK; break;
    }
    return E_OK;
}

uint8 NvM_IsWriteAllowed(void)
{
    return NvM_Cfg_IsWriteAllowed();
}

void NvM_MainFunction(void)
{
    if (NvM_IsWriteAllowed() != 0u)
    {
        Fee_MainFunction();
    }
}
