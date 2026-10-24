/* DcmAppl.h - ELC application callouts of the Dcm (the "no RTE" replacement for the
 * data/routine ports a generated Dcm would bind through the RTE). */
#ifndef ELC_DCMAPPL_H
#define ELC_DCMAPPL_H

#include "Dcm.h"

#define DCMAPPL_FBL_SW_VERSION 33u      /* FBL x.y.z as reported by ELC_FBL (BOOT_SW_VERSION); the APP cannot read it back */
#define DCMAPPL_LOADSTATUS_NONE 0xFFu   /* no forced load state from routine 0x0042 */
extern uint8 DcmAppl_LoadStatus;        /* forced load state (0/1) while routine 0x0042 is active */

/* Data identifier readers: write exactly the configured length, return E_OK. */
uint8 DcmAppl_ReadResetCounter(uint8 *data);       /* 4  EcuM_TotalResetCounter, little endian */
uint8 DcmAppl_ReadTimeActive(uint8 *data);         /* 4  [ms] */
uint8 DcmAppl_ReadTimeWithoutReset(uint8 *data);   /* 4  [ms] */
uint8 DcmAppl_ReadResetData(uint8 *data);          /* 2  reset reason, reset info */
uint8 DcmAppl_ReadCpuLoad(uint8 *data);            /* 1  [%] */
uint8 DcmAppl_ReadLoadStatus(uint8 *data);         /* 1  SMon_L1ST */
uint8 DcmAppl_ReadSoftwareVersion(uint8 *data);    /* 4  */
uint8 DcmAppl_ReadSoftwareVersionFcd(uint8 *data); /* 6  F101: APP major/minor/patch, FBL major/minor/patch (FCD, ZGW) */
uint8 DcmAppl_ReadActiveSession(uint8 *data);      /* 1  */
uint8 DcmAppl_ReadActiveBlock(uint8 *data);        /* 1  1 = application */

/* Routines. */
uint8 DcmAppl_RoutineHealRetry(const uint8 *option);    /* 0x0040: clear L1 retry counter / supply lock */
uint8 DcmAppl_RoutineTimedLoad(const uint8 *option);    /* 0x0042: [state][seconds] forced load state */

/* ECU reset / programming session entry: persists NvM first. Returns 0 (ready), DCM_E_PENDING
 * (call again) or an NRC (0x22 conditions, 0x72 persistence failed). */
uint8 DcmAppl_PrepareReset(Dcm_OpStatusType opStatus, boolean programming);
void DcmAppl_ExecuteReset(void);

void DcmAppl_SessionChanged(uint8 session);
void DcmAppl_MainFunction(void);                         /* 5 ms, from Dcm_MainFunction() */

#endif /* ELC_DCMAPPL_H */
