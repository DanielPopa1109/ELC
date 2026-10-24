/* ISO 15765-2 conformance of the production CanTp.c (extended addressing, classic CAN).
 * The upper layer (PduR/Dcm) and CanIf are replaced by mocks; CanTp itself is unmodified. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ComStack_Cfg.h"
#include "../ELC_APP/BSW/Com/CanTp/CanTp.h"
#include "../ELC_APP/BSW/Com/CanIf/CanIf.h"
#include "../ELC_APP/BSW/Com/PduR/PduR.h"

#define ME  0x58u   /* this ECU (received N_TA) */
#define GW  0xF0u   /* gateway (transmitted N_TA) */

/* ---- mock CanIf --------------------------------------------------------------------------- */
static uint8_t sent[256][8];
static unsigned nSent;
static int canifBusy;
Std_ReturnType CanIf_Transmit(PduIdType id, const PduInfoType *info)
{
    assert(id == CANIF_TXPDU_DIAG_RESP && info->SduLength == 8u);
    if (canifBusy) { return E_NOT_OK; }
    assert(nSent < 256u);
    memcpy(sent[nSent++], info->SduDataPtr, 8u);
    return E_OK;
}

/* ---- mock PduR / Dcm side -------------------------------------------------------------------- */
#define CAP 256u
static uint8_t rxBuf[CAP], txBuf[4096];
static uint16_t rxLen, txLen, txOff;
static unsigned startCalls, rxIndications, txConfirmations, copyRxCalls;
static Std_ReturnType lastRxResult, lastTxResult;
static BufReq_ReturnType startResult;
static BufReq_ReturnType copyTxResult;
static uint16_t lastStartLen;
BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType id, const PduInfoType *info, PduLengthType len, PduLengthType *buf)
{
    (void)info; assert(id == CANTP_RXNSDU_DIAG);
    startCalls++; lastStartLen = len;
    if (startResult != BUFREQ_OK) { return startResult; }
    if (len > CAP) { return BUFREQ_E_OVFL; }
    rxLen = 0; *buf = CAP; return BUFREQ_OK;
}
BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType id, const PduInfoType *info, PduLengthType *buf)
{
    assert(id == CANTP_RXNSDU_DIAG); copyRxCalls++;
    memcpy(rxBuf + rxLen, info->SduDataPtr, info->SduLength); rxLen = (uint16_t)(rxLen + info->SduLength);
    *buf = (PduLengthType)(CAP - rxLen); return BUFREQ_OK;
}
void PduR_CanTpRxIndication(PduIdType id, Std_ReturnType result)
{
    assert(id == CANTP_RXNSDU_DIAG); rxIndications++; lastRxResult = result;
}
BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType id, const PduInfoType *info, const RetryInfoType *retry, PduLengthType *avail)
{
    (void)retry; assert(id == CANTP_TXNSDU_DIAG);
    if (copyTxResult != BUFREQ_OK) { return copyTxResult; }
    assert(txOff + info->SduLength <= txLen);
    memcpy(info->SduDataPtr, txBuf + txOff, info->SduLength); txOff = (uint16_t)(txOff + info->SduLength);
    *avail = (PduLengthType)(txLen - txOff); return BUFREQ_OK;
}
void PduR_CanTpTxConfirmation(PduIdType id, Std_ReturnType result)
{
    assert(id == CANTP_TXNSDU_DIAG); txConfirmations++; lastTxResult = result;
}

/* ---- helpers --------------------------------------------------------------------------------- */
static unsigned confirmed;
static void reset(void)
{
    CanTp_Init(&CanTp_Config);
    nSent = 0; confirmed = 0; canifBusy = 0; rxLen = 0; txOff = 0; startCalls = rxIndications = txConfirmations = copyRxCalls = 0;
    startResult = BUFREQ_OK; copyTxResult = BUFREQ_OK;
}
static void rx(const uint8_t *d, uint8_t len)
{
    PduInfoType info; info.SduDataPtr = (uint8_t *)d; info.MetaDataPtr = 0; info.SduLength = len;
    CanTp_RxIndication(CANTP_RXNPDU_DIAG, &info);
}
static void frame(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g, uint8_t h)
{
    uint8_t x[8] = {a, b, c, d, e, f, g, h}; rx(x, 8);
}
static void fc(uint8_t fs, uint8_t bs, uint8_t st) { frame(ME, (uint8_t)(0x30u | fs), bs, st, 0, 0, 0, 0); }
static void tick(void) { CanTp_MainFunction(); }
/* Emulates the CAN driver: every frame handed to CanIf is confirmed on the next call. */
static void confirmAll(void) { while (confirmed < nSent) { confirmed++; CanTp_TxConfirmation(CANTP_TXNPDU_DIAG); } }
static void run(unsigned n) { while (n--) { tick(); confirmAll(); } }
static void transmit(uint16_t len)
{
    PduInfoType info; info.SduDataPtr = 0; info.MetaDataPtr = 0; info.SduLength = len;
    txLen = len; txOff = 0; for (unsigned i = 0; i < len; ++i) { txBuf[i] = (uint8_t)(i * 7u + 1u); }
    assert(CanTp_Transmit(CANTP_TXNSDU_DIAG, &info) == E_OK);
}

int main(void)
{
    /* ---- Reception: single frame ---------------------------------------------------------- */
    reset();
    frame(ME, 0x03, 0x22, 0xF1, 0x01, 0, 0, 0);
    assert(startCalls == 1 && rxIndications == 1 && lastRxResult == E_OK && rxLen == 3 && rxBuf[0] == 0x22 && rxBuf[2] == 0x01);
    frame(ME, 0x00, 1, 2, 3, 4, 5, 6);                    /* SF_DL 0 is invalid */
    frame(ME, 0x07, 1, 2, 3, 4, 5, 6);                    /* SF_DL 7 does not fit with one address byte */
    { uint8_t shortSf[3] = {ME, 0x05, 0x22}; rx(shortSf, 3); }   /* DLC smaller than SF_DL */
    frame(0x57, 0x03, 0x22, 0xF1, 0x01, 0, 0, 0);         /* wrong N_TA */
    { PduInfoType i = {(uint8_t[]){ME, 3, 0x22, 1, 1, 0, 0, 0}, 0, 8}; CanTp_RxIndication(9, &i); }  /* wrong N-PDU */
    assert(startCalls == 1 && rxIndications == 1);
    startResult = BUFREQ_E_BUSY; frame(ME, 0x01, 0x3E, 0, 0, 0, 0, 0); assert(rxIndications == 1);   /* busy upper layer: SF dropped */
    startResult = BUFREQ_OK;

    /* ---- Reception: first frame validity ------------------------------------------------------ */
    reset();
    frame(ME, 0x10, 6, 1, 2, 3, 4, 5);                    /* FF_DL 6 <= max SF_DL: invalid, ignored */
    frame(ME, 0x10, 0, 1, 2, 3, 4, 5);
    tick(); assert(startCalls == 0 && nSent == 0);
    { uint8_t shortFf[6] = {ME, 0x10, 20, 1, 2, 3}; rx(shortFf, 6); }   /* FF must be 8 bytes */
    tick(); assert(startCalls == 0 && nSent == 0);
    frame(ME, 0x10, 7, 1, 2, 3, 4, 5);                    /* smallest legal FF */
    tick(); assert(startCalls == 1 && nSent == 1 && sent[0][0] == GW && sent[0][1] == 0x30 && sent[0][2] == 8 && sent[0][3] == 5);
    assert(sent[0][4] == 0 && sent[0][7] == 0);           /* padding */
    confirmAll(); frame(ME, 0x21, 6, 7, 0, 0, 0, 0);
    assert(rxIndications == 1 && lastRxResult == E_OK && rxLen == 7 && rxBuf[5] == 6 && rxBuf[6] == 7);
    tick(); assert(CanTp_IsIdle());

    /* ---- Reception: 256 byte message, block size 8, SN wrap ---------------------------------------- */
    reset();
    frame(ME, 0x11, 0x00, 0, 1, 2, 3, 4);                 /* FF_DL 256 */
    run(1); assert(sent[0][1] == 0x30);
    for (unsigned i = 0, off = 5; off < 256; ++i)
    {
        uint8_t cf[8] = {ME, (uint8_t)(0x20u | ((i + 1u) & 15u))};
        unsigned n = (256u - off) > 6u ? 6u : (256u - off);
        for (unsigned k = 0; k < n; ++k) { cf[2 + k] = (uint8_t)(off + k); }
        rx(cf, 8); off += n;
        if (((i + 1u) % 8u) == 0u && off < 256) { assert(rxIndications == 0); run(1); assert(sent[nSent - 1][1] == 0x30); }
    }
    assert(rxIndications == 1 && lastRxResult == E_OK && rxLen == 256);
    for (unsigned i = 5; i < 256; ++i) { assert(rxBuf[i] == (uint8_t)i); }
    assert(nSent == 6u);                                   /* 42 CFs: initial FC + one after CF 8, 16, 24, 32, 40 */

    /* ---- Reception: errors ---------------------------------------------------------------------------- */
    reset();                                               /* message larger than the upper layer buffer */
    frame(ME, 0x11, 0x01, 0, 0, 0, 0, 0);
    run(2); assert(startCalls == 1 && nSent == 1 && sent[0][1] == 0x32 && rxIndications == 0 && CanTp_IsIdle());
    reset();                                               /* wrong sequence number */
    frame(ME, 0x10, 20, 1, 2, 3, 4, 5); run(1);
    frame(ME, 0x22, 6, 7, 8, 9, 10, 11); assert(rxIndications == 1 && lastRxResult == E_NOT_OK && CanTp_LastRxResult == NTFRSLT_E_WRONG_SN);
    frame(ME, 0x21, 6, 7, 8, 9, 10, 11); assert(rxIndications == 1);   /* unexpected CF ignored */
    reset();                                               /* N_Cr: no consecutive frame */
    frame(ME, 0x10, 20, 1, 2, 3, 4, 5); run(1);
    run(CANTP_MS_TO_TICKS(1000) - 2); assert(rxIndications == 0);
    run(3); assert(rxIndications == 1 && lastRxResult == E_NOT_OK && CanTp_LastRxResult == NTFRSLT_E_TIMEOUT_CR && CanTp_IsIdle());
    reset();                                               /* a new SF terminates the running reception */
    frame(ME, 0x10, 20, 1, 2, 3, 4, 5); run(1);
    frame(ME, 0x02, 0x3E, 0x00, 0, 0, 0, 0);
    assert(rxIndications == 2 && lastRxResult == E_OK && CanTp_LastRxResult == NTFRSLT_E_UNEXP_PDU && rxLen == 2);
    reset();                                               /* a new FF restarts the reception */
    frame(ME, 0x10, 20, 1, 2, 3, 4, 5); run(1);
    frame(ME, 0x10, 14, 9, 9, 9, 9, 9); assert(rxIndications == 1 && lastRxResult == E_NOT_OK); run(1);
    assert(startCalls == 2 && nSent == 2);
    reset();                                               /* CF before our FC reached CanIf aborts; after hand-over it is accepted */
    canifBusy = 1; frame(ME, 0x10, 20, 1, 2, 3, 4, 5); tick();
    frame(ME, 0x21, 6, 7, 8, 9, 10, 11); assert(rxIndications == 1 && lastRxResult == E_NOT_OK);
    reset();
    frame(ME, 0x10, 20, 1, 2, 3, 4, 5); tick(); assert(nSent == 1);   /* FC on the bus, confirmation not yet processed */
    frame(ME, 0x21, 6, 7, 8, 9, 10, 11); frame(ME, 0x22, 12, 13, 14, 15, 16, 17); frame(ME, 0x23, 18, 19, 20, 0, 0, 0);
    assert(rxIndications == 1 && lastRxResult == E_OK && rxLen == 20);
    confirmAll(); tick(); assert(CanTp_IsIdle());
    reset();                                               /* own FC never confirmed: N_Ar releases the channel */
    frame(ME, 0x10, 20, 1, 2, 3, 4, 5); tick();
    for (unsigned i = 0; i < CANTP_MS_TO_TICKS(1000) + 2u; ++i) { tick(); }
    assert(rxIndications == 1 && lastRxResult == E_NOT_OK && CanTp_IsIdle());
    reset();                                               /* FC that CanIf cannot send does not wedge CanTp */
    canifBusy = 1; frame(ME, 0x11, 0x01, 0, 0, 0, 0, 0);
    for (unsigned i = 0; i < CANTP_MS_TO_TICKS(1000) + 2u; ++i) { tick(); }
    assert(CanTp_IsIdle());

    /* ---- Transmission: single frame ----------------------------------------------------------------------- */
    reset();
    transmit(5); run(1);
    assert(nSent == 1 && sent[0][0] == GW && sent[0][1] == 0x05 && !memcmp(&sent[0][2], txBuf, 5) && sent[0][7] == 0);
    assert(txConfirmations == 1 && lastTxResult == E_OK && CanTp_IsIdle());
    reset();
    transmit(6); run(1); assert(sent[0][1] == 0x06 && txConfirmations == 1);        /* 6 bytes still fit one SF */
    reset();
    { PduInfoType i = {0, 0, 0}; assert(CanTp_Transmit(CANTP_TXNSDU_DIAG, &i) == E_NOT_OK); i.SduLength = 4096; assert(CanTp_Transmit(CANTP_TXNSDU_DIAG, &i) == E_NOT_OK);
      i.SduLength = 4095; assert(CanTp_Transmit(CANTP_TXNSDU_DIAG, &i) == E_OK); assert(CanTp_Transmit(CANTP_TXNSDU_DIAG, &i) == E_NOT_OK);
      assert(CanTp_CancelTransmit(CANTP_TXNSDU_DIAG) == E_OK && txConfirmations == 1 && lastTxResult == E_NOT_OK && CanTp_IsIdle()); }

    /* ---- Transmission: segmented, BS 0 ------------------------------------------------------------------------------ */
    reset();
    transmit(20); run(1);
    assert(nSent == 1 && sent[0][1] == 0x10 && sent[0][2] == 20 && !memcmp(&sent[0][3], txBuf, 5));
    run(3); assert(nSent == 1);                            /* waits for the flow control */
    fc(0, 0, 0); run(1);
    run(8); assert(nSent == 4);
    assert(sent[1][1] == 0x21 && !memcmp(&sent[1][2], txBuf + 5, 6));
    assert(sent[2][1] == 0x22 && !memcmp(&sent[2][2], txBuf + 11, 6));
    assert(sent[3][1] == 0x23 && !memcmp(&sent[3][2], txBuf + 17, 3) && sent[3][5] == 0);
    assert(txConfirmations == 1 && lastTxResult == E_OK && CanTp_IsIdle());

    /* ---- Transmission: block size, SN wrap ------------------------------------------------------------------------------- */
    reset();
    transmit(5 + 6 * 20); run(1);
    fc(0, 4, 0);                                            /* four CFs per block */
    for (unsigned block = 0; block < 5; ++block)
    {
        unsigned before = nSent;
        run(8); assert(nSent == before + 4);
        if (block < 4) { assert(txConfirmations == 0); run(3); assert(nSent == before + 4); fc(0, 4, 0); }
    }
    assert(txConfirmations == 1 && lastTxResult == E_OK && sent[nSent - 1][1] == (uint8_t)(0x20u | (20u & 15u)));
    reset();                                 /* N_Bs: flow control never arrives after the first block */
    transmit(40); run(1); fc(0, 2, 0); run(8); assert(nSent == 3 && txConfirmations == 0);
    run(CANTP_MS_TO_TICKS(1000) + 1u); assert(txConfirmations == 1 && lastTxResult == E_NOT_OK && CanTp_LastTxResult == NTFRSLT_E_TIMEOUT_BS && CanTp_IsIdle());

    /* ---- Transmission: STmin ------------------------------------------------------------------------------------------------- */
    reset();
    transmit(5 + 6 * 3); run(1); fc(0, 0, 10); run(1);       /* 10 ms -> CFs at least 10 ms apart */
    { unsigned at[3], n = 0, base = nSent; for (unsigned t = 1; t < 40 && n < 3; ++t) { unsigned before = nSent; run(1); if (nSent != before) { at[n++] = t; } }
      assert(n == 3 && at[1] - at[0] >= 2u && at[2] - at[1] >= 2u); (void)base; }
    reset();                                  /* reserved STmin values behave like 127 ms and do not abort */
    transmit(5 + 6 * 2); run(1); fc(0, 0, 0x80); run(1);
    { unsigned at[2], n = 0; for (unsigned t = 1; t < 120 && n < 2; ++t) { unsigned before = nSent; run(1); if (nSent != before) { at[n++] = t; } }
      assert(n == 2 && (at[1] - at[0]) * CANTP_MAIN_PERIOD_MS >= 127u && txConfirmations == 1 && lastTxResult == E_OK); }
    reset();
    transmit(5 + 6 * 2); run(1); fc(0, 0, 0xFA); run(1);
    { unsigned at[2], n = 0; for (unsigned t = 1; t < 120 && n < 2; ++t) { unsigned before = nSent; run(1); if (nSent != before) { at[n++] = t; } }
      assert(n == 2 && (at[1] - at[0]) * CANTP_MAIN_PERIOD_MS >= 127u); }
    reset();                                  /* 100..900 us: back-to-back at the main period */
    transmit(5 + 6 * 2); run(1); fc(0, 0, 0xF5); run(1);
    { unsigned at[2], n = 0; for (unsigned t = 1; t < 20 && n < 2; ++t) { unsigned before = nSent; run(1); if (nSent != before) { at[n++] = t; } }
      assert(n == 2 && (at[1] - at[0]) <= 2u); }

    /* ---- Transmission: flow status handling -------------------------------------------------------------------------------------- */
    reset();                                  /* WAIT is tolerated N_WFTmax times and counted per consecutive series */
    transmit(40); run(1);
    fc(1, 0, 0); fc(1, 0, 0); fc(1, 0, 0); assert(txConfirmations == 0);
    fc(0, 2, 0); run(8); assert(nSent == 3);                  /* CTS resets the wait counter */
    fc(1, 0, 0); fc(1, 0, 0); fc(1, 0, 0); assert(txConfirmations == 0);
    fc(1, 0, 0); assert(txConfirmations == 1 && lastTxResult == E_NOT_OK && CanTp_LastTxResult == NTFRSLT_E_WFT_OVRN);
    reset(); transmit(40); run(1); fc(2, 0, 0);   /* overflow */
    assert(txConfirmations == 1 && lastTxResult == E_NOT_OK && CanTp_IsIdle());
    reset(); transmit(40); run(1); fc(3, 0, 0);   /* reserved flow status */
    assert(txConfirmations == 1 && lastTxResult == E_NOT_OK && CanTp_LastTxResult == NTFRSLT_E_INVALID_FS);
    reset(); transmit(40); run(1); fc(0, 0, 0);   /* FC with too few bytes is ignored */
    { uint8_t shortFc[3] = {ME, 0x30, 0}; rx(shortFc, 3); }
    reset();                                       /* FC overtaking our own TX confirmation */
    transmit(20); tick(); assert(nSent == 1); fc(0, 0, 0); confirmAll(); run(8);
    assert(nSent == 4 && txConfirmations == 1 && lastTxResult == E_OK);
    reset();                                                      /* ZGW addresses its FC with the tester address 0x41 */
    transmit(20); run(1);
    frame(0x41, 0x30, 0, 0, 0, 0, 0, 0); run(8); assert(nSent == 4 && txConfirmations == 1 && lastTxResult == E_OK);
    reset(); transmit(20); run(1);
    frame(0x57, 0x30, 0, 0, 0, 0, 0, 0); run(8); assert(nSent == 1 && txConfirmations == 0);        /* any other address is not for us */
    frame(0x41, 0x21, 1, 2, 3, 4, 5, 6); assert(startCalls == 0);                                    /* 0x41 is valid for FC only */
    reset();
    frame(0x41, 0x03, 0x22, 0xF1, 0x01, 0, 0, 0); assert(startCalls == 0 && rxIndications == 0);
    reset();                                                      /* FC without a transfer in progress is ignored */
    fc(0, 0, 0); run(2); assert(nSent == 0 && txConfirmations == 0);

    /* ---- Transmission: lower layer problems ---------------------------------------------------------------------------------------- */
    reset();                                       /* CanIf busy: retried every cycle */
    transmit(5); canifBusy = 1; run(3); assert(nSent == 0 && txConfirmations == 0); canifBusy = 0; run(1);
    assert(nSent == 1 && txConfirmations == 1);
    reset();                                       /* upper layer data not ready: retried */
    transmit(5); copyTxResult = BUFREQ_E_BUSY; run(3); assert(nSent == 0); copyTxResult = BUFREQ_OK; run(1); assert(nSent == 1);
    reset();
    transmit(5); copyTxResult = BUFREQ_E_NOT_OK; run(1); assert(txConfirmations == 1 && lastTxResult == E_NOT_OK && CanTp_IsIdle());
    reset();                                       /* N_As: frame never confirmed */
    transmit(5); tick(); assert(nSent == 1);
    for (unsigned i = 0; i < CANTP_MS_TO_TICKS(1000) + 1u; ++i) { tick(); }
    assert(txConfirmations == 1 && lastTxResult == E_NOT_OK && CanTp_LastTxResult == NTFRSLT_E_TIMEOUT_A && CanTp_IsIdle());
    reset();                                       /* half duplex: no transmission while receiving, no reception while sending */
    frame(ME, 0x10, 20, 1, 2, 3, 4, 5); run(1);
    { PduInfoType i = {0, 0, 5}; assert(CanTp_Transmit(CANTP_TXNSDU_DIAG, &i) == E_NOT_OK); }
    assert(CanTp_CancelReceive(CANTP_RXNSDU_DIAG) == E_OK && rxIndications == 1 && lastRxResult == E_NOT_OK && CanTp_IsIdle());
    reset(); transmit(20); run(1);
    { unsigned starts = startCalls; frame(ME, 0x03, 0x22, 0xF1, 0x01, 0, 0, 0); assert(startCalls == starts); }
    puts("CanTp: ISO 15765-2 SF/FF/CF/FC rules, STmin/BS/WFT handling, timers and error cases passed");
    return 0;
}
