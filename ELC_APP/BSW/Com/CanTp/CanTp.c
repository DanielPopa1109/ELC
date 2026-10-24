#include "CanTp.h"
#include "../CanIf/CanIf.h"
#include "../PduR/PduR.h"
#include <string.h>

/* N_PCI frame types (high nibble of the PCI byte). */
#define CANTP_NPCI_SF   0u
#define CANTP_NPCI_FF   1u
#define CANTP_NPCI_CF   2u
#define CANTP_NPCI_FC   3u
/* Flow status (low nibble of FC). */
#define CANTP_FS_CTS    0u
#define CANTP_FS_WAIT   1u
#define CANTP_FS_OVFLW  2u
/* Largest SF_DL / first-frame payload with one address byte on classic CAN. */
#define CANTP_SF_MAX    6u
#define CANTP_FF_DATA   5u
#define CANTP_CF_DATA   6u
/* STmin: 0x00..0x7F ms, 0xF1..0xF9 100..900 us, everything else reserved (use 127 ms). */
#define CANTP_STMIN_MAX_MS 127u

typedef enum
{
    CANTP_TX_IDLE = 0u,
    CANTP_TX_SEND_FIRST,       /* SF or FF waiting to be handed to CanIf */
    CANTP_TX_WAIT_FIRST_CONF,
    CANTP_TX_WAIT_FC,
    CANTP_TX_SEND_CF,
    CANTP_TX_WAIT_CF_CONF
} CanTp_TxStateType;

static const CanTp_ChannelConfigType *CanTp_Ch;

/* Receive (ISO 15765-2 receiver). */
static uint8  CanTp_RxActive;      /* reception open at the upper layer */
static uint8  CanTp_FcPending;     /* 0 none, 1 CTS, 2 OVFLW still to be sent */
static uint8  CanTp_FcInFlight;    /* FC handed to CanIf, confirmation outstanding */
static uint8  CanTp_FcWasOvfl;
static uint8  CanTp_RxSn;
static uint8  CanTp_RxBlock;
static uint16 CanTp_RxExpected;
static uint16 CanTp_RxCopied;
static uint16 CanTp_RxTimer;
static uint16 CanTp_FcTimer;       /* N_Ar: own FC queued/in flight but not confirmed */

/* Transmit (ISO 15765-2 sender). */
static CanTp_TxStateType CanTp_TxState;
static uint8  CanTp_TxFrame[8];
static uint8  CanTp_TxFrameReady;
static uint8  CanTp_TxChunk;       /* payload bytes in CanTp_TxFrame */
static uint8  CanTp_TxSn;
static uint8  CanTp_TxBlock;
static uint8  CanTp_TxBs;
static uint8  CanTp_TxStTicks;
static uint8  CanTp_TxDelay;
static uint8  CanTp_TxWft;
static uint16 CanTp_TxLength;
static uint16 CanTp_TxOffset;
static uint16 CanTp_TxTimer;
static uint8  CanTp_EarlyFc[3];    /* FC that overtook our own TX confirmation */
static uint8  CanTp_EarlyFcValid;

volatile uint8 CanTp_LastRxResult;
volatile uint8 CanTp_LastTxResult;

static void CanTp_RxAbort(NotifResultType why)
{
    if (CanTp_RxActive != 0u)
    {
        CanTp_RxActive = 0u;
        CanTp_FcPending = 0u;
        CanTp_RxTimer = 0u;
        CanTp_LastRxResult = (uint8)why;
        PduR_CanTpRxIndication(CanTp_Ch->rxNSduId, E_NOT_OK);
    }
}

static void CanTp_QueueFc(uint8 kind)
{
    CanTp_FcPending = kind;
    CanTp_FcTimer = CanTp_Ch->nArTicks;
}

static void CanTp_TxFinish(Std_ReturnType result, NotifResultType why)
{
    CanTp_TxState = CANTP_TX_IDLE;
    CanTp_TxFrameReady = 0u;
    CanTp_EarlyFcValid = 0u;
    if (result != E_OK)
    {
        CanTp_LastTxResult = (uint8)why;
    }
    PduR_CanTpTxConfirmation(CanTp_Ch->txNSduId, result);
}

void CanTp_Init(const CanTp_ConfigType *CfgPtr)
{
    const CanTp_ConfigType *cfg = (CfgPtr != NULL_PTR) ? CfgPtr : &CanTp_Config;

    CanTp_Ch = (cfg->numChannels != 0u) ? &cfg->channels[0] : NULL_PTR;
    CanTp_RxActive = 0u;
    CanTp_FcPending = 0u;
    CanTp_FcInFlight = 0u;
    CanTp_TxState = CANTP_TX_IDLE;
    CanTp_TxFrameReady = 0u;
    CanTp_EarlyFcValid = 0u;
    CanTp_LastRxResult = (uint8)NTFRSLT_OK;
    CanTp_LastTxResult = (uint8)NTFRSLT_OK;
}

uint8 CanTp_IsIdle(void)
{
    return (uint8)((CanTp_TxState == CANTP_TX_IDLE) && (CanTp_RxActive == 0u) &&
                   (CanTp_FcInFlight == 0u) && (CanTp_FcPending == 0u));
}

Std_ReturnType CanTp_CancelTransmit(PduIdType TxPduId)
{
    if ((CanTp_Ch == NULL_PTR) || (TxPduId != CanTp_Ch->txNSduId) || (CanTp_TxState == CANTP_TX_IDLE))
    {
        return E_NOT_OK;
    }
    CanTp_TxFinish(E_NOT_OK, NTFRSLT_E_CANCELATION_OK);
    return E_OK;
}

Std_ReturnType CanTp_CancelReceive(PduIdType RxPduId)
{
    if ((CanTp_Ch == NULL_PTR) || (RxPduId != CanTp_Ch->rxNSduId) || (CanTp_RxActive == 0u))
    {
        return E_NOT_OK;
    }
    CanTp_RxAbort(NTFRSLT_E_CANCELATION_OK);
    return E_OK;
}

Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
    if ((CanTp_Ch == NULL_PTR) || (TxPduId != CanTp_Ch->txNSduId) || (PduInfoPtr == NULL_PTR) ||
        (PduInfoPtr->SduLength == 0u) || (PduInfoPtr->SduLength > CANTP_MAX_SDU_LEN) ||
        (CanTp_TxState != CANTP_TX_IDLE) || (CanTp_IsIdle() == 0u))
    {
        return E_NOT_OK;
    }
    CanTp_TxLength = PduInfoPtr->SduLength;
    CanTp_TxOffset = 0u;
    CanTp_TxSn = 1u;
    CanTp_TxWft = 0u;
    CanTp_TxFrameReady = 0u;
    CanTp_TxTimer = CanTp_Ch->nAsTicks;
    CanTp_TxState = CANTP_TX_SEND_FIRST;
    return E_OK;
}

static uint8 CanTp_StMinToTicks(uint8 stMin)
{
    if (stMin <= CANTP_STMIN_MAX_MS)
    {
        return (uint8)((stMin + CANTP_MAIN_PERIOD_MS - 1u) / CANTP_MAIN_PERIOD_MS);
    }
    if ((stMin >= 0xF1u) && (stMin <= 0xF9u))
    {
        return 1u;
    }
    /* ISO 15765-2: a reserved STmin must be treated as the longest value (127 ms). */
    return (uint8)((CANTP_STMIN_MAX_MS + CANTP_MAIN_PERIOD_MS - 1u) / CANTP_MAIN_PERIOD_MS);
}

/* ---- Sender: flow control received ---------------------------------------- */
static void CanTp_TxFlowControl(const uint8 *fc)
{
    uint8 fs = (uint8)(fc[0] & 0x0Fu);

    if (fs == CANTP_FS_CTS)
    {
        CanTp_TxBs = fc[1];
        CanTp_TxStTicks = CanTp_StMinToTicks(fc[2]);
        CanTp_TxDelay = CanTp_TxStTicks;
        CanTp_TxBlock = 0u;
        CanTp_TxWft = 0u;
        CanTp_TxState = CANTP_TX_SEND_CF;
        CanTp_TxTimer = CanTp_Ch->nAsTicks;
    }
    else if (fs == CANTP_FS_WAIT)
    {
        if (++CanTp_TxWft > CanTp_Ch->wftMax)
        {
            CanTp_TxFinish(E_NOT_OK, NTFRSLT_E_WFT_OVRN);
        }
        else
        {
            CanTp_TxTimer = CanTp_Ch->nBsTicks;
        }
    }
    else
    {
        CanTp_TxFinish(E_NOT_OK, (fs == CANTP_FS_OVFLW) ? NTFRSLT_E_NOT_OK : NTFRSLT_E_INVALID_FS);
    }
}

/* ---- Receiver ---------------------------------------------------------------- */
static void CanTp_RxSingleFrame(const uint8 *d, uint8 len)
{
    uint8 n = (uint8)(d[1] & 0x0Fu);
    PduInfoType info;
    PduLengthType avail = 0u;

    if ((n == 0u) || (n > CANTP_SF_MAX) || (len < (uint8)(2u + n)))
    {
        return;                              /* invalid SF_DL: ignore */
    }
    CanTp_RxAbort(NTFRSLT_E_UNEXP_PDU);      /* a new SF terminates a running reception */
    if ((PduR_CanTpStartOfReception(CanTp_Ch->rxNSduId, NULL_PTR, n, &avail) != BUFREQ_OK) || (avail < n))
    {
        return;
    }
    info.SduDataPtr = (uint8 *)&d[2];
    info.MetaDataPtr = NULL_PTR;
    info.SduLength = n;
    if (PduR_CanTpCopyRxData(CanTp_Ch->rxNSduId, &info, &avail) == BUFREQ_OK)
    {
        PduR_CanTpRxIndication(CanTp_Ch->rxNSduId, E_OK);
    }
    else
    {
        PduR_CanTpRxIndication(CanTp_Ch->rxNSduId, E_NOT_OK);
    }
}

static void CanTp_RxFirstFrame(const uint8 *d, uint8 len)
{
    uint16 total = (uint16)(((uint16)(d[1] & 0x0Fu) << 8) | d[2]);
    PduInfoType info;
    PduLengthType avail = 0u;
    BufReq_ReturnType br;

    /* FF_DL must exceed the largest SF_DL; a shorter value is an invalid FF and is ignored. */
    if ((len != 8u) || (total <= CANTP_SF_MAX))
    {
        return;
    }
    CanTp_RxAbort(NTFRSLT_E_UNEXP_PDU);
    br = PduR_CanTpStartOfReception(CanTp_Ch->rxNSduId, NULL_PTR, total, &avail);
    if (br == BUFREQ_E_OVFL)
    {
        CanTp_QueueFc(2u);                   /* FC.OVFLW, no reception opened */
        return;
    }
    if (br != BUFREQ_OK)
    {
        return;
    }
    info.SduDataPtr = (uint8 *)&d[3];
    info.MetaDataPtr = NULL_PTR;
    info.SduLength = CANTP_FF_DATA;
    if ((avail < CANTP_FF_DATA) || (PduR_CanTpCopyRxData(CanTp_Ch->rxNSduId, &info, &avail) != BUFREQ_OK))
    {
        PduR_CanTpRxIndication(CanTp_Ch->rxNSduId, E_NOT_OK);
        return;
    }
    CanTp_RxActive = 1u;
    CanTp_RxExpected = total;
    CanTp_RxCopied = CANTP_FF_DATA;
    CanTp_RxSn = 1u;
    CanTp_RxBlock = 0u;
    CanTp_RxTimer = CanTp_Ch->nCrTicks;
    CanTp_QueueFc(1u);
}

static void CanTp_RxConsecutiveFrame(const uint8 *d, uint8 len)
{
    uint16 remain;
    uint8 count;
    PduInfoType info;
    PduLengthType avail = 0u;

    if (CanTp_RxActive == 0u)
    {
        return;                              /* unexpected CF: ignore */
    }
    if (CanTp_FcPending != 0u)
    {
        CanTp_RxAbort(NTFRSLT_E_UNEXP_PDU);  /* CF before our FC was even sent */
        return;
    }
    if ((uint8)(d[1] & 0x0Fu) != CanTp_RxSn)
    {
        CanTp_RxAbort(NTFRSLT_E_WRONG_SN);
        return;
    }
    remain = (uint16)(CanTp_RxExpected - CanTp_RxCopied);
    count = (remain > CANTP_CF_DATA) ? (uint8)CANTP_CF_DATA : (uint8)remain;
    if (len < (uint8)(2u + count))
    {
        CanTp_RxAbort(NTFRSLT_E_NOT_OK);
        return;
    }
    info.SduDataPtr = (uint8 *)&d[2];
    info.MetaDataPtr = NULL_PTR;
    info.SduLength = count;
    if (PduR_CanTpCopyRxData(CanTp_Ch->rxNSduId, &info, &avail) != BUFREQ_OK)
    {
        CanTp_RxAbort(NTFRSLT_E_NO_BUFFER);
        return;
    }
    CanTp_RxCopied = (uint16)(CanTp_RxCopied + count);
    CanTp_RxSn = (uint8)((CanTp_RxSn + 1u) & 0x0Fu);
    CanTp_RxTimer = CanTp_Ch->nCrTicks;
    if (CanTp_RxCopied == CanTp_RxExpected)
    {
        CanTp_RxActive = 0u;
        CanTp_RxTimer = 0u;
        PduR_CanTpRxIndication(CanTp_Ch->rxNSduId, E_OK);
    }
    else if ((CanTp_Ch->blockSize != 0u) && (++CanTp_RxBlock >= CanTp_Ch->blockSize))
    {
        CanTp_RxBlock = 0u;
        CanTp_QueueFc(1u);
    }
}

void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    const uint8 *d;
    uint8 len;

    if ((CanTp_Ch == NULL_PTR) || (RxPduId != CanTp_Ch->rxNPduId) || (PduInfoPtr == NULL_PTR) ||
        (PduInfoPtr->SduDataPtr == NULL_PTR) || (PduInfoPtr->SduLength < 2u) || (PduInfoPtr->SduLength > 8u))
    {
        return;
    }
    d = PduInfoPtr->SduDataPtr;
    len = (uint8)PduInfoPtr->SduLength;
    if ((d[0] != CanTp_Ch->rxAddress) &&
        !((d[0] == CanTp_Ch->fcAddress) && ((uint8)(d[1] >> 4) == CANTP_NPCI_FC)))
    {
        return;
    }

    switch ((uint8)(d[1] >> 4))
    {
    case CANTP_NPCI_FC:
        if (len < 4u)
        {
            return;
        }
        if (CanTp_TxState == CANTP_TX_WAIT_FC)
        {
            CanTp_TxFlowControl(&d[1]);
        }
        else if ((CanTp_TxState == CANTP_TX_WAIT_FIRST_CONF) ||
                 ((CanTp_TxState == CANTP_TX_WAIT_CF_CONF) && (CanTp_TxBs != 0u) &&
                  ((CanTp_TxBlock + 1u) >= CanTp_TxBs)))
        {
            (void)memcpy(CanTp_EarlyFc, &d[1], 3u);
            CanTp_EarlyFcValid = 1u;
        }
        else
        {
            /* unexpected FC: ignore */
        }
        break;
    case CANTP_NPCI_SF:
        if (CanTp_TxState == CANTP_TX_IDLE) { CanTp_RxSingleFrame(d, len); }
        break;
    case CANTP_NPCI_FF:
        if (CanTp_TxState == CANTP_TX_IDLE) { CanTp_RxFirstFrame(d, len); }
        break;
    case CANTP_NPCI_CF:
        if (CanTp_TxState == CANTP_TX_IDLE) { CanTp_RxConsecutiveFrame(d, len); }
        break;
    default:
        break;
    }
}

void CanTp_TxConfirmation(PduIdType TxPduId)
{
    if ((CanTp_Ch == NULL_PTR) || (TxPduId != CanTp_Ch->txNPduId))
    {
        return;
    }
    if (CanTp_FcInFlight != 0u)
    {
        CanTp_FcInFlight = 0u;
        if (CanTp_FcWasOvfl == 0u)
        {
            CanTp_RxTimer = CanTp_Ch->nCrTicks;
        }
        return;
    }

    switch (CanTp_TxState)
    {
    case CANTP_TX_WAIT_FIRST_CONF:
        if (CanTp_TxLength <= CANTP_SF_MAX)
        {
            CanTp_TxFinish(E_OK, NTFRSLT_OK);
            return;
        }
        CanTp_TxOffset = CANTP_FF_DATA;
        CanTp_TxState = CANTP_TX_WAIT_FC;
        break;
    case CANTP_TX_WAIT_CF_CONF:
        CanTp_TxOffset = (uint16)(CanTp_TxOffset + CanTp_TxChunk);
        CanTp_TxSn = (uint8)((CanTp_TxSn + 1u) & 0x0Fu);
        if (CanTp_TxOffset == CanTp_TxLength)
        {
            CanTp_TxFinish(E_OK, NTFRSLT_OK);
            return;
        }
        CanTp_TxBlock++;
        CanTp_TxDelay = CanTp_TxStTicks;
        CanTp_TxState = ((CanTp_TxBs != 0u) && (CanTp_TxBlock >= CanTp_TxBs)) ?
                        CANTP_TX_WAIT_FC : CANTP_TX_SEND_CF;
        break;
    default:
        return;
    }
    CanTp_TxTimer = (CanTp_TxState == CANTP_TX_WAIT_FC) ? CanTp_Ch->nBsTicks : CanTp_Ch->nAsTicks;
    if ((CanTp_TxState == CANTP_TX_WAIT_FC) && (CanTp_EarlyFcValid != 0u))
    {
        CanTp_EarlyFcValid = 0u;
        CanTp_TxFlowControl(CanTp_EarlyFc);
    }
}

static void CanTp_InitFrame(uint8 *frame)
{
    (void)memset(frame, CanTp_Ch->paddingByte, 8u);
    frame[0] = CanTp_Ch->txAddress;
}

static Std_ReturnType CanTp_SendFrame(const uint8 *frame)
{
    PduInfoType info;

    info.SduDataPtr = (uint8 *)frame;
    info.MetaDataPtr = NULL_PTR;
    info.SduLength = 8u;
    return CanIf_Transmit(CanTp_Ch->txNPduId, &info);
}

/* Fetch `count` payload bytes from the upper layer into `dst`.
 * Returns E_OK, or E_NOT_OK when the frame is not available yet / the transfer failed. */
static Std_ReturnType CanTp_FetchPayload(uint8 *dst, uint8 count)
{
    PduInfoType info;
    PduLengthType available = 0u;
    BufReq_ReturnType br;

    info.SduDataPtr = dst;
    info.MetaDataPtr = NULL_PTR;
    info.SduLength = count;
    br = PduR_CanTpCopyTxData(CanTp_Ch->txNSduId, &info, NULL_PTR, &available);
    if (br == BUFREQ_E_BUSY)
    {
        return E_NOT_OK;                     /* try again next cycle */
    }
    if (br != BUFREQ_OK)
    {
        CanTp_TxFinish(E_NOT_OK, NTFRSLT_E_NO_BUFFER);
        return E_NOT_OK;
    }
    return E_OK;
}

static void CanTp_MainTx(void)
{
    uint8 *f = CanTp_TxFrame;

    if (CanTp_TxState == CANTP_TX_IDLE)
    {
        return;
    }
    if ((CanTp_TxTimer != 0u) && (--CanTp_TxTimer == 0u))
    {
        CanTp_TxFinish(E_NOT_OK, (CanTp_TxState == CANTP_TX_WAIT_FC) ? NTFRSLT_E_TIMEOUT_BS : NTFRSLT_E_TIMEOUT_A);
        return;
    }

    if (CanTp_TxState == CANTP_TX_SEND_FIRST)
    {
        if (CanTp_TxFrameReady == 0u)
        {
            CanTp_InitFrame(f);
            if (CanTp_TxLength <= CANTP_SF_MAX)
            {
                CanTp_TxChunk = (uint8)CanTp_TxLength;
                f[1] = (uint8)((CANTP_NPCI_SF << 4) | CanTp_TxChunk);
                if (CanTp_FetchPayload(&f[2], CanTp_TxChunk) != E_OK) { return; }
            }
            else
            {
                CanTp_TxChunk = CANTP_FF_DATA;
                f[1] = (uint8)((CANTP_NPCI_FF << 4) | (CanTp_TxLength >> 8));
                f[2] = (uint8)CanTp_TxLength;
                if (CanTp_FetchPayload(&f[3], CanTp_TxChunk) != E_OK) { return; }
            }
            CanTp_TxFrameReady = 1u;
        }
    }
    else if (CanTp_TxState == CANTP_TX_SEND_CF)
    {
        uint16 remain;

        if (CanTp_TxDelay != 0u)
        {
            CanTp_TxDelay--;
            return;
        }
        if (CanTp_TxFrameReady == 0u)
        {
            remain = (uint16)(CanTp_TxLength - CanTp_TxOffset);
            CanTp_TxChunk = (remain > CANTP_CF_DATA) ? (uint8)CANTP_CF_DATA : (uint8)remain;
            CanTp_InitFrame(f);
            f[1] = (uint8)((CANTP_NPCI_CF << 4) | CanTp_TxSn);
            if (CanTp_FetchPayload(&f[2], CanTp_TxChunk) != E_OK) { return; }
            CanTp_TxFrameReady = 1u;
        }
    }
    else
    {
        return;
    }

    if (CanTp_SendFrame(f) == E_OK)
    {
        CanTp_TxFrameReady = 0u;
        CanTp_TxState = (CanTp_TxState == CANTP_TX_SEND_FIRST) ? CANTP_TX_WAIT_FIRST_CONF : CANTP_TX_WAIT_CF_CONF;
        CanTp_TxTimer = CanTp_Ch->nAsTicks;
    }
}

static void CanTp_MainRx(void)
{
    uint8 f[8];

    if ((CanTp_RxActive != 0u) && (CanTp_RxTimer != 0u) && (--CanTp_RxTimer == 0u))
    {
        CanTp_RxAbort(NTFRSLT_E_TIMEOUT_CR);
    }
    if (((CanTp_FcPending != 0u) || (CanTp_FcInFlight != 0u)) && (CanTp_FcTimer != 0u) && (--CanTp_FcTimer == 0u))
    {
        /* Own flow control never reached the bus (N_Ar). */
        CanTp_FcPending = 0u;
        CanTp_FcInFlight = 0u;
        CanTp_RxAbort(NTFRSLT_E_TIMEOUT_A);
    }
    if ((CanTp_FcPending != 0u) && (CanTp_FcInFlight == 0u))
    {
        CanTp_InitFrame(f);
        f[1] = (uint8)((CANTP_NPCI_FC << 4) | ((CanTp_FcPending == 1u) ? CANTP_FS_CTS : CANTP_FS_OVFLW));
        f[2] = CanTp_Ch->blockSize;
        f[3] = CanTp_Ch->stMin;
        if (CanTp_SendFrame(f) == E_OK)
        {
            CanTp_FcWasOvfl = (uint8)(CanTp_FcPending == 2u);
            CanTp_FcPending = 0u;
            CanTp_FcInFlight = 1u;
        }
    }
}

void CanTp_MainFunction(void)
{
    if (CanTp_Ch == NULL_PTR)
    {
        return;
    }
    CanTp_MainRx();
    if (CanTp_FcInFlight == 0u)
    {
        CanTp_MainTx();
    }
}
