#include "Can.h"
#include "../../../Inc/can.h"
#include "../CanIf/CanIf.h"
#include <string.h>
typedef struct {uint16_t id;uint8_t busy;uint32_t started;} Mailbox;
typedef struct {uint16_t id;uint8_t len,data[8];} Rx;
static Mailbox boxes[3];static Rx queue[16];static volatile uint8_t head,tail;
volatile uint32_t Can_RxOverflow,Can_TxErrors;
void Can_Init(void){memset(boxes,0,sizeof boxes);head=tail=0;Can_RxOverflow=Can_TxErrors=0;}
uint8_t Can_IsControllerOnline(void){return HAL_CAN_GetState(&hcan)==HAL_CAN_STATE_LISTENING && !(hcan.Instance->ESR&CAN_ESR_BOFF);}
int Can_Write(uint16_t id,const uint8_t *data,uint8_t len)
{
    CAN_TxHeaderTypeDef h={0};uint32_t mb,mask;int rc=-1;
    if(!data||id>0x7FFu||len>8u||!Can_IsControllerOnline())return -1;
    mask=__get_PRIMASK();__disable_irq();
    for(unsigned i=0;i<3;++i)if(boxes[i].busy && boxes[i].id==id)goto done;
    for(unsigned i=0;i<3;++i)if(boxes[i].busy && !HAL_CAN_IsTxMessagePending(&hcan,1u<<i))goto done;
    h.StdId=id;h.IDE=CAN_ID_STD;h.RTR=CAN_RTR_DATA;h.DLC=len;
    if(HAL_CAN_AddTxMessage(&hcan,&h,(uint8_t*)data,&mb)==HAL_OK){
        unsigned i=(mb==CAN_TX_MAILBOX0)?0u:(mb==CAN_TX_MAILBOX1)?1u:2u;
        boxes[i].id=id;boxes[i].busy=1;boxes[i].started=HAL_GetTick();rc=0;
    }
done:__set_PRIMASK(mask);return rc;
}
void Can_QueueRx(uint16_t id,const uint8_t *data,uint8_t len)
{
    if(!data||len>8)return;
    uint32_t mask=__get_PRIMASK();__disable_irq();uint8_t next=(head+1u)%16u;
    if(next==tail){Can_RxOverflow++;__set_PRIMASK(mask);return;}
    queue[head].id=id;queue[head].len=len;memcpy(queue[head].data,data,len);head=next;__set_PRIMASK(mask);
}
void Can_MainFunction(void)
{
    for(unsigned i=0;i<3;++i)if(boxes[i].busy){
        uint32_t mask=__get_PRIMASK();__disable_irq();uint32_t tsr=hcan.Instance->TSR,shift=8u*i;
        int pending=HAL_CAN_IsTxMessagePending(&hcan,1u<<i);
        int success=!pending && ((tsr&(CAN_TSR_TXOK0<<shift))!=0);
        if(!pending || !Can_IsControllerOnline() || (uint32_t)(HAL_GetTick()-boxes[i].started)>=100u){
            uint16_t id=boxes[i].id;if(pending)HAL_CAN_AbortTxRequest(&hcan,1u<<i);
            if(!HAL_CAN_IsTxMessagePending(&hcan,1u<<i)){
                hcan.Instance->TSR=CAN_TSR_RQCP0<<shift;boxes[i].busy=0;
                __set_PRIMASK(mask);if(!success)Can_TxErrors++;
                CanIf_TxConfirmation(id,success?0:-1);continue;
            }
        }
        __set_PRIMASK(mask);
    }
    for(unsigned n=0;n<16u && tail!=head;++n){
        uint32_t mask=__get_PRIMASK();__disable_irq();Rx item=queue[tail];tail=(tail+1u)%16u;__set_PRIMASK(mask);
        CanIf_RxIndication(item.id,item.data,item.len);CanH_RxIndication(item.id,item.data,item.len);
    }
}
