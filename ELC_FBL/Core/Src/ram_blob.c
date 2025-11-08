#include <stdint.h>
/* Put code in .ram_blob_text, data in .ram_blob_data, vectors in .ram_blob_vectors */
#define RAM_CODE  __attribute__((section(".ram_blob_text"))) __attribute__((noinline))
#define RAM_DATA  __attribute__((section(".ram_blob_data")))
#define RAM_VECT  __attribute__((section(".ram_blob_vectors"))) __attribute__((used))

#define NVIC_ISER0          (*(volatile uint32_t*)0xE000E100UL)
#define IRQN_FLASH          4U

#define FLASH_SR_PGERR      (1U << 2)
#define FLASH_SR_WRPRTERR   (1U << 4)
#define FLASH_SR_EOP        (1U << 5)

#define FLASH_CR_ERRIE      (1U << 10)
#define FLASH_CR_EOPIE      (1U << 12)
RAM_CODE void ram_updater_start(void);
RAM_CODE void RAM_FLASH_IRQHandler(void);

static RAM_CODE void default_irq(void) { for(;;){} }
static RAM_CODE void ram_reset(void)   { ram_updater_start(); }

// status latches for debug/use in wait loops
RAM_DATA static volatile uint32_t flash_irq_sr = 0;
RAM_DATA static volatile uint32_t flash_irq_seen = 0;

// Align to at least 128 bytes per ARM VTOR rules
RAM_VECT const uint32_t ram_vectors[] __attribute__((aligned(128))) = {
		/*  0 */ 0x20004F80,                 /* MSP */
		/*  1 */ (uint32_t)ram_reset,        /* Reset */

		/* System exceptions */
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

		/* Peripheral IRQs */
		/* 16 */ (uint32_t)default_irq,      /* WWDG */
		/* 17 */ (uint32_t)default_irq,      /* PVD */
		/* 18 */ (uint32_t)default_irq,      /* TAMPER */
		/* 19 */ (uint32_t)default_irq,      /* RTC */
		/* 20 */ (uint32_t)RAM_FLASH_IRQHandler, /* FLASH */
		/* 21 */ (uint32_t)default_irq,      /* RCC */
		/* 22 */ (uint32_t)default_irq,      /* EXTI0 */
		/* 23 */ (uint32_t)default_irq,      /* EXTI1 */
		/* 24 */ (uint32_t)default_irq,      /* EXTI2 */
		/* 25 */ (uint32_t)default_irq,      /* EXTI3 */
		/* 26 */ (uint32_t)default_irq,      /* EXTI4 */
		/* ... add more as needed ... */
};

// -------------------- Device constants --------------------
#define PERIPH_BASE        0x40000000UL
#define APB1PERIPH_BASE    (PERIPH_BASE + 0x00000000UL)
#define APB2PERIPH_BASE    (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE     (PERIPH_BASE + 0x00020000UL)

#define RCC_BASE           (AHBPERIPH_BASE + 0x00001000UL)
#define RCC_APB1ENR        (*(volatile uint32_t *)(RCC_BASE + 0x1C))
#define RCC_APB2ENR        (*(volatile uint32_t *)(RCC_BASE + 0x18))

#define GPIOA_BASE         (APB2PERIPH_BASE + 0x00000800UL)
#define GPIOB_BASE         (APB2PERIPH_BASE + 0x00000C00UL)
#define GPIO_CRL(base)     (*(volatile uint32_t *)((base) + 0x00))
#define GPIO_CRH(base)     (*(volatile uint32_t *)((base) + 0x04))
#define GPIO_IDR(base)     (*(volatile uint32_t *)((base) + 0x08))
#define GPIO_ODR(base)     (*(volatile uint32_t *)((base) + 0x0C))
#define GPIO_BSRR(base)    (*(volatile uint32_t *)((base) + 0x10))

#define AFIO_BASE          (APB2PERIPH_BASE + 0x00000000UL)
#define AFIO_MAPR          (*(volatile uint32_t *)(AFIO_BASE + 0x04))

#define CAN1_BASE          (APB1PERIPH_BASE + 0x00006400UL)
#define CAN_MCR            (*(volatile uint32_t *)(CAN1_BASE + 0x000))
#define CAN_MSR            (*(volatile uint32_t *)(CAN1_BASE + 0x004))
#define CAN_TSR            (*(volatile uint32_t *)(CAN1_BASE + 0x008))
#define CAN_RF0R           (*(volatile uint32_t *)(CAN1_BASE + 0x00C))
#define CAN_IER            (*(volatile uint32_t *)(CAN1_BASE + 0x014))
#define CAN_ESR            (*(volatile uint32_t *)(CAN1_BASE + 0x018))
#define CAN_BTR            (*(volatile uint32_t *)(CAN1_BASE + 0x01C))

#define CAN_TI0R           (*(volatile uint32_t *)(CAN1_BASE + 0x180)) // TIR
#define CAN_TDT0R          (*(volatile uint32_t *)(CAN1_BASE + 0x184)) // TDTR
#define CAN_TDL0R          (*(volatile uint32_t *)(CAN1_BASE + 0x188))
#define CAN_TDH0R          (*(volatile uint32_t *)(CAN1_BASE + 0x18C))

#define CAN_RI0R           (*(volatile uint32_t *)(CAN1_BASE + 0x1B0)) // RIR
#define CAN_RDT0R          (*(volatile uint32_t *)(CAN1_BASE + 0x1B4)) // RDTR
#define CAN_RDL0R          (*(volatile uint32_t *)(CAN1_BASE + 0x1B8))
#define CAN_RDH0R          (*(volatile uint32_t *)(CAN1_BASE + 0x1BC))

#define CAN_FMR            (*(volatile uint32_t *)(CAN1_BASE + 0x200))
#define CAN_FM1R           (*(volatile uint32_t *)(CAN1_BASE + 0x204))
#define CAN_FS1R           (*(volatile uint32_t *)(CAN1_BASE + 0x20C))
#define CAN_FFA1R          (*(volatile uint32_t *)(CAN1_BASE + 0x214))
#define CAN_FA1R           (*(volatile uint32_t *)(CAN1_BASE + 0x21C))
#define CAN_FiR1(i)        (*(volatile uint32_t *)(CAN1_BASE + 0x240 + (i)*0x08))
#define CAN_FiR2(i)        (*(volatile uint32_t *)(CAN1_BASE + 0x244 + (i)*0x08))

#define FLASH_BASE         (AHBPERIPH_BASE + 0x00002000UL)
#define FLASH_ACR          (*(volatile uint32_t *)(FLASH_BASE + 0x00))
#define FLASH_KEYR         (*(volatile uint32_t *)(FLASH_BASE + 0x04))
#define FLASH_SR           (*(volatile uint32_t *)(FLASH_BASE + 0x0C))
#define FLASH_CR           (*(volatile uint32_t *)(FLASH_BASE + 0x10))
#define FLASH_AR           (*(volatile uint32_t *)(FLASH_BASE + 0x14))

#define FLASH_KEY1         0x45670123UL
#define FLASH_KEY2         0xCDEF89ABUL
#define FLASH_SR_BSY       (1U << 0)
#define FLASH_SR_EOP       (1U << 5)
#define FLASH_CR_PG        (1U << 0)
#define FLASH_CR_PER       (1U << 1)
#define FLASH_CR_MER       (1U << 2)
#define FLASH_CR_STRT      (1U << 6)
#define FLASH_CR_LOCK      (1U << 7)

#define AIRCR              (*(volatile uint32_t *)0xE000ED0CUL)
#define AIRCR_VECTKEY      (0x5FA << 16)
#define AIRCR_SYSRESETREQ  (1U << 2)

#define SCB_VTOR           (*(volatile uint32_t *)0xE000ED08UL)
#define __disable_irq_()   __asm volatile ("cpsid i")
#define __enable_irq_()    __asm volatile ("cpsie i")

// -------------------- UDS and memory parameters --------------------
#define SESSIONSTATUS_ADDR 0x20004FC0UL
#define FLASH_START_ADDR   0x08000000UL
#define FLASH_END_ADDR     0x08003FFF
#define FLASH_PAGE_SIZE    1024U

// -------------------- RAM state --------------------
RAM_DATA static volatile uint32_t *const dsc_status_ptr = (uint32_t *)SESSIONSTATUS_ADDR;
RAM_DATA static uint32_t prog_addr = 0;
RAM_DATA static uint32_t prog_index = 0;
RAM_DATA static uint8_t  rx[8];
RAM_DATA static uint8_t  tx[8];
RAM_DATA static uint16_t last_rx_dlc = 0;
RAM_DATA static uint16_t last_rx_id  = 0;


// -------------------- Small delays --------------------
RAM_CODE void spin(volatile uint32_t n) { while(n--) { __asm volatile("nop"); } }


// -------------------- FLASH primitives --------------------
RAM_CODE void RAM_FLASH_IRQHandler(void)
{
	uint32_t sr = FLASH_SR;
	flash_irq_sr = sr;
	// clear all handled flags
	if (sr & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP)) {
		FLASH_SR = (FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP);
	}
	flash_irq_seen = 1;
}
#define NVIC_ICER0 (*(volatile uint32_t*)0xE000E180)
static RAM_CODE void flash_irq_enable(void)
{
	// route vectors to RAM table
	SCB_VTOR = (uint32_t)ram_vectors;
	__asm volatile("dsb");
	__asm volatile("isb");

	// clear stale flags, enable IRQ sources, unmask in NVIC
	FLASH_SR  = FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP;
	FLASH_CR |= FLASH_CR_ERRIE | FLASH_CR_EOPIE;
	NVIC_ICER0 = 0xFFFFFFFF;              // mask all IRQs
	NVIC_ISER0 = (1u << IRQN_FLASH);      // enable only FLASH
	__enable_irq_();
}
static RAM_CODE void flash_wait(void) {
	while (FLASH_SR & FLASH_SR_BSY) { }
	if (FLASH_SR & FLASH_SR_EOP) FLASH_SR = FLASH_SR_EOP; // clear EOP
	for (volatile int i = 0; i < 2000; i++);
}

static RAM_CODE void flash_unlock(void) {
	if (FLASH_CR & FLASH_CR_LOCK) {
		FLASH_KEYR = FLASH_KEY1;
		FLASH_KEYR = FLASH_KEY2;
	}
}

static RAM_CODE void flash_lock(void) {
	FLASH_CR |= FLASH_CR_LOCK;
}

static RAM_CODE void flash_page_erase(uint32_t addr) {
	flash_wait();
	FLASH_CR |= FLASH_CR_PER;
	FLASH_AR  = addr;
	FLASH_CR |= FLASH_CR_STRT;
	flash_wait();
	FLASH_CR &= ~FLASH_CR_PER;
}
static RAM_CODE void flash_mass_erase_range(uint32_t start, uint32_t end) {
	flash_unlock();
	for (uint32_t a = start; a < end; a += FLASH_PAGE_SIZE) flash_page_erase(a);
	flash_lock();
}

static RAM_CODE int flash_program_hword(uint32_t addr, uint16_t half)
{
	if (addr & 1U)                    return -3;           // misaligned
	if (addr < FLASH_START_ADDR)      return -4;
	if (addr >= FLASH_END_ADDR)       return -5;
	// optional blank check
	if (*(volatile uint16_t*)addr != 0xFFFF) return -6;    // not erased

	while (FLASH_SR & FLASH_SR_BSY);
	// clear stale status before starting
	FLASH_SR = FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP;

	FLASH_CR |= FLASH_CR_PG;
	*(volatile uint16_t*)addr = half;

	while (FLASH_SR & FLASH_SR_BSY);

	if (FLASH_SR & FLASH_SR_PGERR)    { FLASH_SR = FLASH_SR_PGERR;    FLASH_CR &= ~FLASH_CR_PG; return -7; }
	if (FLASH_SR & FLASH_SR_WRPRTERR) { FLASH_SR = FLASH_SR_WRPRTERR; FLASH_CR &= ~FLASH_CR_PG; return -8; }

	FLASH_SR = FLASH_SR_EOP;  // acknowledge end of operation
	FLASH_CR &= ~FLASH_CR_PG;
	return 0;
}

static RAM_CODE int flash_program_word_strict(uint32_t addr, uint32_t word)
{
	__disable_irq_();
	flash_unlock();
	int r = flash_program_hword(addr,     (uint16_t)(word & 0xFFFF));
	if (r == 0) r = flash_program_hword(addr + 2, (uint16_t)(word >> 16));
	flash_lock();
	__enable_irq_();
	return r;  // 0 on success
}

static RAM_CODE int can_rx_poll(uint16_t *stdid, uint8_t *dlc, uint8_t data[8]) {
	if ((CAN_RF0R & 0x03) == 0) return 0;
	uint32_t RIR   = CAN_RI0R;
	uint32_t RDTTR = CAN_RDT0R;
	uint32_t RDLR  = CAN_RDL0R;
	uint32_t RDHR  = CAN_RDH0R;
	*stdid = (uint16_t)((RIR >> 21) & 0x7FF);
	*dlc   = (uint8_t)(RDTTR & 0x0F);
	((uint32_t*)data)[0] = RDLR;
	((uint32_t*)data)[1] = RDHR;
	CAN_RF0R |= (1U<<5); // RFOM0: release FIFO0 output mailbox
	return 1;
}
static RAM_CODE void can_tx_blocking(uint16_t stdid, uint8_t dlc, const uint8_t data[8]) {
	// Wait for mailbox empty
	while (!(CAN_TSR & (1U<<26))) {} // TME0
	CAN_TDL0R = ((uint32_t)data[3]<<24) | ((uint32_t)data[2]<<16) | ((uint32_t)data[1]<<8) | data[0];
	CAN_TDH0R = ((uint32_t)data[7]<<24) | ((uint32_t)data[6]<<16) | ((uint32_t)data[5]<<8) | data[4];
	CAN_TDT0R = (dlc & 0x0F);
	CAN_TI0R  = ((uint32_t)stdid << 21) | (1U<<0); // STID + TXRQ
	// Optional wait sent
	while (!(CAN_TSR & (1U<<0))) {} // RQCP0
	CAN_TSR = (1U<<0);              // clear RQCP0
}

// -------------------- UDS handlers --------------------
static RAM_CODE void uds_send_pos(uint16_t rx_id, const uint8_t req[8], uint8_t extra_len_copy) {
	for (int i=0;i<8;i++) tx[i]=0;
	tx[0] = req[0];
	tx[1] = req[1] + 0x40;
	tx[2] = req[2];
	if (extra_len_copy) {
		for (int i=3;i<8;i++) tx[i] = req[i];
	}
	can_tx_blocking(rx_id + 1, 8, tx);
}

static RAM_CODE void handle_0x10(const uint8_t d[8], uint16_t rxid) {
	// Diagnostic Session Control: 0x10 0x02
	if (d[1]==0x10 && d[2]==0x02 && d[0] == 0x02) {
		*dsc_status_ptr = 2; // PROGRAMMING
		uds_send_pos(rxid, d, 0);
	}
}
static RAM_CODE void handle_0x31(const uint8_t d[8], uint16_t rxid) {
	// RoutineControl 0x31 01 04 -> erase
	if (d[4]==0x06 && d[0] == 0x04 && d[1] == 0x31 && d[2] == 0 && d[3] == 0 ) {
		uds_send_pos(rxid, d, 1); // echo routine id
		spin(10000);
		flash_mass_erase_range(FLASH_START_ADDR, FLASH_END_ADDR);
	}
}
static RAM_CODE void handle_0x34(const uint8_t d[8], uint16_t rxid) {
	if (d[1]==0x34 && d[0] == 0x07) {
		// Same address packing: (0x08<<24) | byte4..6
		prog_addr = (0x08UL<<24) | ((uint32_t)d[4]<<16) | ((uint32_t)d[5]<<8) | d[6];
		prog_index = 0;
		uds_send_pos(rxid, d, 1);
	}
}
uint32_t word;
static RAM_CODE void handle_0x36(const uint8_t d[8], uint16_t rxid) {
	if (d[1] == 0x36 && d[0] == 0x06){
		uint32_t w = ((uint32_t)d[6]<<24) | ((uint32_t)d[5]<<16) | ((uint32_t)d[4]<<8) | d[3];
		uds_send_pos(rxid, d, 0);             // keep your current positive if you want
		int rc = flash_program_word_strict(prog_addr, w);
		prog_addr  += 4;
		prog_index += 1;

		(void)rc;
	}
}
static RAM_CODE void handle_0x37(const uint8_t d[8], uint16_t rxid) {
	if (d[1]==0x37 && d[0]==0x01) {
		uds_send_pos(rxid, d, 0);
	}
}
static RAM_CODE void handle_0x11(const uint8_t d[8], uint16_t rxid) {
	if (d[1]==0x11 && d[2]==0x01) {
		uds_send_pos(rxid, d, 1);
		spin(90000);
		__disable_irq_();
		*dsc_status_ptr = 2;
		AIRCR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ;
		while(1) {}
	}
}

// -------------------- Entry --------------------
RAM_CODE void ram_updater_start(void) {
	__disable_irq_();                // we run polling
	flash_irq_enable();
	__enable_irq_();

	for (;;) {
		uint8_t dlc;
		uint16_t id;

		if (!can_rx_poll(&id, &dlc, rx)) continue;

		last_rx_id  = id;
		last_rx_dlc = dlc;

		// Minimal UDS state machine
		handle_0x10(rx, id);
		handle_0x31(rx, id);
		handle_0x34(rx, id);
		handle_0x36(rx, id);
		handle_0x37(rx, id);
		handle_0x11(rx, id);

		for(uint8_t i = 0; i < 8; i ++) rx[i] = 0u;
		for(uint8_t i = 0; i < 8; i ++) tx[i] = 0u;
	}
}
