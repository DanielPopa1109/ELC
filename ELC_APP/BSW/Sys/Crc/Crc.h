/* Crc.h - AUTOSAR Crc library subset (CRC-32, IEEE 802.3, reflected 0xEDB88320).
 * Include with a relative path: the CubeMX hardware CRC header Inc/crc.h has the same
 * name on case-insensitive file systems. */
#ifndef ELC_BSW_CRC_H
#define ELC_BSW_CRC_H

#include "Std_Types.h"

/* Crc_IsFirstCall == TRUE starts a new CRC (seed 0xFFFFFFFF). Otherwise Crc_StartValue32
 * is the value returned by the previous call. Result is XORed with 0xFFFFFFFF. */
uint32 Crc_CalculateCRC32(const uint8 *Crc_DataPtr, uint32 Crc_Length, uint32 Crc_StartValue32,
                          boolean Crc_IsFirstCall);

#endif /* ELC_BSW_CRC_H */
