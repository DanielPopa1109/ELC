#include "Crc.h"

uint32 Crc_CalculateCRC32(const uint8 *Crc_DataPtr, uint32 Crc_Length, uint32 Crc_StartValue32,
                          boolean Crc_IsFirstCall)
{
    uint32 crc = (Crc_IsFirstCall != 0u) ? 0xFFFFFFFFu : ~Crc_StartValue32;
    uint32 bit;

    while (Crc_Length-- != 0u)
    {
        crc ^= *Crc_DataPtr++;
        for (bit = 0u; bit < 8u; ++bit)
        {
            crc = (crc >> 1u) ^ (((crc & 1u) != 0u) ? 0xEDB88320u : 0u);
        }
    }
    return ~crc;
}
