#ifndef DCM_H
#define DCM_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "can.h"

#define DCM_RESP_ID_DELTA 0x01u

#define DCM_DID_FF_OCCURRENCE_COUNT   0xF300u
#define DCM_DID_FF_YEAR               0xF301u
#define DCM_DID_FF_MONTH              0xF302u
#define DCM_DID_FF_DAY                0xF303u
#define DCM_DID_FF_HOUR               0xF304u
#define DCM_DID_FF_MINUTE             0xF305u
#define DCM_DID_FF_SECOND             0xF306u
#define DCM_DID_FF_VEH_STATUS         0xF307u
#define DCM_DID_FF_L1_MV              0xF308u
#define DCM_DID_FF_T30_MV             0xF309u
#define DCM_DID_FF_NTC_C              0xF30Au
#define DCM_DID_FF_ISENSE_A           0xF30Bu

#define DCM_19_04_ALL_DTCS            0xFFFFFFu
#define DCM_19_04_ALL_RECORDS         0xFFu
#define DCM_19_04_MAX_PAYLOAD         640u

typedef enum
{
    DCM_NRC_GENERAL_REJECT                = 0x10u,
    DCM_NRC_INCORRECT_LENGTH             = 0x13u,
    DCM_NRC_CONDITIONS_NOT_CORRECT       = 0x22u,
    DCM_NRC_REQUEST_OUT_OF_RANGE         = 0x31u,
    DCM_NRC_SUBFUNCTION_NOT_SUPPORTED    = 0x12u,
    DCM_NRC_SERVICE_NOT_SUPPORTED        = 0x11u,
    DCM_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_SESSION = 0x7Eu,
    DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION     = 0x7Fu
} Dcm_Nrc_t;

typedef struct
{
    volatile uint8_t fc_pending;
    volatile uint8_t active;
    uint8_t fc_bytes[8];
    uint8_t stmin_ms_default;
    uint32_t fc_expect_id;
    uint32_t timeout_ms;
} iso_tx_ctx_t;

extern volatile uint8_t Dcm_RequestPending;
extern uint8_t rx[8u];
extern uint8_t Dcm_RxData[8u];
extern uint8_t Dcm_TxData[8u];
extern uint8_t Dcm_SWV[4u];
extern uint8_t Dcm_LoadStatus;
extern uint8_t Dcm_CC;
extern uint8_t Dcm_CDTCS;
extern uint32_t Dcm_SessionCounter;
extern uint32_t Dcm_ActiveSessionState;
extern uint32_t Dcm_MainCounter;
extern uint32_t Dcm_LoadTimer;
extern CAN_TxHeaderTypeDef Dcm_DiagTxHeader;
extern CAN_RxHeaderTypeDef Dcm_DiagRxHeader;

extern bool Dcm_IsoTp_RxHook(const CAN_RxHeaderTypeDef *rh, const uint8_t *data);
extern bool Dcm_IsoTp_Send(uint32_t req_canid, const uint8_t *payload, uint16_t len, uint8_t pad, uint8_t force_pad);
extern void Dcm_IsoTp_Config(uint8_t stmin_default_ms, uint32_t timeout_ms);
extern void Dcm_TesterPresent(void);
extern void Dcm_ExtendedSession(void);
extern void Dcm_DefaultSession(void);
extern void Dcm_LoadControl(void);
extern void Dcm_main(void);
extern void Dcm_ProgrammingSession(void);
extern void Dcm_HardReset(void);
extern void Dcm_ReadSWV(void);
extern void Dcm_RC_HealSupply(void);
extern void Dcm_ReadLoadStatus(void);
extern void Dcm_SendNrc(uint8_t nrc);
extern void Dcm_RDBI_ResetCounter(void);
extern void Dcm_RDBI_TimeActive(void);
extern void Dcm_RDBI_TimeWithoutReset(void);
extern void Dcm_RDBI_ResetData(void);
extern void Dcm_RDBI_CpuLoad(void);
extern void Dcm_CommunicationControl(void);
extern void Dcm_ControlDTCSetting(void);
extern void Dcm_CDTCI(void);
extern void Dcm_RDTCI(void);
extern void Dcm_RDBI_ReadActiveDiagnosticSession(void);
extern void Dcm_RDBI_ReadActiveSoftwareBlock(void);
extern HAL_StatusTypeDef Dcm_CanSendSF(uint32_t stdId, uint8_t *data, uint8_t len);

#endif /* DCM_H */
