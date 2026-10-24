/* Dcm_Cfg.c - ELC data identifiers and routines.
 *
 * Legacy ELC data identifiers keep their meaning under both the F0xx and F1xx prefix
 * (mask 0xFEFF), except the reset counter (F001 only): F101 is the 6 byte software version
 * (APP x.y.z, FBL x.y.z) that FCD and the ZGW expect. F180/F080 keep the 4 byte ELC version
 * (see docs/knowledge/DIAGNOSTICS.md). Freeze frame DIDs F300..F30B are only
 * available inside UDS 0x19 0x04 (they are encoded by Dem). */
#include "Dcm.h"
#include "DcmAppl.h"

#define DCM_DID_MASK_F0F1 0xFEFFu
#define DCM_DID_MASK_EXACT 0xFFFFu

static const Dcm_DidType Dcm_Dids[] =
{
    /* did     mask             len  sessions              reader */
    { 0xF001u, DCM_DID_MASK_EXACT, 4u, DCM_SESSION_MASK_ALL, DcmAppl_ReadResetCounter },        /* F101 is the FCD/ZGW software version */
    { 0xF101u, DCM_DID_MASK_EXACT, 6u, DCM_SESSION_MASK_ALL, DcmAppl_ReadSoftwareVersionFcd },
    { 0xF003u, DCM_DID_MASK_F0F1, 4u, DCM_SESSION_MASK_ALL, DcmAppl_ReadTimeActive },
    { 0xF004u, DCM_DID_MASK_F0F1, 4u, DCM_SESSION_MASK_ALL, DcmAppl_ReadTimeWithoutReset },
    { 0xF005u, DCM_DID_MASK_F0F1, 2u, DCM_SESSION_MASK_ALL, DcmAppl_ReadResetData },
    { 0xF006u, DCM_DID_MASK_F0F1, 1u, DCM_SESSION_MASK_ALL, DcmAppl_ReadCpuLoad },
    { 0xF01Du, DCM_DID_MASK_F0F1, 1u, DCM_SESSION_MASK_ALL, DcmAppl_ReadLoadStatus },
    { 0xF080u, DCM_DID_MASK_F0F1, 4u, DCM_SESSION_MASK_ALL, DcmAppl_ReadSoftwareVersion },
    { 0xF07Eu, DCM_DID_MASK_F0F1, 1u, DCM_SESSION_MASK_ALL, DcmAppl_ReadActiveSession },
    { 0xF086u, DCM_DID_MASK_F0F1, 1u, DCM_SESSION_MASK_ALL, DcmAppl_ReadActiveSession },   /* F186: active diagnostic session */
    { 0xF07Fu, DCM_DID_MASK_F0F1, 1u, DCM_SESSION_MASK_ALL, DcmAppl_ReadActiveBlock },
    { 0xF000u, DCM_DID_MASK_F0F1, 1u, DCM_SESSION_MASK_ALL, DcmAppl_ReadActiveBlock }      /* F100: active software block */
};

static const Dcm_RoutineType Dcm_Routines[] =
{
    { 0x0040u, 0u, DcmAppl_RoutineHealRetry },
    { 0x0042u, 2u, DcmAppl_RoutineTimedLoad }
};

const Dcm_ConfigType Dcm_Config =
{
    Dcm_DefaultServices, DCM_DEFAULT_SERVICE_COUNT,
    Dcm_Dids,            (uint8)(sizeof(Dcm_Dids) / sizeof(Dcm_Dids[0])),
    Dcm_Routines,        (uint8)(sizeof(Dcm_Routines) / sizeof(Dcm_Routines[0]))
};
