/* Dcm_Cfg.h - Dcm limits, timing and UDS constants. */
#ifndef ELC_BSW_DCM_CFG_H
#define ELC_BSW_DCM_CFG_H

#include "Std_Types.h"

/* ---- Buffers and timing ------------------------------------------------------------- */
#define DCM_MAIN_PERIOD_MS        5u
#define DCM_RX_BUFFER_LEN         256u     /* largest request; CanTp answers FC.OVFLW above it */
#define DCM_TX_BUFFER_LEN         1152u    /* largest response (0x19 0x04 for every configured DTC) */

#define DCM_P2_SERVER_MS          50u      /* advertised in the session control response */
#define DCM_P2STAR_SERVER_MS      5000u
#define DCM_S3_SERVER_MS          5000u
/* NRC 0x78 is sent when a handler is still pending shortly before P2 expires and then renewed
 * before every P2* expiry. */
#define DCM_PENDING_FIRST_MS      40u
#define DCM_PENDING_RENEW_MS      4500u

/* Number of entries in Dcm_DefaultServices[] (Dcm.c). Keep in sync. */
#define DCM_DEFAULT_SERVICE_COUNT 9u

/* ---- Sessions (ISO 14229-1 table 25) -------------------------------------------------- */
#define DCM_SESSION_DEFAULT       1u
#define DCM_SESSION_PROGRAMMING   2u
#define DCM_SESSION_EXTENDED      3u
#define DCM_SESSION_MASK_DEFAULT  (1u << DCM_SESSION_DEFAULT)
#define DCM_SESSION_MASK_PROGRAMMING (1u << DCM_SESSION_PROGRAMMING)
#define DCM_SESSION_MASK_EXTENDED (1u << DCM_SESSION_EXTENDED)
#define DCM_SESSION_MASK_ALL      (DCM_SESSION_MASK_DEFAULT | DCM_SESSION_MASK_PROGRAMMING | DCM_SESSION_MASK_EXTENDED)

/* ---- Service identifiers ---------------------------------------------------------------- */
#define DCM_SID_DIAGNOSTIC_SESSION_CONTROL   0x10u
#define DCM_SID_ECU_RESET                    0x11u
#define DCM_SID_CLEAR_DIAGNOSTIC_INFORMATION 0x14u
#define DCM_SID_READ_DTC_INFORMATION         0x19u
#define DCM_SID_READ_DATA_BY_IDENTIFIER      0x22u
#define DCM_SID_COMMUNICATION_CONTROL        0x28u
#define DCM_SID_ROUTINE_CONTROL              0x31u
#define DCM_SID_TESTER_PRESENT               0x3Eu
#define DCM_SID_CONTROL_DTC_SETTING          0x85u
#define DCM_SUPPRESS_POS_RSP_MSG_INDICATION_BIT 0x80u

/* ---- Negative response codes (ISO 14229-1 annex A) ------------------------------------- */
#define DCM_NRC_GENERAL_REJECT                       0x10u
#define DCM_NRC_SERVICE_NOT_SUPPORTED                0x11u
#define DCM_NRC_SUBFUNCTION_NOT_SUPPORTED            0x12u
#define DCM_NRC_INCORRECT_LENGTH                     0x13u
#define DCM_NRC_RESPONSE_TOO_LONG                    0x14u
#define DCM_NRC_BUSY_REPEAT_REQUEST                  0x21u
#define DCM_NRC_CONDITIONS_NOT_CORRECT               0x22u
#define DCM_NRC_REQUEST_SEQUENCE_ERROR               0x24u
#define DCM_NRC_REQUEST_OUT_OF_RANGE                 0x31u
#define DCM_NRC_GENERAL_PROGRAMMING_FAILURE          0x72u
#define DCM_NRC_RESPONSE_PENDING                     0x78u
#define DCM_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_SESSION 0x7Eu
#define DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION     0x7Fu

#endif /* ELC_BSW_DCM_CFG_H */
