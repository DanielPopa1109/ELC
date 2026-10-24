#ifndef CANH_H
#define CANH_H

#include <stdint.h>
#include <stdbool.h>
#include "ComStack_Types.h"

/* CanH is the ELC application-side CAN handler: it feeds the Com signal layer
 * (LoadStatus, telemetry), consumes the Com RX signals for the SMon load logic
 * and hosts the XCP slave (CAN 0x7C0 / 0x7C1) and the time stamp PDU (0x202).
 * All bus access goes through CanIf/Com/PduR. */

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
extern CanH_ComStat_t CanH_CommunicationState;
extern uint8_t CanH_NM3PN1_Value;
extern uint8_t CSBSDAT_Year;
extern uint8_t CSBSDAT_Month;
extern uint8_t CSBSDAT_Day;
extern uint8_t CSBSDAT_Hour;
extern uint8_t CSBSDAT_Minute;
extern uint8_t CSBSDAT_Second;
extern volatile uint8_t CanH_VehicleStatus;
extern volatile uint8_t CanH_AliveCounter_LoadRequest;
extern uint32_t CanH_MainCounter;
extern uint32_t CanH_NoCommCounter;
extern uint32_t CanH_MissingLoadRequest;
extern uint32_t CanH_MissingVehicleData;
extern uint32_t CanH_E2eErrCnt;

void CanH_Init(void);
void Xcp_MainFunction(void);                                            /* 1 ms, with the CAN driver main functions */
void CanH_MainFunction(void);                                           /* 5 ms, after Com_MainFunctionRx */
void CanH_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr); /* from CanIf */
void CanH_TxConfirmation(PduIdType TxPduId);                              /* from CanIf */
void CanH_PublishMeasurements(void);

#endif /* CANH_H */
