/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "crc.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <string.h>

#define SYST_CSR  (*(volatile uint32_t*)0xE000E010)
#define SYST_RVR  (*(volatile uint32_t*)0xE000E014)
#define SYST_CVR  (*(volatile uint32_t*)0xE000E018)
#define SESSIONSTATUS_ADDR 					0x20004FC0
#define APPL_START_ADDRESS 					0x08003C00
#define APPL_END_ADDRESS     0x0800F7FFu    /* same as erase end */
#define APPL_CRC_ADDRESS     (APPL_END_ADDRESS - 3u) /* last 4 bytes */
#define APPL_PARAMFLASH_START_ADDRESS 0x08003800u
#define APPL_PARAMFLASH_END_ADDRESS 0x08003BFFu

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef enum
{
	PROGRAMMING = 2U,
	HARDRESET = 5U,
	JUMPTOAPPL = 7U
}FBL_DSC_t;

volatile uint8_t g_run_ram_updater;
FBL_DSC_t FBL_DSC_State = JUMPTOAPPL;
uint32* FBL_DSC_Pointer = (uint32*)SESSIONSTATUS_ADDR;
uint32 FBL_DSC_Status;
extern uint8_t __ram_blob_load_start[];
extern uint8_t __ram_blob_load_end[];
extern uint8_t __ram_blob_vma_start[];
extern uint8_t __ram_blob_vma_end[];

#define BOOT_START APPL_START_ADDRESS
#define BOOT_END APPL_CRC_ADDRESS
#define BOOT_BLOCK 1
#define BOOT_LEGACY_ERASE 4
#include "boot_protocol.h"
static Boot_Context boot;
static uint8_t FBL_AppVectorsValid(void);
void FBL_RunRamUpdater(void);
void FBL_JumpToAppl(void);
void FBL_DiagService_ER_HardReset(void);
void FBL_NvM_FlashReadData(uint32 StartSectorAddress, uint32 *RxBuf, uint16 numberofwords);
uint32 FBL_NvM_FlashWriteData(uint32 StartSectorAddress, uint32 *Data, uint16 numberofwords);
uint32 FBL_NvM_EraseFlash_APPL(void);
uint32 Nvm_GetPage(uint32 Address);
uint32 FBL_NvM_EraseFlash_PARAM(void);
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
void FBL_RunRamUpdater(void)
{
    uint32_t previousMask=__get_PRIMASK();
	__disable_irq();

	SYST_CSR = 0; SYST_RVR = 0; SYST_CVR = 0;

	uint8_t *src = __ram_blob_load_start;
	uint8_t *end = __ram_blob_load_end;
	uint8_t *dst = __ram_blob_vma_start;

	size_t size = (size_t)(end - src);
	/* Leave at least 1 KiB below the updater MSP at 0x20004F80. */
	if (((uintptr_t)dst < 0x20000000u) ||
		((uintptr_t)__ram_blob_vma_end > 0x20004B80u) ||
		(((uintptr_t)dst & 0x7Fu) != 0u) ||
		(size < 8u) ||
		(size != (size_t)(__ram_blob_vma_end - dst)))
	{
		__set_PRIMASK(previousMask);
		return;
	}
	memcpy(dst, src, size);

	uint32_t vtor     = (uint32_t)dst;
	uint32_t new_msp  = *(uint32_t *)(vtor + 0);
	uint32_t reset_pc = *(uint32_t *)(vtor + 4);
	if ((new_msp != 0x20004F80u) ||
		((reset_pc & 1u) == 0u) ||
		((reset_pc & ~1u) < vtor) ||
		((reset_pc & ~1u) >= (vtor + size)))
	{
		__set_PRIMASK(previousMask);
		return;
	}

	SCB->VTOR = vtor;
	__DSB();
	__ISB();

	__asm volatile("msr msp, %0\n bx %1"::"r"(new_msp),"r"(reset_pc):"memory");
	__builtin_unreachable();
}

typedef struct {uint8_t data[8],len;} BootRxFrame;
static BootRxFrame bootQueue[16];
static volatile uint8_t bootHead,bootTail;
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *handle)
{
    CAN_RxHeaderTypeDef h;uint8_t d[8];
    while(HAL_CAN_GetRxFifoFillLevel(handle,CAN_RX_FIFO0)){
        if(HAL_CAN_GetRxMessage(handle,CAN_RX_FIFO0,&h,d)!=HAL_OK)break;
        if(h.IDE!=CAN_ID_STD||h.RTR!=CAN_RTR_DATA||h.StdId!=BOOT_RX_ID||h.DLC>8)continue;
        uint8_t next=(bootHead+1u)%16u;if(next==bootTail)continue;
        for(unsigned i=0;i<h.DLC;++i)bootQueue[bootHead].data[i]=d[i];
        bootQueue[bootHead].len=(uint8_t)h.DLC;bootHead=next;
    }
}
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  Boot_Init(&boot);
  MX_GPIO_Init();
  MX_CAN_Init();
  MX_CRC_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */

	PWR_PVDTypeDef sConfigPVD;

	sConfigPVD.Mode = PWR_PVD_MODE_IT_FALLING;
	sConfigPVD.PVDLevel = PWR_PVDLEVEL_0;

	HAL_PWR_ConfigPVD(&sConfigPVD);

	HAL_PWR_EnablePVD();

	if(((RCC->CSR & RCC_CSR_PORRSTF) != 0))
	{
		for(uint32* addr = ((uint32*)0x20004fc0); addr <= ((uint32*)0x20004fff); addr++)
		{
			*addr = 0;
		}

		RCC->CSR |= RCC_CSR_PORRSTF;
		RCC->CSR |= RCC_CSR_RMVF;
	}
	else
	{
		/* Do nothing. */
	}

    /* A single explicit retained session word is shared with APP. The old
       0x20004FC8 heuristic aliases APP active-time data and is not a boot request. */
    FBL_DSC_Status=*FBL_DSC_Pointer;

	if ((FBL_DSC_Status == PROGRAMMING) || (FBL_AppVectorsValid() == 0u))
	{
		FBL_DSC_State = PROGRAMMING;
	}
	else
	{
		FBL_JumpToAppl();
	}

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

	while (1)
	{
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

        Boot_MainFunction(&boot,HAL_GetTick());
        if(bootTail!=bootHead){
            BootRxFrame frame=bootQueue[bootTail];bootTail=(bootTail+1u)%16u;
            BootTp_RxIndication(&boot,frame.data,frame.len,HAL_GetTick());
        }
		if (g_run_ram_updater)
		{
			g_run_ram_updater = 0;

			FBL_RunRamUpdater();
		}
		else
		{
			/* Do nothing. */
		}
	}

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* RCC_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(RCC_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(RCC_IRQn);
  /* FLASH_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(FLASH_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(FLASH_IRQn);
  /* PVD_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(PVD_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(PVD_IRQn);
  /* CAN1_SCE_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
  /* CAN1_RX1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
  /* USB_LP_CAN1_RX0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
  /* USB_HP_CAN1_TX_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(USB_HP_CAN1_TX_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USB_HP_CAN1_TX_IRQn);
}

/* USER CODE BEGIN 4 */
static uint8_t FBL_AppVectorsValid(void)
{
    uint32_t msp=*(const uint32_t*)APPL_START_ADDRESS,pc=*(const uint32_t*)(APPL_START_ADDRESS+4u);
    return msp>=0x20000000u && msp<=0x20004FC0u && !(msp&7u) && (pc&1u) &&
        (pc&~1u)>=APPL_START_ADDRESS && (pc&~1u)<APPL_CRC_ADDRESS &&
        *(const uint32_t*)APPL_CRC_ADDRESS==Boot_Crc((const uint8_t*)APPL_START_ADDRESS,APPL_CRC_ADDRESS-APPL_START_ADDRESS);
}
static int BootPort_Send(const uint8_t frame[8])
{
    CAN_TxHeaderTypeDef h={0};uint32_t mailbox,start=HAL_GetTick();
    h.StdId=BOOT_TX_ID;h.IDE=CAN_ID_STD;h.RTR=CAN_RTR_DATA;h.DLC=8;
    if(HAL_CAN_AddTxMessage(&hcan,&h,(uint8_t*)frame,&mailbox)!=HAL_OK)return -1;
    while(HAL_CAN_IsTxMessagePending(&hcan,mailbox))if((uint32_t)(HAL_GetTick()-start)>=100u){HAL_CAN_AbortTxRequest(&hcan,mailbox);return -1;}
    unsigned shift=mailbox==CAN_TX_MAILBOX0?0u:mailbox==CAN_TX_MAILBOX1?8u:16u;
    return (hcan.Instance->TSR&(CAN_TSR_TXOK0<<shift))?0:-1;
}
static int BootPort_Program(uint32_t address,uint32_t word)
{return FBL_NvM_FlashWriteData(address,&word,1)?-1:0;}
static int BootPort_Erase(uint32_t address,uint32_t size)
{
    FLASH_EraseInitTypeDef e={0};uint32_t error,mask=__get_PRIMASK();int rc=-1;
    if(!size || !((address>=APPL_START_ADDRESS && address<APPL_CRC_ADDRESS && size<=APPL_CRC_ADDRESS-address)||
       (address==APPL_PARAMFLASH_START_ADDRESS && size==1024u)))return -1;
    uint32_t end=(address+size+1023u)&~1023u;address&=~1023u;
    e.TypeErase=FLASH_TYPEERASE_PAGES;e.PageAddress=address;e.NbPages=(end-address)/1024u;
    __disable_irq();
    if(HAL_FLASH_Unlock()==HAL_OK && HAL_FLASHEx_Erase(&e,&error)==HAL_OK){
        rc=0;for(uint32_t a=address;a<end;a+=4u)if(*(volatile uint32_t*)a!=0xFFFFFFFFu)rc=-1;
    }
    HAL_FLASH_Lock();__set_PRIMASK(mask);return rc;
}
static uint32_t BootPort_Crc(uint32_t address,uint32_t size){return Boot_Crc((const uint8_t*)address,size);}
static int BootPort_Finalize(uint32_t address,uint32_t size)
{
    (void)address;(void)size;
    uint32_t sp=*(volatile uint32_t*)APPL_START_ADDRESS,pc=*(volatile uint32_t*)(APPL_START_ADDRESS+4u);
    if(sp<0x20000000u||sp>0x20004FC0u||(sp&7u)||!(pc&1u)||(pc&~1u)<APPL_START_ADDRESS||(pc&~1u)>=APPL_CRC_ADDRESS)return -1;
    uint32_t crc=BootPort_Crc(APPL_START_ADDRESS,APPL_CRC_ADDRESS-APPL_START_ADDRESS);
    if(*(volatile uint32_t*)APPL_CRC_ADDRESS==crc)return 0;
    return BootPort_Program(APPL_CRC_ADDRESS,crc);
}
static int BootPort_Reset(void)
{
    if(!FBL_AppVectorsValid())return -1;
    FBL_DiagService_ER_HardReset();return 0;
}
static void BootPort_Updater(void){g_run_ram_updater=1;}

uint32 FBL_NvM_EraseFlash_PARAM(void){return BootPort_Erase(APPL_PARAMFLASH_START_ADDRESS,1024u)?1u:0u;}
uint32 FBL_NvM_EraseFlash_APPL(void){return BootPort_Erase(APPL_START_ADDRESS,APPL_CRC_ADDRESS-APPL_START_ADDRESS)?1u:0u;}
uint32 Nvm_GetPage(uint32 address){return address>=0x08000000u && address<0x08010000u?address&~1023u:0u;}
uint32 FBL_NvM_FlashWriteData(uint32 address,uint32 *data,uint16 words)
{
    if(!data||!words||(address&3u)||address<APPL_START_ADDRESS||address>APPL_CRC_ADDRESS||4u*words>APPL_CRC_ADDRESS+4u-address)return 1;
    uint32_t mask=__get_PRIMASK(),result=0;__disable_irq();
    if(HAL_FLASH_Unlock()!=HAL_OK)result=1;
    for(unsigned i=0;i<words && !result;++i){
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,address+4u*i,data[i])!=HAL_OK || *(volatile uint32_t*)(address+4u*i)!=data[i])result=1;
    }
    HAL_FLASH_Lock();__set_PRIMASK(mask);return result;
}

void FBL_NvM_FlashReadData(uint32 address,uint32 *dst,uint16 words)
{
    if(!dst||address<0x08000000u||address>0x08010000u||4u*words>0x08010000u-address)return;
    for(unsigned i=0;i<words;++i)dst[i]=*(const uint32_t*)(address+4u*i);
}

void FBL_JumpToAppl(void)
{
	typedef void (*pFunction)(void);

	uint32 app_address = APPL_START_ADDRESS;
	pFunction app_entry;
	uint32 msp_value = *((uint32*)app_address);

	__disable_irq();

	HAL_NVIC_ClearPendingIRQ(CAN1_SCE_IRQn);
	HAL_NVIC_ClearPendingIRQ(CAN1_RX1_IRQn);
	HAL_NVIC_ClearPendingIRQ(USB_LP_CAN1_RX0_IRQn);
	HAL_NVIC_ClearPendingIRQ(USB_HP_CAN1_TX_IRQn);
	HAL_NVIC_ClearPendingIRQ(RCC_IRQn);
	HAL_NVIC_ClearPendingIRQ(FLASH_IRQn);
	HAL_NVIC_ClearPendingIRQ(PVD_IRQn);

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;
	SCB->VTOR = app_address;

	app_entry = (pFunction)(*((uint32*)(app_address + 4)));
	__DSB();__ISB();
	__asm volatile("msr msp, %0\n bx %1"::"r"(msp_value),"r"(app_entry):"memory");
	__builtin_unreachable();
}

void FBL_DiagService_ER_HardReset(void)
{
	__disable_irq();

	*FBL_DSC_Pointer = 0u;


	__NVIC_SystemReset();
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

	NVIC_SystemReset();

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
