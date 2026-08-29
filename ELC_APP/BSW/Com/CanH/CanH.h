#ifndef CANH_H
#define CANH_H

#include <stdint.h>
#include "can.h"
#include <stdbool.h>
#include "crc.h"
#include <string.h>
#include <stdlib.h>

#define XCP_CAN_RX_ID      0x7C0u
#define XCP_CAN_TX_ID      0x7C1u
#define XCP_PID_RES        0xFFu
#define XCP_PID_ERR        0xFEu
#define XCP_CMD_CONNECT            0xFFu
#define XCP_CMD_DISCONNECT         0xFEu
#define XCP_CMD_GET_STATUS         0xFDu
#define XCP_CMD_GET_COMM_MODE_INFO 0xFBu
#define XCP_CMD_GET_ID             0xFAu
#define XCP_CMD_SHORT_UPLOAD       0xF4u
#define XCP_CMD_UPLOAD             0xF5u
#define XCP_CMD_SET_MTA            0xF6u
#define XCP_ERR_OK             0x00u
#define XCP_ERR_CMD_UNKNOWN    0x20u
#define XCP_ERR_OUT_OF_RANGE   0x22u

typedef enum
{
	INIT = 0x00U,
	NO_COMMUNICATION = 0x01U,
	FULL_COMMUNICATION = 0x02U,
	PARTIAL_COMMUNICATION = 0x03U,
	CC_ACTIVE = 0x04U
}CanH_ComStat_t;

typedef struct
{
    uint8_t  connected;
    uint8_t  sessionStatus;
    uint8_t  mtaExt;
    uint32_t mta;
} Xcp_State_t;

extern Xcp_State_t Xcp_State;
extern uint8_t Xcp_TxData[8u];
extern CAN_TxHeaderTypeDef Xcp_TxHeader;
extern CanH_ComStat_t CanH_CommunicationState;
extern uint8_t CanH_NM3PN1_Value;
extern uint8_t CanH_RxData[8u];
extern uint8_t CanH_TxData[8u];
extern uint8_t CSBSDAT_Year;
extern uint8_t CSBSDAT_Month;
extern uint8_t CSBSDAT_Day;
extern uint8_t CSBSDAT_Hour;
extern uint8_t CSBSDAT_Minute;
extern uint8_t CSBSDAT_Second;
extern volatile uint8_t CanH_VehicleStatus;
extern volatile uint8_t CanH_AliveCounter_LoadStatus;
extern volatile uint8_t CanH_AliveCounter_LoadRequest;
extern CAN_RxHeaderTypeDef CanH_RxHeader;
extern CAN_TxHeaderTypeDef CanH_TxHeader;
extern uint32_t CanH_MainCounter;
extern uint32_t CanH_TxMailbox;
extern uint32_t CanH_NoCommCounter;
extern uint32_t CanH_MissingLoadRequest;
extern uint32_t CanH_MissingVehicleData;
extern uint32_t CanH_E2eErrCnt;
extern const uint32_t CanH_P_DelayTxParam;

extern void CanH_MainFunction(void);
extern bool Dcm_IsoTp_RxHook(const CAN_RxHeaderTypeDef *rh, const uint8_t *data);
extern void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);
extern void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan);

#endif /* CANH_H */
