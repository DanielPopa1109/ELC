#ifndef DCM_H
#define DCM_H
#include <stdint.h>
#define DCM_19_04_MAX_PAYLOAD 1152u
#define DCM_DID_FF_OCCURRENCE_COUNT 0xF300u
#define DCM_DID_FF_YEAR 0xF301u
#define DCM_DID_FF_MONTH 0xF302u
#define DCM_DID_FF_DAY 0xF303u
#define DCM_DID_FF_HOUR 0xF304u
#define DCM_DID_FF_MINUTE 0xF305u
#define DCM_DID_FF_SECOND 0xF306u
#define DCM_DID_FF_VEH_STATUS 0xF307u
#define DCM_DID_FF_L1_MV 0xF308u
#define DCM_DID_FF_T30_MV 0xF309u
#define DCM_DID_FF_NTC_C 0xF30Au
#define DCM_DID_FF_ISENSE_A 0xF30Bu
extern uint8_t Dcm_SWV[4],Dcm_LoadStatus,Dcm_CC,Dcm_CDTCS;
extern uint32_t Dcm_ActiveSessionState,Dcm_LoadTimer,Dcm_SessionCounter,Dcm_MainCounter;
void Dcm_Init(void);
void Dcm_main(void);
void Dcm_RxIndication(const uint8_t *data,uint16_t len,int result);
void Dcm_TxConfirmation(int result);
#endif
