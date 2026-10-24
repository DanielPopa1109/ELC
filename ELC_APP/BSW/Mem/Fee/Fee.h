/* Fee.h - Flash EEPROM emulation: one fixed-size logical block in two alternating 1 KiB pages.
 *
 * Page layout (word offsets): 0 magic "ELC2", 1 generation, 2 payload size, 3 payload CRC32,
 * 4 header CRC32 (words 0..3), 8.. payload, 255 commit word 0xA55A69C3.
 * A write erases the inactive page, programs header+payload four words per Fee_MainFunction()
 * call, verifies it by readback and finally programs the commit word; the previously committed
 * page stays valid until the commit word is read back, so a power cut at any point leaves the
 * old or the new block. If neither page can be written without destroying unrecovered legacy
 * data (Fee_AllowMigration() not granted) the write is refused with MEMIF_MIGRATION_REQUIRED. */
#ifndef ELC_BSW_FEE_H
#define ELC_BSW_FEE_H

#include "../MemIf/MemIf_Types.h"

#define FEE_PAGE_COUNT     2u
#define FEE_MAX_BLOCK_SIZE 988u   /* 1024 - 32 header - 4 commit */

void Fee_Init(uint16 BlockSize);
Std_ReturnType Fee_Read(uint8 *DataBufferPtr);               /* latest committed block, or E_NOT_OK */
Std_ReturnType Fee_Write(const uint8 *DataBufferPtr);        /* starts an asynchronous job */
void Fee_MainFunction(void);
MemIf_JobResultType Fee_GetJobResult(void);
void Fee_AllowMigration(uint8 Page);                         /* page that may be erased although no valid block exists */

/* ELC: staging buffer for the block payload; passing it to Fee_Write() avoids a copy. */
uint8 *Fee_GetBuffer(void);

#endif /* ELC_BSW_FEE_H */
