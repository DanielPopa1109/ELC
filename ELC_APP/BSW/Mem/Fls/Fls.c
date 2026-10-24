#include "Fls.h"
#include "main.h"
#include <string.h>

static boolean Fls_InRegion(Fls_AddressType address, Fls_LengthType length, Fls_AddressType base, Fls_LengthType size)
{
    return (boolean)((address >= base) && (length <= size) && ((address - base) <= (size - length)));
}

Std_ReturnType Fls_Read(Fls_AddressType SourceAddress, uint8 *TargetAddressPtr, Fls_LengthType Length)
{
    if ((TargetAddressPtr == NULL_PTR) ||
        ((Fls_InRegion(SourceAddress, Length, FLS_NVM_BASE_ADDRESS, FLS_NVM_SIZE) == FALSE) &&
         (Fls_InRegion(SourceAddress, Length, FLS_CAL_BASE_ADDRESS, FLS_CAL_SIZE) == FALSE)))
    {
        return E_NOT_OK;
    }
    (void)memcpy(TargetAddressPtr, (const void *)SourceAddress, Length);
    return E_OK;
}

Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32 pageError;
    uint32 primask;
    uint32 a;
    Std_ReturnType rc = E_NOT_OK;

    if ((Length == 0u) || ((TargetAddress % FLS_PAGE_SIZE) != 0u) || ((Length % FLS_PAGE_SIZE) != 0u) ||
        (Fls_InRegion(TargetAddress, Length, FLS_NVM_BASE_ADDRESS, FLS_NVM_SIZE) == FALSE))
    {
        return E_NOT_OK;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    if (HAL_FLASH_Unlock() == HAL_OK)
    {
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.PageAddress = TargetAddress;
        erase.NbPages = Length / FLS_PAGE_SIZE;
        if (HAL_FLASHEx_Erase(&erase, &pageError) == HAL_OK)
        {
            rc = E_OK;
            for (a = TargetAddress; a < (TargetAddress + Length); a += 4u)
            {
                if (*(volatile uint32 *)a != 0xFFFFFFFFu)
                {
                    rc = E_NOT_OK;           /* blank check */
                }
            }
        }
    }
    (void)HAL_FLASH_Lock();
    __set_PRIMASK(primask);
    return rc;
}

Std_ReturnType Fls_Write(Fls_AddressType TargetAddress, const uint8 *SourceAddressPtr, Fls_LengthType Length)
{
    Fls_LengthType offset;

    if ((SourceAddressPtr == NULL_PTR) || (Length == 0u) || ((TargetAddress & 3u) != 0u) || ((Length & 3u) != 0u) ||
        (Fls_InRegion(TargetAddress, Length, FLS_NVM_BASE_ADDRESS, FLS_NVM_SIZE) == FALSE))
    {
        return E_NOT_OK;
    }
    for (offset = 0u; offset < Length; offset += 4u)
    {
        uint32 word;
        uint32 primask;
        Std_ReturnType rc = E_NOT_OK;

        (void)memcpy(&word, &SourceAddressPtr[offset], 4u);
        primask = __get_PRIMASK();
        __disable_irq();
        if ((HAL_FLASH_Unlock() == HAL_OK) &&
            (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, TargetAddress + offset, word) == HAL_OK) &&
            (*(volatile uint32 *)(TargetAddress + offset) == word))   /* readback */
        {
            rc = E_OK;
        }
        (void)HAL_FLASH_Lock();
        __set_PRIMASK(primask);
        if (rc != E_OK)
        {
            return E_NOT_OK;
        }
    }
    return E_OK;
}
