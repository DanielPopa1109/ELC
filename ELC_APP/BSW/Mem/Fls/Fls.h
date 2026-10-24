/* Fls.h - Flash driver for the STM32F103C8 internal Flash (1 KiB pages).
 *
 * Synchronous: every call returns after the HAL operation and its readback/blank check.
 * Interrupts are masked only for the duration of a single page erase or word program.
 * A page erase stalls the CPU (single bank), so callers must only erase while the load
 * path is idle (NvM_IsWriteAllowed()). Write/erase are restricted to the NvM region;
 * the calibration page is read-only here. */
#ifndef ELC_BSW_FLS_H
#define ELC_BSW_FLS_H

#include "Std_Types.h"

typedef uint32 Fls_AddressType;
typedef uint32 Fls_LengthType;

#define FLS_PAGE_SIZE           1024u
#define FLS_NVM_BASE_ADDRESS    0x0800F800u   /* two 1 KiB pages: diagnostic/battery NvM */
#define FLS_NVM_SIZE            2048u
#define FLS_CAL_BASE_ADDRESS    0x08003800u   /* one 1 KiB page: calibration (never rewritten by the APP) */
#define FLS_CAL_SIZE            1024u

Std_ReturnType Fls_Read(Fls_AddressType SourceAddress, uint8 *TargetAddressPtr, Fls_LengthType Length);
Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length);   /* whole pages */
Std_ReturnType Fls_Write(Fls_AddressType TargetAddress, const uint8 *SourceAddressPtr, Fls_LengthType Length); /* 32-bit words */

#endif /* ELC_BSW_FLS_H */
