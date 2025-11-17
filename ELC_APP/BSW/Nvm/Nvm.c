#include "Nvm.h"
#include "Dem.h"
#include "EcuM.h"
#include "Dcm.h"
#include "main.h"
#include <string.h>
#include "string.h"
#include "stdio.h"
#include <stdlib.h>
#include "crc.h"

#define BYTES_TO_WORDS(x)  ((uint16_t)((x) / 4u))  /* all your sizes are multiples of 4 */
#define NVM_LARGEST_BLOCK_SIZE sizeof(Dem_DTC_Stat)
#define NVM_DTC_START_ADDRESS 0x0800FC00
#define NVM_DTC_END_ADDRESS 0x0800FFFF
#define DFLASH_STARTING_ADDRESS     	0x0800FC00

extern uint8_t Dem_DTC_Stat[24u];

uint32_t Nvm_CurrentAddress;
uint32_t Nvm_SectorSwitchActivated;
uint32_t Nvm_CurrentSector;
Nvm_Header_t Nvm_HeaderArr[NVM_NO_BLOCKS];
Nvm_NvStat_t Nvm_NvStatArr[NVM_NO_BLOCKS];

Nvm_Header_t Nvm_HeaderArr_Default[NVM_NO_BLOCKS]=
{
		{0u, 0u, 0u, 0u}, // block 0 dummy not used
		{1u, 24u, 0u, 0u}
};

Nvm_NvStat_t Nvm_NvStatArr_Default[NVM_NO_BLOCKS] =
{
		{0u, 0u, 0u}, // block 0 dummy not used
		{24u, 0u, 0u,},
};

Nvm_Block_t Nvm_BlockDataList[NVM_NO_BLOCKS] =
{
		{0u, 0u}, // block 0 dummy not used
		{(uint32_t*)&Dem_DTC_Stat, 0u},
};

Nvm_Block_t Nvm_RomDefaults_BlockDataList[NVM_NO_BLOCKS] =
{
		{0u, 0u}, // block 0 dummy not used
		{(uint32_t*)&Dem_DTC_Stat, 0u},
};

uint8_t Nvm_BlockIdListForWriteAll[NVM_NO_BLOCKS] = {0u, 1u};
uint8_t Nvm_WriteAllFinished;
uint8_t Nvm_ReadAllFinished;

void Nvm_SectorSwitch(void);
void Nvm_WriteBlock(uint16_t blockId, uint32_t *data);
void Nvm_FindCurrentAddress();
void Nvm_ReadAll(void);
void Nvm_WriteAll(void);
uint32_t Nvm_GetPage(uint32_t Address);
void Nvm_FlashReadData(uint32_t StartPageAddress, uint32_t *RxBuf, uint16_t numberofwords);
uint32_t Nvm_FlashWriteData(uint32_t StartPageAddress, uint32_t *Data, uint16_t numberofwords);
uint32_t Nvm_Erase();

void Nvm_SectorSwitch(void)
{
	__disable_irq();

	uint32_t startPattern[2u] = {0u, 0u};

	Nvm_Erase();

	Nvm_CurrentAddress = DFLASH_STARTING_ADDRESS;
	startPattern[0u] = 0xA5A5A5A5u;
	startPattern[1u] = 0xA5A5A5A5u;

	Nvm_FlashWriteData(Nvm_CurrentAddress, startPattern, 2u);

	Nvm_CurrentAddress += 8u;

	for(uint8_t i = 1u; i < NVM_NO_BLOCKS; i++)
	{
		Nvm_WriteBlock(i, Nvm_BlockDataList[i].data);
	}

	__enable_irq();
}

void Nvm_WriteBlock(uint16_t blockId, uint32_t *data)
{
	__disable_irq();

	uint32_t address = Nvm_CurrentAddress;
	uint32_t crc;
	uint32_t crcPadded[2] = {0u, 0xFFFFFFFFu};
	uint32_t localMaxAddress = NVM_DTC_END_ADDRESS - NVM_LARGEST_BLOCK_SIZE;
	uint16_t sizeWords = (uint16_t)(Nvm_NvStatArr[blockId].blockSize / 4u);

	crc = HAL_CRC_Calculate(&hcrc, data, sizeWords);

	if ((address + NVM_SIZE_HEADER_BYTES +
			Nvm_NvStatArr[blockId].blockSize + 8u) < localMaxAddress)
	{
		/* header */
		Nvm_FlashWriteData(address,
				(uint32_t*)&Nvm_HeaderArr[blockId],
				BYTES_TO_WORDS(NVM_SIZE_HEADER_BYTES));
		address += NVM_SIZE_HEADER_BYTES;

		Nvm_NvStatArr[blockId].blockAddress = address;

		/* data */
		Nvm_FlashWriteData(address, data, sizeWords);
		address += Nvm_NvStatArr[blockId].blockSize;

		/* crc + padding (8 bytes = 2 words) */
		crcPadded[0u] = crc;
		Nvm_FlashWriteData(address, crcPadded, 2u);
		address += 8u;

		Nvm_CurrentAddress = address;
	}
	else
	{
		/* page full -> compact (erase + rewrite all) */
		Nvm_SectorSwitch();

		address = Nvm_CurrentAddress;

		Nvm_FlashWriteData(address,
				(uint32_t*)&Nvm_HeaderArr[blockId],
				BYTES_TO_WORDS(NVM_SIZE_HEADER_BYTES));
		address += NVM_SIZE_HEADER_BYTES;

		Nvm_NvStatArr[blockId].blockAddress = address;

		Nvm_FlashWriteData(address, data, sizeWords);
		address += Nvm_NvStatArr[blockId].blockSize;

		crcPadded[0u] = crc;
		Nvm_FlashWriteData(address, crcPadded, 2u);
		address += 8u;

		Nvm_CurrentAddress = address;
	}

	__enable_irq();
}

void Nvm_FindCurrentAddress()
{
	__disable_irq();

	uint32_t localAddress = Nvm_CurrentAddress;
	uint32_t keepOldLocalAddress = localAddress;
	uint32_t startPattern[2u] = {0u, 0u};
	uint32_t localMaxAddress = NVM_DTC_END_ADDRESS - NVM_LARGEST_BLOCK_SIZE;
	Nvm_Header_t localHeader;
	uint8_t localBlockId;

	if (0u == localAddress)
	{
		localAddress = DFLASH_STARTING_ADDRESS;
		Nvm_CurrentAddress = localAddress;
		keepOldLocalAddress = localAddress;
	}

	if (DFLASH_STARTING_ADDRESS == localAddress)
	{
		/* Read start pattern (2 words) */
		Nvm_FlashReadData(localAddress, startPattern, 2u);

		if (0xA5A5A5A5u == startPattern[0u] &&
				0xA5A5A5A5u == startPattern[1u])
		{
			localAddress += 8u;

			while ((localAddress + NVM_SIZE_HEADER_BYTES + 8u) < localMaxAddress)
			{
				Nvm_FlashReadData(localAddress,
						(uint32_t*)&localHeader,
						BYTES_TO_WORDS(NVM_SIZE_HEADER_BYTES));

				localBlockId = localHeader.blockId;

				if (0xFFu != localBlockId && NVM_NO_BLOCKS > localBlockId)
				{
					if (localHeader.blockId  == Nvm_HeaderArr_Default[localBlockId].blockId &&
							localHeader.blockSize == Nvm_HeaderArr_Default[localBlockId].blockSize)
					{
						Nvm_HeaderArr[localBlockId].blockId    = localHeader.blockId;
						Nvm_HeaderArr[localBlockId].blockSize  = localHeader.blockSize;
						Nvm_NvStatArr[localBlockId].blockSize  = localHeader.blockSize;
						Nvm_NvStatArr[localBlockId].blockAddress =
								localAddress + NVM_SIZE_HEADER_BYTES;

						/* jump over header + data + crc(8) */
						localAddress += NVM_SIZE_HEADER_BYTES +
								localHeader.blockSize + 8u;

						Nvm_CurrentAddress   = localAddress;
						keepOldLocalAddress  = localAddress;
					}
					else
					{
						localAddress += NVM_SIZE_HEADER_BYTES;
						Nvm_CurrentAddress = localAddress;
					}
				}
				else
				{
					/* uninitialized header -> end of used area */
					break;
				}
			}
		}
		else if (0xFFFFFFFFu != startPattern[0u] ||
				0xFFFFFFFFu != startPattern[1u])
		{
			/* garbage -> erase + reinit */
			Nvm_Erase();

			startPattern[0u] = 0xA5A5A5A5u;
			startPattern[1u] = 0xA5A5A5A5u;

			Nvm_FlashWriteData(localAddress, startPattern, 2u);

			localAddress       += 8u;
			Nvm_CurrentAddress  = localAddress;
			keepOldLocalAddress = localAddress;
		}
		else
		{
			/* fresh page */
			startPattern[0u] = 0xA5A5A5A5u;
			startPattern[1u] = 0xA5A5A5A5u;

			Nvm_FlashWriteData(localAddress, startPattern, 2u);

			localAddress       += 8u;
			Nvm_CurrentAddress  = localAddress;
			keepOldLocalAddress = localAddress;
		}
	}

	Nvm_CurrentAddress = keepOldLocalAddress;

	__enable_irq();
}

void Nvm_ReadAll(void)
{
	__disable_irq();

	uint32_t localCrc[2] = {0u};
	uint32_t compareCrc;
	uint32_t crcAddress;

	Nvm_FindCurrentAddress();

	for (uint8_t i = 1u; i < NVM_NO_BLOCKS; i++)
	{
		if ((Nvm_HeaderArr[i].blockId   != 0xFFu) &&
				(Nvm_HeaderArr[i].blockId   != 0u)    &&
				(Nvm_HeaderArr[i].blockSize != 0xFFu) &&
				(Nvm_HeaderArr[i].blockSize != 0u))
		{
			/* read data */
			Nvm_FlashReadData(Nvm_NvStatArr[i].blockAddress,
					(uint32_t*)Nvm_BlockDataList[i].data,
					(uint16_t)(Nvm_NvStatArr[i].blockSize / 4u));

			crcAddress = Nvm_NvStatArr[i].blockAddress +
					Nvm_NvStatArr[i].blockSize;

			/* read crc (2 words) */
			Nvm_FlashReadData(crcAddress, localCrc, 2u);

			compareCrc = HAL_CRC_Calculate(&hcrc,
					(uint32_t*)Nvm_BlockDataList[i].data,
					(uint32_t)(Nvm_NvStatArr[i].blockSize / 4u));

			if (compareCrc != localCrc[0u])
			{
				/* CRC fail -> load defaults into RAM + write back */
				memcpy((void*)Nvm_BlockDataList[i].data,
						(const void*)Nvm_RomDefaults_BlockDataList[i].data,
						Nvm_NvStatArr[i].blockSize);

				Nvm_WriteBlock(i, Nvm_BlockDataList[i].data);
			}
		}
		else
		{
			/* header invalid -> use defaults */
			memcpy(&Nvm_HeaderArr[i],
					&Nvm_HeaderArr_Default[i],
					sizeof(Nvm_HeaderArr[i]));

			memcpy(&Nvm_NvStatArr[i],
					&Nvm_NvStatArr_Default[i],
					sizeof(Nvm_NvStatArr[i]));

			memcpy((void*)Nvm_BlockDataList[i].data,
					(const void*)Nvm_RomDefaults_BlockDataList[i].data,
					Nvm_NvStatArr[i].blockSize);

			Nvm_WriteBlock(i, Nvm_BlockDataList[i].data);
		}
	}

	Nvm_ReadAllFinished = 2u;

	__enable_irq();
}

void Nvm_WriteAll(void)
{
	__disable_irq();

	for(uint8_t i = 0u; i < NVM_NO_BLOCKS; i++)
	{
		if(1u == Nvm_BlockIdListForWriteAll[i])
		{
			Nvm_WriteBlock(i, Nvm_BlockDataList[i].data);
		}
		else
		{
			/* Do nothing. */
		}
	}

	Nvm_WriteAllFinished = 2u;

	__enable_irq();
}

uint32_t Nvm_GetPage(uint32_t Address)
{
	for (uint8_t indx=0; indx < 128; indx++)
	{
		if((Address < (0x08000000 + (FLASH_PAGE_SIZE *(indx + 1))) ) && (Address >= (0x08000000 + FLASH_PAGE_SIZE * indx)))
		{
			return (0x08000000 + FLASH_PAGE_SIZE * indx);
		}
		else
		{
			/* Do nothing. */
		}
	}

	return 0;
}

uint32_t Nvm_Erase()
{
	static FLASH_EraseInitTypeDef EraseInitStruct;

	uint32_t PAGEError;

	uint32_t StartPage = Nvm_GetPage(NVM_DTC_START_ADDRESS);

	HAL_FLASH_Unlock();

	EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
	EraseInitStruct.PageAddress = StartPage;
	EraseInitStruct.NbPages     = 1;

	if (HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError) != HAL_OK)
	{
		return HAL_FLASH_GetError ();
	}
	else
	{
		/* Do nothing. */
	}

	HAL_FLASH_Lock();

	return 0;
}

uint32_t Nvm_FlashWriteData(uint32_t addr, uint32_t *data, uint16_t wordCount)
{
	uint32_t err;

	HAL_FLASH_Unlock();

	for (uint16_t i = 0; i < wordCount; i++)
	{
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data[i]) != HAL_OK)
		{
			err = HAL_FLASH_GetError();

			HAL_FLASH_Lock();

			return err;
		}
		else
		{
			/* Do nothing. */
		}

		addr += 4u;
	}

	HAL_FLASH_Lock();

	return 0u;
}

void Nvm_FlashReadData(uint32_t addr, uint32_t *rxBuf, uint16_t wordCount)
{
	for (uint16_t i = 0; i < wordCount; i++)
	{
		rxBuf[i] = *(__IO uint32_t *)(addr + (i * 4u));
	}
}
