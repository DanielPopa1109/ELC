// seedNKeyDiag.cpp – DIY Seed/Key DLL for TSMaster

#include <cstdint>

// Helper: our 32-bit algorithm
static std::uint32_t GenKeyFromSeed(std::uint32_t seed32, std::uint32_t level)
{
    std::uint32_t x = seed32 ^ 0xA5A5A5A5u;
    x += 0x13572468u + (level * 0x1F3D5B79u);
    x = (x << 3) | (x >> (32u - 3u));  // ROL3

    std::uint32_t rev =
        ((seed32 & 0x000000FFu) << 24) |
        ((seed32 & 0x0000FF00u) << 8) |
        ((seed32 & 0x00FF0000u) >> 8) |
        ((seed32 & 0xFF000000u) >> 24);

    return x ^ rev;
}

// Exported interface for TSMaster (Function interface 1)
extern "C" __declspec(dllexport)
unsigned int GenerateKeyEx(
    const unsigned char* ipSeedArray,     // [in] seed bytes from ECU
    unsigned int         iSeedArraySize,  // [in] seed length
    const unsigned int   iSecurityLevel,  // [in] security level
    const char* ipVariant,       // [in] variant (can be ignored)
    unsigned char* iopKeyArray,     // [out] key bytes
    unsigned int         iMaxKeyArraySize,// [in] key buffer size
    unsigned int& oActualKeyArraySize // [out] key length
)
{
    (void)ipVariant; // unused in this simple implementation

    // basic checks – no fancy error codes, just 0/!=0 for lab usage
    if (!ipSeedArray || !iopKeyArray)
        return 1u;

    if (iSeedArraySize < 4u)
        return 2u;

    if (iMaxKeyArraySize < 4u)
        return 3u;

    // Build 32-bit seed from first 4 bytes (big-endian)
    std::uint32_t seed32 =
        ((std::uint32_t)ipSeedArray[0] << 24) |
        ((std::uint32_t)ipSeedArray[1] << 16) |
        ((std::uint32_t)ipSeedArray[2] << 8) |
        ((std::uint32_t)ipSeedArray[3] << 0);

    std::uint32_t key32 = GenKeyFromSeed(seed32, iSecurityLevel);

    // Split key back to bytes (big-endian)
    iopKeyArray[0] = (unsigned char)((key32 >> 24) & 0xFF);
    iopKeyArray[1] = (unsigned char)((key32 >> 16) & 0xFF);
    iopKeyArray[2] = (unsigned char)((key32 >> 8) & 0xFF);
    iopKeyArray[3] = (unsigned char)((key32 >> 0) & 0xFF);

    oActualKeyArraySize = 4u;

    // 0 = OK for our simple implementation
    return 0u;
}
