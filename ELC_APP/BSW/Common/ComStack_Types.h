/* ComStack_Types.h - AUTOSAR communication stack types (PDU, TP buffer API). */
#ifndef ELC_COMSTACK_TYPES_H
#define ELC_COMSTACK_TYPES_H

#include "Std_Types.h"

typedef uint8  PduIdType;
typedef uint16 PduLengthType;
typedef uint8  NetworkHandleType;

typedef struct
{
    uint8        *SduDataPtr;
    uint8        *MetaDataPtr;
    PduLengthType SduLength;
} PduInfoType;

/* Buffer request result of the TP buffer callbacks (StartOfReception, CopyRxData, CopyTxData). */
typedef enum
{
    BUFREQ_OK = 0u,
    BUFREQ_E_NOT_OK,
    BUFREQ_E_BUSY,
    BUFREQ_E_OVFL
} BufReq_ReturnType;

typedef enum
{
    TP_DATACONF = 0u,
    TP_DATARETRY,
    TP_CONFPENDING
} TpDataStateType;

typedef struct
{
    TpDataStateType TpDataState;
    PduLengthType   TxTpDataCnt;
} RetryInfoType;

/* ISO 15765-2 / CanTp notification results (reported through the Std_ReturnType
 * result of the *RxIndication / *TxConfirmation callbacks; the detailed reason is
 * kept for diagnostics counters). */
typedef enum
{
    NTFRSLT_OK = 0u,
    NTFRSLT_E_NOT_OK,
    NTFRSLT_E_TIMEOUT_A,
    NTFRSLT_E_TIMEOUT_BS,
    NTFRSLT_E_TIMEOUT_CR,
    NTFRSLT_E_WRONG_SN,
    NTFRSLT_E_INVALID_FS,
    NTFRSLT_E_UNEXP_PDU,
    NTFRSLT_E_WFT_OVRN,
    NTFRSLT_E_ABORT,
    NTFRSLT_E_NO_BUFFER,
    NTFRSLT_E_CANCELATION_OK,
    NTFRSLT_E_CANCELATION_NOT_OK
} NotifResultType;

#endif /* ELC_COMSTACK_TYPES_H */
