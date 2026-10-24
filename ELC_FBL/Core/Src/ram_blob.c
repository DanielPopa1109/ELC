#include <stdint.h>

#define RAM_CODE  __attribute__((section(".ram_blob_text"))) __attribute__((noinline))
#define RAM_VECT  __attribute__((section(".ram_blob_vectors"))) __attribute__((used))
#define RAM_DATA  __attribute__((section(".ram_blob_data")))
#define __disable_irq_()   __asm volatile ("cpsid i")
#define __enable_irq_()    __asm volatile ("cpsie i")
#define SESSIONSTATUS_ADDR 0x20004FC0UL
#define FLASH_START_ADDR   0x08000000UL
#define FLASH_END_ADDR     0x080037FFUL
#define FLASH_PAGE_SIZE    1024U
#define PERIPH_BASE        0x40000000UL
#define APB1PERIPH_BASE    (PERIPH_BASE + 0x00000000UL)
#define AHBPERIPH_BASE     (PERIPH_BASE + 0x00020000UL)
#define FLASH_BASE         (AHBPERIPH_BASE + 0x00002000UL)
#define FLASH_KEYR         (*(volatile uint32_t *)(FLASH_BASE + 0x04))
#define FLASH_SR           (*(volatile uint32_t *)(FLASH_BASE + 0x0C))
#define FLASH_CR           (*(volatile uint32_t *)(FLASH_BASE + 0x10))
#define FLASH_AR           (*(volatile uint32_t *)(FLASH_BASE + 0x14))
#define FLASH_KEY1         0x45670123UL
#define FLASH_KEY2         0xCDEF89ABUL
#define FLASH_SR_BSY       (1U << 0)
#define FLASH_SR_PGERR     (1U << 2)
#define FLASH_SR_WRPRTERR  (1U << 4)
#define FLASH_SR_EOP       (1U << 5)
#define FLASH_CR_PG        (1U << 0)
#define FLASH_CR_PER       (1U << 1)
#define FLASH_CR_STRT      (1U << 6)
#define FLASH_CR_LOCK      (1U << 7)
#define AIRCR              (*(volatile uint32_t *)0xE000ED0CUL)
#define AIRCR_VECTKEY      (0x5FA << 16)
#define AIRCR_SYSRESETREQ  (1U << 2)
#define NVIC_ICER0         (*(volatile uint32_t*)0xE000E180UL)
#define SYST_CSR           (*(volatile uint32_t*)0xE000E010UL)
#define SYST_RVR           (*(volatile uint32_t*)0xE000E014UL)
#define SYST_CVR           (*(volatile uint32_t*)0xE000E018UL)
#define SCB_HFSR   (*(volatile uint32_t*)0xE000ED2CUL)
#define SCB_CFSR   (*(volatile uint32_t*)0xE000ED28UL)
#define SCB_BFAR   (*(volatile uint32_t*)0xE000ED38UL)
#define SCB_MMFAR  (*(volatile uint32_t*)0xE000ED34UL)
/* NCR is 0x20004FC0..0x20004FFF; keep fault words clear of the RAM blob. */
#define FAULT_DUMP_ADDR   ((volatile uint32_t*)0x20004FE0UL)
#define CAN1_BASE          (APB1PERIPH_BASE + 0x00006400UL)
#define CAN_TSR            (*(volatile uint32_t *)(CAN1_BASE + 0x008))
#define CAN_RF0R           (*(volatile uint32_t *)(CAN1_BASE + 0x00C))
#define CAN_TI0R           (*(volatile uint32_t *)(CAN1_BASE + 0x180)) // TIR
#define CAN_TDT0R          (*(volatile uint32_t *)(CAN1_BASE + 0x184)) // TDTR
#define CAN_TDL0R          (*(volatile uint32_t *)(CAN1_BASE + 0x188))
#define CAN_TDH0R          (*(volatile uint32_t *)(CAN1_BASE + 0x18C))
#define CAN_RI0R           (*(volatile uint32_t *)(CAN1_BASE + 0x1B0)) // RIR
#define CAN_RDT0R          (*(volatile uint32_t *)(CAN1_BASE + 0x1B4)) // RDTR
#define CAN_RDL0R          (*(volatile uint32_t *)(CAN1_BASE + 0x1B8))
#define CAN_RDH0R          (*(volatile uint32_t *)(CAN1_BASE + 0x1BC))

RAM_CODE void ram_updater_start(void);

static RAM_CODE void default_irq(void)
{
	volatile uint32_t *p = FAULT_DUMP_ADDR;

	p[0] = SCB_HFSR;
	p[1] = SCB_CFSR;
	p[2] = SCB_BFAR;
	p[3] = SCB_MMFAR;

	uint32_t msp;

	__asm volatile("mrs %0, msp" : "=r"(msp));

	p[4] = msp;

	for(;;) { }
}

static RAM_CODE void ram_reset(void)
{
	ram_updater_start();
}

RAM_VECT const uint32_t ram_vectors[] __attribute__((aligned(128))) =
{
		/*  0 */ 0x20004F80,                 /* MSP */
		/*  1 */ (uint32_t)ram_reset,        /* Reset */
		/*  2 */ (uint32_t)default_irq,      /* NMI */
		/*  3 */ (uint32_t)default_irq,      /* HardFault */
		/*  4 */ (uint32_t)default_irq,      /* MemManage */
		/*  5 */ (uint32_t)default_irq,      /* BusFault */
		/*  6 */ (uint32_t)default_irq,      /* UsageFault */
		/*  7 */ 0,
		/*  8 */ 0,
		/*  9 */ 0,
		/* 10 */ 0,
		/* 11 */ (uint32_t)default_irq,      /* SVCall */
		/* 12 */ (uint32_t)default_irq,      /* DebugMon */
		/* 13 */ 0,
		/* 14 */ (uint32_t)default_irq,      /* PendSV */
		/* 15 */ (uint32_t)default_irq,      /* SysTick */
		/* 16 */ (uint32_t)default_irq,      /* WWDG */
		/* 17 */ (uint32_t)default_irq,      /* PVD */
		/* 18 */ (uint32_t)default_irq,      /* TAMPER */
		/* 19 */ (uint32_t)default_irq,      /* RTC */
		/* 20 */ (uint32_t)default_irq,      /* FLASH */
		/* 21 */ (uint32_t)default_irq,      /* RCC */
		/* 22 */ (uint32_t)default_irq,      /* EXTI0 */
		/* 23 */ (uint32_t)default_irq,      /* EXTI1 */
		/* 24 */ (uint32_t)default_irq,      /* EXTI2 */
		/* 25 */ (uint32_t)default_irq,      /* EXTI3 */
		/* 26 */ (uint32_t)default_irq       /* EXTI4 */
};



static RAM_CODE int flash_wait_ready(void)
{
	uint32_t budget = 10000000u;
	while ((FLASH_SR & FLASH_SR_BSY) != 0u)
	{
		if (--budget == 0u) return -1;
	}
	return 0;
}

static RAM_CODE int flash_unlock(void)
{
	if (FLASH_CR & FLASH_CR_LOCK)
	{
		FLASH_KEYR = FLASH_KEY1;
		FLASH_KEYR = FLASH_KEY2;
	}
	else
	{
		/* Do nothing. */
	}
	return ((FLASH_CR & FLASH_CR_LOCK) == 0u) ? 0 : -1;
}

static RAM_CODE void flash_lock(void)
{
	FLASH_CR |= FLASH_CR_LOCK;
}

static RAM_CODE int flash_page_erase(uint32_t addr)
{
	if (flash_wait_ready() != 0) return -1;
	FLASH_SR = FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP;

	FLASH_CR |= FLASH_CR_PER;
	FLASH_AR  = addr;
	FLASH_CR |= FLASH_CR_STRT;

	int result = flash_wait_ready();

	FLASH_CR &= ~FLASH_CR_PER;
	if ((result != 0) || ((FLASH_SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0u)) return -1;
	FLASH_SR = FLASH_SR_EOP;
	for (uint32_t offset = 0u; offset < FLASH_PAGE_SIZE; offset += 4u)
	{
		if (*(volatile uint32_t *)(addr + offset) != 0xFFFFFFFFu) return -1;
	}
	return 0;
}

static RAM_CODE int flash_mass_erase_range(uint32_t start, uint32_t end)
{
	if (flash_unlock() != 0) return -1;

	for (uint32_t a = start; a < end; a += FLASH_PAGE_SIZE)
	{
		if (flash_page_erase(a) != 0)
		{
			flash_lock();
			return -1;
		}
	}

	flash_lock();
	return 0;
}

static RAM_CODE int flash_program_hword(uint32_t addr, uint16_t half)
{
	if (addr & 1U)
	{
		return -3;
	}
	else
	{
		/* Do nothing. */
	}

	if (addr < FLASH_START_ADDR)
	{
		return -4;
	}
	else
	{
		/* Do nothing. */
	}

	if (addr > FLASH_END_ADDR)
	{
		return -5;
	}
	else
	{
		/* Do nothing. */
	}

	if (*(volatile uint16_t*)addr != 0xFFFFu)
	{
		return -6;
	}
	else
	{
		/* Do nothing. */
	}

	if (flash_wait_ready() != 0) return -9;

	FLASH_SR = FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP;

	FLASH_CR |= FLASH_CR_PG;

	*(volatile uint16_t*)addr = half;

	if (flash_wait_ready() != 0)
	{
		FLASH_CR &= ~FLASH_CR_PG;
		return -9;
	}

	if (FLASH_SR & FLASH_SR_PGERR)
	{
		FLASH_SR = FLASH_SR_PGERR;
		FLASH_CR &= ~FLASH_CR_PG;
		return -7;
	}
	else
	{
		/* Do nothing. */
	}

	if (FLASH_SR & FLASH_SR_WRPRTERR)
	{
		FLASH_SR = FLASH_SR_WRPRTERR;
		FLASH_CR &= ~FLASH_CR_PG;
		return -8;
	}
	else
	{
		/* Do nothing. */
	}

	FLASH_SR = FLASH_SR_EOP;
	FLASH_CR &= ~FLASH_CR_PG;

	return (*(volatile uint16_t *)addr == half) ? 0 : -10;
}

static RAM_CODE int flash_program_word_strict(uint32_t addr, uint32_t word)
{
	if (flash_unlock() != 0) return -1;

	int r = flash_program_hword(addr,     (uint16_t)(word & 0xFFFFu));

	if (r == 0)
	{
		r = flash_program_hword(addr + 2u, (uint16_t)(word >> 16));
	}
	else
	{
		/* Do nothing. */
	}

	flash_lock();

	return r;
}

static RAM_CODE int can_rx_poll(uint16_t *stdid, uint8_t *dlc, uint8_t data[8])
{
	if ((CAN_RF0R & 0x03u) == 0u)
	{
		return 0;
	}
	else
	{
		/* Do nothing. */
	}

	uint32_t RIR   = CAN_RI0R;
	uint32_t RDTTR = CAN_RDT0R;
	uint32_t RDLR  = CAN_RDL0R;
	uint32_t RDHR  = CAN_RDH0R;

	*stdid = (uint16_t)((RIR >> 21) & 0x7FFu);
	*dlc   = (uint8_t)(RDTTR & 0x0Fu);

	data[0] = (uint8_t)(RDLR & 0xFFu);
	data[1] = (uint8_t)((RDLR >> 8) & 0xFFu);
	data[2] = (uint8_t)((RDLR >> 16) & 0xFFu);
	data[3] = (uint8_t)((RDLR >> 24) & 0xFFu);
	data[4] = (uint8_t)(RDHR & 0xFFu);
	data[5] = (uint8_t)((RDHR >> 8) & 0xFFu);
	data[6] = (uint8_t)((RDHR >> 16) & 0xFFu);
	data[7] = (uint8_t)((RDHR >> 24) & 0xFFu);

	CAN_RF0R |= (1U<<5); /* RFOM0 */

	return (RIR&6u)==0u;
}

#define BOOT_CODE RAM_CODE
#define BOOT_START FLASH_START_ADDR
#define BOOT_END (FLASH_END_ADDR+1u)
#define BOOT_BLOCK 2
#define BOOT_LEGACY_ERASE 6
#include "boot_protocol.h"
static RAM_DATA Boot_Context boot;
static RAM_CODE int BootPort_Send(const uint8_t frame[8])
{
    uint32_t count=1000000u;
    while(!(CAN_TSR&(1u<<26)))if(!--count)return -1;
    CAN_TSR=1u;
    CAN_TDL0R=((uint32_t)frame[3]<<24)|((uint32_t)frame[2]<<16)|((uint32_t)frame[1]<<8)|frame[0];
    CAN_TDH0R=((uint32_t)frame[7]<<24)|((uint32_t)frame[6]<<16)|((uint32_t)frame[5]<<8)|frame[4];
    CAN_TDT0R=8;CAN_TI0R=((uint32_t)BOOT_TX_ID<<21)|1u;count=1000000u;
    while(!(CAN_TSR&1u))if(!--count){CAN_TSR=1u<<7;return -1;}
    int rc=(CAN_TSR&2u)?0:-1;CAN_TSR=1u;return rc;
}
static RAM_CODE int BootPort_Program(uint32_t address,uint32_t word){return flash_program_word_strict(address,word);}
static RAM_CODE int BootPort_Erase(uint32_t address,uint32_t size)
{
    if(!Boot_Range(address,size))return -1;
    return flash_mass_erase_range(address&~1023u,(address+size+1023u)&~1023u);
}
static RAM_CODE uint32_t BootPort_Crc(uint32_t address,uint32_t size){return Boot_Crc((const uint8_t*)address,size);}
static RAM_CODE int BootPort_Finalize(uint32_t address,uint32_t size)
{
    uint32_t sp=*(volatile uint32_t*)FLASH_START_ADDR,pc=*(volatile uint32_t*)(FLASH_START_ADDR+4u);
    return sp>=0x20000000u && sp<=0x20004FC0u && !(sp&7u) && (pc&1u) && (pc&~1u)>=address && (pc&~1u)<address+size?0:-1;
}
static RAM_CODE int BootPort_Reset(void)
{
    *(volatile uint32_t*)SESSIONSTATUS_ADDR=0;
    AIRCR=AIRCR_VECTKEY|AIRCR_SYSRESETREQ;for(;;){}return 0;
}
static RAM_CODE void BootPort_Updater(void){__builtin_unreachable();} /* already in updater; protocol rejects re-entry */
RAM_CODE void ram_updater_start(void)
{
    __disable_irq_();
    *(volatile uint32_t*)0xE000E180u=0xFFFFFFFFu;
    *(volatile uint32_t*)0xE000E184u=0xFFFFFFFFu;
    *(volatile uint32_t*)0xE000E188u=0xFFFFFFFFu;
    SYST_CSR=0;SYST_RVR=0;SYST_CVR=0;
    *(volatile uint32_t*)0xE000EDFCu|=1u<<24; /* DWT cycle time, no Flash dependencies */
    *(volatile uint32_t*)0xE0001000u|=1u;
    uint32_t previous=*(volatile uint32_t*)0xE0001004u,now=0;
    Boot_Init(&boot);
    for(;;){
        uint32_t cycles=*(volatile uint32_t*)0xE0001004u;
        uint32_t elapsed=(uint32_t)(cycles-previous)/72000u;
        previous+=elapsed*72000u;now+=elapsed;
        Boot_MainFunction(&boot,now);
        uint8_t d[8],len;uint16_t id;
        if(can_rx_poll(&id,&len,d) && id==BOOT_RX_ID && len<=8u)BootTp_RxIndication(&boot,d,len,now);
    }
}
