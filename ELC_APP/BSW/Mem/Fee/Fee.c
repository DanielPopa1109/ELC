#include "Fee.h"
#include "../Fls/Fls.h"
#include "../../Sys/Crc/Crc.h"
#include <string.h>

#define FEE_MAGIC           0x32434C45u   /* "ELC2" */
#define FEE_COMMIT          0xA55A69C3u
#define FEE_WORDS_PER_PAGE  256u
#define FEE_PAYLOAD_WORD    8u            /* payload starts at byte 32 */
#define FEE_COMMIT_WORD     255u
#define FEE_WORDS_PER_CALL  4u

enum { FEE_IDLE = 0u, FEE_ERASE, FEE_PROGRAM, FEE_VERIFY, FEE_COMMIT_PHASE };

static uint32 Fee_Image[FEE_WORDS_PER_PAGE];   /* read/stage buffer for a whole page */
static uint16 Fee_BlockSize;
static uint16 Fee_Position;
static uint32 Fee_Generation;
static sint8  Fee_Active;                      /* page holding the latest committed block, -1 if none */
static sint8  Fee_Target;
static sint8  Fee_Migration;                   /* page that may be overwritten without a valid block, -1 if none */
static uint8  Fee_Phase;
static MemIf_JobResultType Fee_Result;

static uint32 Fee_Crc(const void *data, uint32 length)
{
    return Crc_CalculateCRC32((const uint8 *)data, length, 0u, TRUE);
}

static uint32 Fee_PageAddress(uint8 page)
{
    return FLS_NVM_BASE_ADDRESS + ((uint32)page * FLS_PAGE_SIZE);
}

static boolean Fee_PageValid(uint8 page, uint32 *generation)
{
    if (Fls_Read(Fee_PageAddress(page), (uint8 *)Fee_Image, FLS_PAGE_SIZE) != E_OK)
    {
        return FALSE;
    }
    if ((Fee_Image[0] != FEE_MAGIC) || (Fee_Image[2] != Fee_BlockSize) || (Fee_Image[FEE_COMMIT_WORD] != FEE_COMMIT) ||
        (Fee_Image[4] != Fee_Crc(Fee_Image, 16u)) ||
        (Fee_Image[3] != Fee_Crc(&Fee_Image[FEE_PAYLOAD_WORD], Fee_BlockSize)))
    {
        return FALSE;
    }
    *generation = Fee_Image[1];
    return TRUE;
}

void Fee_Init(uint16 BlockSize)
{
    uint32 generation;
    uint8 page;

    Fee_Active = -1;
    Fee_Migration = -1;
    Fee_Generation = 0u;
    Fee_Phase = FEE_IDLE;
    Fee_BlockSize = BlockSize;
    Fee_Result = MEMIF_BLOCK_INVALID;
    if ((BlockSize > FEE_MAX_BLOCK_SIZE) || ((BlockSize & 3u) != 0u))
    {
        Fee_Result = MEMIF_JOB_FAILED;
        return;
    }
    for (page = 0u; page < FEE_PAGE_COUNT; ++page)
    {
        /* Serial-number arithmetic: the newest generation wins even across wrap-around. */
        if ((Fee_PageValid(page, &generation) != FALSE) &&
            ((Fee_Active < 0) || ((sint32)(generation - Fee_Generation) > 0)))
        {
            Fee_Active = (sint8)page;
            Fee_Generation = generation;
            Fee_Result = MEMIF_JOB_OK;
        }
    }
}

Std_ReturnType Fee_Read(uint8 *DataBufferPtr)
{
    if ((DataBufferPtr == NULL_PTR) || (Fee_Active < 0) || (Fee_Phase != FEE_IDLE))
    {
        return E_NOT_OK;
    }
    return Fls_Read(Fee_PageAddress((uint8)Fee_Active) + (FEE_PAYLOAD_WORD * 4u), DataBufferPtr, Fee_BlockSize);
}

void Fee_AllowMigration(uint8 Page)
{
    if (Page < FEE_PAGE_COUNT)
    {
        Fee_Migration = (sint8)Page;
    }
}

uint8 *Fee_GetBuffer(void)
{
    return (uint8 *)&Fee_Image[FEE_PAYLOAD_WORD];
}

Std_ReturnType Fee_Write(const uint8 *DataBufferPtr)
{
    uint32 offset;

    if ((DataBufferPtr == NULL_PTR) || (Fee_Phase != FEE_IDLE) || (Fee_BlockSize == 0u) ||
        ((Fee_BlockSize & 3u) != 0u) || (Fee_BlockSize > FEE_MAX_BLOCK_SIZE))
    {
        return E_NOT_OK;
    }
    if (Fee_Active >= 0)
    {
        /* Identical block: nothing to store. */
        boolean equal = TRUE;
        for (offset = 0u; offset < Fee_BlockSize; offset += 4u)
        {
            uint32 word;
            if ((Fls_Read(Fee_PageAddress((uint8)Fee_Active) + (FEE_PAYLOAD_WORD * 4u) + offset, (uint8 *)&word, 4u) != E_OK) ||
                (memcmp(&word, DataBufferPtr + offset, 4u) != 0))
            {
                equal = FALSE;
                break;
            }
        }
        if (equal != FALSE)
        {
            Fee_Result = MEMIF_JOB_OK;
            return E_OK;
        }
    }
    Fee_Target = (Fee_Active >= 0) ? (sint8)(1 - Fee_Active) : Fee_Migration;
    if (Fee_Target < 0)
    {
        Fee_Result = MEMIF_MIGRATION_REQUIRED;
        return E_NOT_OK;
    }
    if (DataBufferPtr != (const uint8 *)&Fee_Image[FEE_PAYLOAD_WORD])
    {
        (void)memset(Fee_Image, 0xFF, sizeof(Fee_Image));
        (void)memcpy(&Fee_Image[FEE_PAYLOAD_WORD], DataBufferPtr, Fee_BlockSize);
    }
    else
    {
        (void)memset(Fee_Image, 0xFF, 32u);   /* header area only; payload already staged in place */
    }
    Fee_Image[0] = FEE_MAGIC;
    Fee_Image[1] = Fee_Generation + 1u;
    Fee_Image[2] = Fee_BlockSize;
    Fee_Image[3] = Fee_Crc(&Fee_Image[FEE_PAYLOAD_WORD], Fee_BlockSize);
    Fee_Image[4] = Fee_Crc(Fee_Image, 16u);
    Fee_Image[FEE_COMMIT_WORD] = FEE_COMMIT;
    Fee_Phase = FEE_ERASE;
    Fee_Position = 0u;
    Fee_Result = MEMIF_JOB_PENDING;
    return E_OK;
}

void Fee_MainFunction(void)
{
    const uint32 base = Fee_PageAddress((uint8)Fee_Target);
    const uint16 words = (uint16)(FEE_PAYLOAD_WORD + (Fee_BlockSize / 4u));
    uint32 word;
    uint8 n;

    if (Fee_Phase == FEE_IDLE)
    {
        return;
    }
    if (Fee_Phase == FEE_ERASE)
    {
        if (Fls_Erase(base, FLS_PAGE_SIZE) != E_OK) { goto fail; }
        Fee_Phase = FEE_PROGRAM;
        return;
    }
    if (Fee_Phase == FEE_PROGRAM)
    {
        /* Four words per invocation; the commit word is deliberately a separate, last job. */
        for (n = 0u; (n < FEE_WORDS_PER_CALL) && (Fee_Position < words); ++n, ++Fee_Position)
        {
            if (Fls_Write(base + ((uint32)Fee_Position * 4u), (const uint8 *)&Fee_Image[Fee_Position], 4u) != E_OK) { goto fail; }
        }
        if (Fee_Position == words)
        {
            Fee_Phase = FEE_VERIFY;
            Fee_Position = 0u;
        }
        return;
    }
    if (Fee_Phase == FEE_VERIFY)
    {
        for (n = 0u; (n < FEE_WORDS_PER_CALL) && (Fee_Position < words); ++n, ++Fee_Position)
        {
            if ((Fls_Read(base + ((uint32)Fee_Position * 4u), (uint8 *)&word, 4u) != E_OK) ||
                (word != Fee_Image[Fee_Position]))
            {
                goto fail;
            }
        }
        if (Fee_Position == words)
        {
            Fee_Phase = FEE_COMMIT_PHASE;
        }
        return;
    }

    word = FEE_COMMIT;
    if ((Fls_Write(base + (FEE_COMMIT_WORD * 4u), (const uint8 *)&word, 4u) != E_OK) ||
        (Fls_Read(base + (FEE_COMMIT_WORD * 4u), (uint8 *)&word, 4u) != E_OK) || (word != FEE_COMMIT))
    {
        goto fail;
    }
    Fee_Active = Fee_Target;
    Fee_Generation++;
    Fee_Result = MEMIF_JOB_OK;
    Fee_Phase = FEE_IDLE;
    return;

fail:
    Fee_Phase = FEE_IDLE;
    Fee_Result = MEMIF_JOB_FAILED;
}

MemIf_JobResultType Fee_GetJobResult(void)
{
    return Fee_Result;
}
