/* Dcm.h - Diagnostic Communication Manager (UDS, ISO 14229-1) for the ELC lab stack.
 *
 * Requests arrive through the TP buffer callbacks (PduR from CanTp) straight into the Dcm RX
 * buffer; responses leave through PduR_DcmTransmit() + Dcm_CopyTxData(). Services, data
 * identifiers and routines are described by the tables in Dcm_Cfg.c; ECU specific data and
 * policy are provided by the DcmAppl_* callouts (APPL/DcmAppl). One connection (physical,
 * extended addressing via CanTp), one request at a time. */
#ifndef ELC_BSW_DCM_H
#define ELC_BSW_DCM_H

#include "ComStack_Cfg.h"
#include "Dcm_Cfg.h"

typedef uint8 Dcm_OpStatusType;
#define DCM_INITIAL   0u    /* first call of a handler */
#define DCM_PENDING   1u    /* repeated call after the handler returned DCM_E_PENDING */
#define DCM_CANCEL    2u

/* Handler result: 0 positive response, DCM_E_PENDING call again, anything else a UDS NRC. */
#define DCM_E_OK       0x00u
#define DCM_E_PENDING  0xFFu

/* Service handler. `req` is the full request without suppressPosRsp bit (req[0] = SID).
 * The positive response body (everything after the SID byte; for sub-function services the
 * echoed sub-function first) is written to `resp`; *respLen is the capacity on entry and the
 * body length on exit. */
typedef uint8 (*Dcm_ServiceHandlerType)(Dcm_OpStatusType opStatus, const uint8 *req, uint16 reqLen,
                                        uint8 *resp, uint16 *respLen);

typedef struct
{
    uint8                  sid;
    uint8                  hasSubFunction;   /* bit 7 of the 2nd byte is suppressPosRspMsgIndicationBit */
    uint8                  minLength;        /* shortest legal request including the SID */
    uint8                  sessionMask;
    Dcm_ServiceHandlerType handler;
} Dcm_ServiceType;

typedef uint8 (*Dcm_DidReadFctType)(uint8 *data);   /* writes exactly `length` bytes */

typedef struct
{
    uint16             did;
    uint16             mask;          /* (requested & mask) == did selects this entry */
    uint8              length;
    uint8              sessionMask;
    Dcm_DidReadFctType read;
} Dcm_DidType;

typedef uint8 (*Dcm_RoutineFctType)(const uint8 *optionRecord);   /* NRC or 0 */

typedef struct
{
    uint16             rid;
    uint8              optionLength;  /* exact length of the routineControlOptionRecord */
    Dcm_RoutineFctType start;
} Dcm_RoutineType;

typedef struct
{
    const Dcm_ServiceType *services;  uint8 numServices;
    const Dcm_DidType     *dids;      uint8 numDids;
    const Dcm_RoutineType *routines;  uint8 numRoutines;
} Dcm_ConfigType;

extern const Dcm_ConfigType Dcm_Config;
extern const Dcm_ServiceType Dcm_DefaultServices[DCM_DEFAULT_SERVICE_COUNT];

/* Active session; retained in the NCR RAM window shared with the FBL (0x20004FC0). */
extern uint32 Dcm_ActiveSessionState;

void Dcm_Init(const Dcm_ConfigType *ConfigPtr);
void Dcm_MainFunction(void);   /* 5 ms */

/* TP interface (called by PduR on behalf of CanTp). */
BufReq_ReturnType Dcm_StartOfReception(PduIdType id, const PduInfoType *info, PduLengthType TpSduLength,
                                       PduLengthType *bufferSizePtr);
BufReq_ReturnType Dcm_CopyRxData(PduIdType id, const PduInfoType *info, PduLengthType *bufferSizePtr);
void Dcm_TpRxIndication(PduIdType id, Std_ReturnType result);
BufReq_ReturnType Dcm_CopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry,
                                 PduLengthType *availableDataPtr);
void Dcm_TpTxConfirmation(PduIdType id, Std_ReturnType result);

#endif /* ELC_BSW_DCM_H */
