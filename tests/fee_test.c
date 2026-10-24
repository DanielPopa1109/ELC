/* Fee journal: production Fee.c + Crc.c over a mocked Fls (1 KiB pages, 1->0 programming only). */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../ELC_APP/BSW/Mem/Fee/Fee.h"
#include "../ELC_APP/BSW/Mem/Fls/Fls.h"
#include "../ELC_APP/BSW/Sys/Crc/Crc.h"

#define BASE FLS_NVM_BASE_ADDRESS
static uint8_t flash[2048];
static int failErase, failProgram, badReadback, silentFlip;

Std_ReturnType Fls_Read(Fls_AddressType a, uint8 *dst, Fls_LengthType n)
{
    assert(a >= BASE && a + n <= BASE + 2048u);
    memcpy(dst, flash + (a - BASE), n);
    return E_OK;
}
Std_ReturnType Fls_Erase(Fls_AddressType a, Fls_LengthType n)
{
    assert(a >= BASE && a + n <= BASE + 2048u && !(a % 1024u) && !(n % 1024u));
    if (failErase) { return E_NOT_OK; }
    memset(flash + (a - BASE), 255, n);
    return E_OK;
}
Std_ReturnType Fls_Write(Fls_AddressType a, const uint8 *src, Fls_LengthType n)
{
    assert(a >= BASE && a + n <= BASE + 2048u && !(a & 3u) && !(n & 3u));
    if (failProgram) { return E_NOT_OK; }
    for (unsigned i = 0; i < n; ++i)
    {
        uint8_t *p = flash + (a - BASE) + i;
        assert((*p & src[i]) == src[i]);      /* NOR Flash can only clear bits */
        *p &= src[i];
    }
    if (badReadback) { flash[a - BASE] ^= 1; badReadback = 0; return E_NOT_OK; }
    if (silentFlip) { flash[a - BASE] ^= 1; silentFlip = 0; }   /* corrupted although the driver reports success */
    return E_OK;
}

static void drain(void)
{
    for (unsigned i = 0; i < 1000 && Fee_GetJobResult() == MEMIF_JOB_PENDING; ++i) { Fee_MainFunction(); }
}

int main(void)
{
    uint32_t a[161], b[161], out[161];
    uint8_t baseline[2048];

    memset(a, 0x12, sizeof a); memset(b, 0x34, sizeof b); memset(flash, 255, sizeof flash);
    assert(Crc_CalculateCRC32((const uint8_t *)"123456789", 9, 0, 1) == 0xCBF43926u);
    assert(Crc_CalculateCRC32((const uint8_t *)"12345", 5, 0, 1) != 0);
    { uint32_t part = Crc_CalculateCRC32((const uint8_t *)"1234", 4, 0, 1);       /* split calls give the same CRC */
      assert(Crc_CalculateCRC32((const uint8_t *)"56789", 5, part, 0) == 0xCBF43926u); }
    Fee_Init(sizeof a); assert(Fee_GetJobResult() == MEMIF_BLOCK_INVALID);
    assert(Fee_Read((uint8_t *)out) == E_NOT_OK); assert(Fee_Write((const uint8_t *)a) == E_NOT_OK);
    assert(Fee_GetJobResult() == MEMIF_MIGRATION_REQUIRED);
    Fee_AllowMigration(0); assert(Fee_Write((const uint8_t *)a) == E_OK); drain(); assert(Fee_GetJobResult() == MEMIF_JOB_OK);
    assert(Fee_Read((uint8_t *)out) == E_OK && !memcmp(a, out, sizeof a)); memcpy(baseline, flash, sizeof flash);

    /* Reboot at every worker boundary: the old or the fully committed new block survives. */
    for (unsigned cut = 0; cut < 90; ++cut)
    {
        memcpy(flash, baseline, sizeof flash); Fee_Init(sizeof a); assert(Fee_Write((const uint8_t *)b) == E_OK);
        for (unsigned n = 0; n < cut; ++n) { Fee_MainFunction(); }
        Fee_Init(sizeof a); assert(Fee_Read((uint8_t *)out) == E_OK);
        assert(!memcmp(out, a, sizeof a) || !memcmp(out, b, sizeof b));
    }
    memcpy(flash, baseline, sizeof flash); Fee_Init(sizeof a); failErase = 1;
    assert(Fee_Write((const uint8_t *)b) == E_OK); drain(); assert(Fee_GetJobResult() == MEMIF_JOB_FAILED); failErase = 0;
    Fee_Init(sizeof a); assert(Fee_Read((uint8_t *)out) == E_OK && !memcmp(a, out, sizeof a));
    assert(Fee_Write((const uint8_t *)b) == E_OK); failProgram = 1; drain(); assert(Fee_GetJobResult() == MEMIF_JOB_FAILED); failProgram = 0;
    Fee_Init(sizeof a); assert(Fee_Write((const uint8_t *)b) == E_OK); badReadback = 1; drain(); assert(Fee_GetJobResult() == MEMIF_JOB_FAILED);
    Fee_Init(sizeof a); assert(Fee_Read((uint8_t *)out) == E_OK && !memcmp(a, out, sizeof a));
    assert(Fee_Write((const uint8_t *)b) == E_OK); silentFlip = 1; drain(); assert(Fee_GetJobResult() == MEMIF_JOB_FAILED);   /* verify phase */
    Fee_Init(sizeof a); assert(Fee_Read((uint8_t *)out) == E_OK && !memcmp(a, out, sizeof a));
    assert(Fee_Write((const uint8_t *)b) == E_OK); drain(); assert(Fee_GetJobResult() == MEMIF_JOB_OK);
    flash[1024 + 36] ^= 1; Fee_Init(sizeof a); assert(Fee_Read((uint8_t *)out) == E_OK && !memcmp(a, out, sizeof a));

    /* A torn commit marker is never accepted. */
    memcpy(flash, baseline, sizeof flash); Fee_Init(sizeof a); assert(Fee_Write((const uint8_t *)b) == E_OK); drain();
    flash[2047] = 255; Fee_Init(sizeof a); assert(Fee_Read((uint8_t *)out) == E_OK && !memcmp(a, out, sizeof a));

    /* Compaction after several generations; identical block is not rewritten. */
    for (unsigned generation = 0; generation < 16; ++generation)
    {
        b[0] = generation; assert(Fee_Write((const uint8_t *)b) == E_OK); drain(); assert(Fee_GetJobResult() == MEMIF_JOB_OK);
    }
    assert(Fee_Read((uint8_t *)out) == E_OK && !memcmp(b, out, sizeof b));
    { uint8_t unchanged[2048]; memcpy(unchanged, flash, sizeof flash);
      assert(Fee_Write((const uint8_t *)b) == E_OK); assert(Fee_GetJobResult() == MEMIF_JOB_OK); assert(!memcmp(unchanged, flash, sizeof flash)); }

    /* Parameter checks. */
    Fee_Init(2); assert(Fee_GetJobResult() == MEMIF_JOB_FAILED);          /* size not a multiple of 4 */
    Fee_Init(FEE_MAX_BLOCK_SIZE + 4u); assert(Fee_GetJobResult() == MEMIF_JOB_FAILED);
    Fee_Init(sizeof a); assert(Fee_Read(0) == E_NOT_OK && Fee_Write(0) == E_NOT_OK);
    /* Staging buffer: writing from the buffer returned by Fee_GetBuffer() needs no copy. */
    { uint8_t *stage = Fee_GetBuffer(); memset(stage, 0x5A, sizeof a); assert(Fee_Write(stage) == E_OK); drain();
      assert(Fee_GetJobResult() == MEMIF_JOB_OK); Fee_Init(sizeof a); memset(stage, 0, sizeof a); assert(Fee_Read(stage) == E_OK && stage[0] == 0x5A && stage[sizeof a - 1u] == 0x5A); }
    puts("Fee: 90 restart cuts, alternating-page compaction, CRC/defaults, erase/program/readback and torn-commit assertions passed");
    return 0;
}
