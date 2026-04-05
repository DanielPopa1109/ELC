#ifndef DCM_H
#define DCM_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "can.h"

#define DCM_RESP_ID_DELTA 0x01u
#define DCM_APPL_END_ADDRESS     0x0800F7FFu    /* same as erase end */
#define DCM_APPL_CRC_ADDRESS     (DCM_APPL_END_ADDRESS - 3u) /* last 4 bytes */

typedef enum
{
    DCM_NRC_GENERAL_REJECT                = 0x10u,
    DCM_NRC_INCORRECT_LENGTH             = 0x13u,
    DCM_NRC_CONDITIONS_NOT_CORRECT       = 0x22u,
    DCM_NRC_REQUEST_OUT_OF_RANGE         = 0x31u,
    DCM_NRC_SECURITY_ACCESS_DENIED       = 0x33u,
    DCM_NRC_INVALID_KEY                  = 0x35u,
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
extern volatile uint8_t Dcm_IsoTp_TxActive;
extern uint8_t g_sec_seed[4u];
extern volatile uint8_t g_sec_level;
extern volatile uint8_t g_sec_unlocked;
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
extern uint32_t Dcm_TxMailbox;
extern uint32_t Dcm_LoadTimer;
extern CAN_TxHeaderTypeDef Dcm_DiagTxHeader;
extern CAN_RxHeaderTypeDef Dcm_DiagRxHeader;

extern uint8_t Dcm_OBD_VIN[17];
extern uint8_t Dcm_OBD_CALID[4u];
extern uint8_t Dcm_OBD_ECU_NAME[3u];
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
extern void Dcm_RDBI_SecureAccess(void);
extern void Dcm_RC_HealSupply(void);
extern void Dcm_RC_ReadHistograms(void);
extern void Dcm_ReadLoadStatus(void);
extern void Dcm_SendNrc(uint8_t nrc);
extern void Dcm_RDBI_ResetCounter(void);
extern void Dcm_RDBI_TimeInSleep(void);
extern void Dcm_RDBI_TimeActive(void);
extern void Dcm_RDBI_TimeWithoutReset(void);
extern void Dcm_RDBI_ResetData(void);
extern void Dcm_RDBI_CpuLoad(void);
extern void Dcm_CommunicationControl(void);
extern void Dcm_ControlDTCSetting(void);
extern void Dcm_CDTCI(void);
extern void Dcm_RDTCI(void);
extern void Dcm_RequestSeed(void);
extern void Dcm_SendKey(void);
extern void Dcm_RDBI_ReadActiveDiagnosticSession(void);
extern void Dcm_RDBI_ReadActiveSoftwareBlock(void);
extern void Dcm_RDBI_WakeupLine_Wakeups(void);
extern void Dcm_RDBI_CAN_Wakeups(void);
extern void Dcm_RDBI_DiagWakeups(void);
extern void Dcm_RDBI_T30Min10s(void);
extern void Dcm_RDBI_T30Max10s(void);
extern void Dcm_RDBI_T30Avg10s(void);
extern void Dcm_RDBI_L1Min10s(void);
extern void Dcm_RDBI_L1Max10s(void);
extern void Dcm_RDBI_L1Avg10s(void);
extern void Dcm_RDBI_L1_Current_Min10s(void);
extern void Dcm_RDBI_L1_Current_Max10s(void);
extern void Dcm_RDBI_L1_Current_Avg10s(void);
extern void Dcm_RDBI_T30MinSW(void);
extern void Dcm_RDBI_T30MaxSW(void);
extern void Dcm_RDBI_T30AvgSW(void);
extern void Dcm_RDBI_L1MinSW(void);
extern void Dcm_RDBI_L1MaxSW(void);
extern void Dcm_RDBI_L1AvgSW(void);
extern void Dcm_RDBI_L1_Current_MinSW(void);
extern void Dcm_RDBI_L1_Current_MaxSW(void);
extern void Dcm_RDBI_L1_Current_AvgSW(void);
extern void Dcm_HandleOBD(void);
extern void Dcm_OBD_Mode01(void);
extern void Dcm_OBD_Mode02(void);
extern void Dcm_OBD_Mode03(void);
extern void Dcm_OBD_Mode04(void);
extern void Dcm_OBD_Mode06(void);
extern void Dcm_OBD_Mode07(void);
extern void Dcm_OBD_Mode08(void);
extern void Dcm_OBD_Mode09(void);
extern void Dcm_OBD_Mode0A(void);
extern void Dcm_RC_ReadMinMaxAvg(void);
extern uint32_t GenKeyFromSeed32(uint32_t seed32, uint32_t level);
extern void SecCalcKeyFromSeed(const uint8_t *seedBytes, uint8_t *keyBytes, uint8_t seedLen, uint8_t level);
extern HAL_StatusTypeDef Dcm_CanSendSF(uint32_t stdId, uint8_t *data, uint8_t len);

#endif /* DCM_H */
