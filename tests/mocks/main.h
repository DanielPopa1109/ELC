#ifndef ELC_TEST_MAIN_H
#define ELC_TEST_MAIN_H
#include <stdint.h>
typedef int HAL_StatusTypeDef;
#define HAL_OK 0
#define HAL_ERROR 1
typedef struct {uint32_t ESR,TSR;} CAN_TypeDef;
typedef struct {CAN_TypeDef *Instance;} CAN_HandleTypeDef;
typedef struct {uint32_t StdId,ExtId,IDE,RTR,DLC,TransmitGlobalTime;} CAN_TxHeaderTypeDef;
typedef struct {uint32_t TypeErase,PageAddress,NbPages;} FLASH_EraseInitTypeDef;
#define HAL_CAN_STATE_LISTENING 1
#define CAN_ESR_BOFF 4u
#define CAN_TSR_TXOK0 2u
#define CAN_TSR_RQCP0 1u
#define CAN_TX_MAILBOX0 1u
#define CAN_TX_MAILBOX1 2u
#define CAN_ID_STD 0
#define CAN_RTR_DATA 0
#define FLASH_TYPEERASE_PAGES 0
#define FLASH_TYPEPROGRAM_WORD 0
extern uint32_t mockPrimask;
static inline uint32_t __get_PRIMASK(void){return mockPrimask;}
static inline void __disable_irq(void){mockPrimask=1;}
static inline void __set_PRIMASK(uint32_t v){mockPrimask=v;}
uint32_t HAL_GetTick(void);
int HAL_CAN_GetState(CAN_HandleTypeDef *h);
int HAL_CAN_IsTxMessagePending(CAN_HandleTypeDef *h,uint32_t mailbox);
int HAL_CAN_AddTxMessage(CAN_HandleTypeDef *h,CAN_TxHeaderTypeDef *header,uint8_t *data,uint32_t *mailbox);
int HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *h,uint32_t mailbox);
int HAL_FLASH_Unlock(void);
int HAL_FLASH_Lock(void);
int HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase,uint32_t *error);
int HAL_FLASH_Program(uint32_t type,uint32_t address,uint64_t data);
#endif
