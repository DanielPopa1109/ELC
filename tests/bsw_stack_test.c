/* Whole-stack host test: the production Can/CanIf/CanTp/PduR/Com/CanNm/CanSM/ComM/CanH/Dcm/Dem/NvM/Fee/Fls/
 * DcmAppl/SMon_Calib sources run unmodified. Only the STM32 HAL (CAN mailboxes, Flash) and the OS/EcuM
 * data are mocked. Frames are injected through the CAN driver RX path and observed at the mailbox. */
#include <windows.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "NvM.h"
#include "Dem.h"
#include "Dcm.h"
#include "DcmAppl.h"
#include "SMon.h"
#include "EcuM.h"
#include "CanH.h"
#include "../ELC_APP/BSW/Com/Can/Can.h"
#include "../ELC_APP/BSW/Com/CanIf/CanIf.h"
#include "../ELC_APP/BSW/Com/CanTp/CanTp.h"
#include "../ELC_APP/BSW/Com/PduR/PduR.h"
#include "../ELC_APP/BSW/Com/Com/Com.h"
#include "../ELC_APP/BSW/Com/CanNm/CanNm.h"
#include "../ELC_APP/BSW/Com/CanSM/CanSM.h"
#include "../ELC_APP/BSW/Com/ComM/ComM.h"
#include "../ELC_APP/BSW/Sys/Crc/Crc.h"
#include "../ELC_APP/BSW/Mem/Fee/Fee.h"

/* ================================ mocks ====================================== */
uint32_t mockPrimask;
static CAN_TypeDef registers;
CAN_HandleTypeDef hcan = {&registers};
static uint32_t now, hardwarePending;
static int flashFailure, eraseFailure, readbackFailure, unlocked, resetCount;
static unsigned programCount, eraseCount, canCount;
static uint8_t lastFrame[8], lastLength;
static uint16_t lastId;
static unsigned idCount[0x800];
static uint8_t lastById[0x800][8], lastLenById[0x800];
static uint8_t uds[1200];
static uint16_t udsLength, udsExpected;
static unsigned pendingResponses, positiveClears;
static int autoFc = 1, pauseNvm;

float OS_XCP_CpuLoad = 23;
uint32_t EcuM_TotalResetCounter = 0x12345678, EcuM_TimeActive = 1234, EcuM_TimeWithoutReset = 234;
uint8_t EcuM_ResetReason = 2, EcuM_ResetInfo = 3;
const uint8_t EcuM_SoftwareVersion[4] = {33, 33, 33, 0xFF};
void EcuM_PerformReset(uint8_t reason, uint8_t info) { (void)reason; (void)info; resetCount++; }
void Dem_AppCapture(Dem_FreezeFrame_t *ff)
{
    memset(ff, 0, sizeof *ff); ff->year = 26; ff->month = 10; ff->day = 19; ff->L1VFB = 12000; ff->T30VFB = 12500;
    ff->L1NTC = 24.5f; ff->L1ISENSE = 2.25f;
}

uint32_t HAL_GetTick(void) { return now; }
int HAL_CAN_GetState(CAN_HandleTypeDef *h) { (void)h; return HAL_CAN_STATE_LISTENING; }
int HAL_CAN_IsTxMessagePending(CAN_HandleTypeDef *h, uint32_t mailbox) { (void)h; return !!(hardwarePending & mailbox); }
int HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *h, uint32_t mailbox) { (void)h; hardwarePending &= ~mailbox; return 0; }
uint32_t HAL_CAN_GetRxFifoFillLevel(CAN_HandleTypeDef *h, uint32_t fifo) { (void)h; (void)fifo; return 0; }
int HAL_CAN_GetRxMessage(CAN_HandleTypeDef *h, uint32_t fifo, CAN_RxHeaderTypeDef *header, uint8_t *data)
{ (void)h; (void)fifo; (void)header; (void)data; return HAL_ERROR; }

static void collected(void)
{
    if (udsLength == udsExpected)
    {
        if (udsLength == 3 && uds[0] == 0x7F && uds[2] == 0x78) { pendingResponses++; }
        if (udsLength == 1 && uds[0] == 0x54) { positiveClears++; }
    }
}
int HAL_CAN_AddTxMessage(CAN_HandleTypeDef *h, CAN_TxHeaderTypeDef *header, uint8_t *data, uint32_t *mailbox)
{
    (void)h;
    for (unsigned i = 0; i < 3; ++i)
    {
        if (!(hardwarePending & (1u << i)))
        {
            hardwarePending |= 1u << i; *mailbox = 1u << i;
            lastId = (uint16_t)header->StdId; lastLength = (uint8_t)header->DLC;
            memcpy(lastFrame, data, lastLength); canCount++; idCount[lastId]++;
            memcpy(lastById[lastId], data, lastLength); lastLenById[lastId] = lastLength;
            if (lastId == 0x710)
            {
                uint8_t type;
                assert(data[0] == 0xF0 && lastLength == 8);       /* extended addressing, always padded */
                type = data[1] >> 4;
                if (type == 0) { udsLength = udsExpected = data[1]; memcpy(uds, data + 2, udsLength); collected(); }
                if (type == 1) { udsExpected = (uint16_t)(((data[1] & 15u) << 8) | data[2]); udsLength = 5; memcpy(uds, data + 3, 5); }
                if (type == 2)
                {
                    uint16_t count = (uint16_t)(udsExpected - udsLength); if (count > 6) { count = 6; }
                    memcpy(uds + udsLength, data + 2, count); udsLength = (uint16_t)(udsLength + count); collected();
                }
            }
            return HAL_OK;
        }
    }
    return HAL_ERROR;
}
int HAL_FLASH_Unlock(void) { unlocked = 1; return 0; }
int HAL_FLASH_Lock(void) { unlocked = 0; return 0; }
int HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *e, uint32_t *error)
{
    (void)error;
    assert(unlocked && e->PageAddress >= 0x0800F800u && e->PageAddress + 1024u * e->NbPages <= 0x08010000u);
    eraseCount++; if (eraseFailure) { return 1; }
    memset((void *)(uintptr_t)e->PageAddress, 255, 1024u * e->NbPages); return 0;
}
int HAL_FLASH_Program(uint32_t type, uint32_t address, uint64_t value)
{
    (void)type;
    assert(unlocked && address >= 0x0800F800u && address <= 0x0800FFFCu && !(address & 3u));
    programCount++; if (flashFailure) { return 1; }
    uint32_t *p = (uint32_t *)(uintptr_t)address; assert((*p & (uint32_t)value) == (uint32_t)value); *p &= (uint32_t)value;
    if (readbackFailure) { *p ^= 1; readbackFailure = 0; }
    return 0;
}

/* ============================== test bench =================================== */
static void rxCan(uint16_t id, const uint8_t *d, uint8_t len) { Can_IsrRxIndication(id, len, d); Can_MainFunction_Read(); }
static void completeOne(int success)
{
    for (unsigned i = 0; i < 3; ++i)
    {
        if (hardwarePending & (1u << i))
        {
            hardwarePending &= ~(1u << i); registers.TSR = success ? (2u << (8u * i)) : 0; Can_MainFunction_Write(); break;
        }
    }
}
static void drainBus(void) { while (hardwarePending) { completeOne(1); } }
static void tick(void)   /* 5 ms of the diagnostic task (CAN driver, CanTp, Dcm, Dem, NvM) */
{
    now += 5; completeOne(1); Can_MainFunction_Write(); Can_MainFunction_Read(); Can_MainFunction_BusOff();
    if (autoFc && udsExpected > 6 && udsLength == 5) { uint8_t fc[8] = {0x58, 0x30, 0, 0, 0, 0, 0, 0}; rxCan(0x710, fc, 8); }
    CanTp_MainFunction(); Dcm_MainFunction(); Dem_MainFunction(); if (!pauseNvm && CanTp_IsIdle()) { NvM_MainFunction(); }
}
static void ticks(unsigned n) { while (n--) { tick(); } }
static void request(const uint8_t *p, uint8_t len)
{
    uint8_t f[8] = {0x58, 0, 0, 0, 0, 0, 0, 0}; assert(len <= 6);
    f[1] = len; memcpy(f + 2, p, len);
    udsLength = udsExpected = 0; rxCan(0x710, f, 8); ticks(14);
    for (unsigned n = 0; n < 300 && !CanTp_IsIdle(); ++n) { tick(); }
}
#define REQ(...) do { const uint8_t r_[] = {__VA_ARGS__}; request(r_, (uint8_t)sizeof r_); } while (0)
/* macros, so that a failing assertion reports the line of the test step */
#define expectNrc(sid, nrc) assert(udsLength == 3 && uds[0] == 0x7F && uds[1] == (sid) && uds[2] == (nrc))
#define expectNoResponse() assert(udsLength == 0)
/* a suppressed positive response may still be preceded by NRC 0x78 (never suppressed) */
#define expectNoFinalResponse() assert(udsLength == 0 || (udsLength == 3 && uds[0] == 0x7F && uds[2] == 0x78))
static uint8_t requestCrc(uint8_t alive, uint8_t command) { uint8_t p[3] = {alive, command, 7}; return (uint8_t)Crc_CalculateCRC32(p, 3, 0, 1); }
static void networkTick(void) { CanNm_MainFunction(); ComM_MainFunction(); CanSM_MainFunction(); Com_MainFunctionRx(); Com_MainFunctionTx(); }
static void pumpCom(unsigned n) { while (n--) { Com_MainFunctionTx(); completeOne(1); } }
static ComM_ModeType busMode(void) { ComM_ModeType m; assert(CanSM_GetCurrentComMode(0, &m) == E_OK); return m; }
static void networkRequest(uint8_t pn1) { rxCan(0x3FF, &pn1, 1); networkTick(); networkTick(); }
static uint32_t legacyCrc(const void *data, unsigned bytes)
{
    const uint32_t *p = data; uint32_t crc = 0xFFFFFFFFu;
    for (unsigned w = 0; w < bytes / 4u; ++w) { crc ^= p[w]; for (unsigned b = 0; b < 32; ++b) { crc = (crc << 1) ^ ((crc & 0x80000000u) ? 0x04C11DB7u : 0); } }
    return crc;
}
static unsigned appendLegacy(unsigned offset, uint8_t id, const void *data, uint16_t size)
{
    uint8_t header[8] = {0}; uint32_t crc = legacyCrc(data, size); uint8_t *base = (uint8_t *)0x0800F800u;
    header[0] = id; header[2] = (uint8_t)size; header[3] = (uint8_t)(size >> 8);
    memcpy(base + offset, header, 8); memcpy(base + offset + 8, data, size); memcpy(base + offset + 8 + size, &crc, 4);
    return offset + 16u + size;
}
static uint32_t floatBits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

int main(void)
{
    void *mapped = VirtualAlloc((void *)0x08000000u, 65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t *cal = (uint8_t *)0x08003800u;
    NvM_RequestResultType nvm;
    unsigned count, writes;
    assert(mapped == (void *)0x08000000u);
    memset(mapped, 255, 65536);

    /* ---- start-up: NvM defaults, calibration, no Flash access ---------------------------------------- */
    Can_Init(); CanIf_Init(0); CanTp_Init(0); PduR_Init(0); Com_Init(); CanNm_Init(); CanSM_Init(); ComM_Init(); CanH_Init(); Dcm_Init(0);
    mockPrimask = 1; NvM_Init(); NvM_ReadAll(); SMon_CalibInit(); assert(mockPrimask == 1 && !programCount && !eraseCount); mockPrimask = 0;
    Dem_Init(0);
    for (unsigned i = 0; i < DEM_NUMBER_OF_EVENTS; ++i) { assert(Dem_DTC_Stat[i] == 0x50); }
    assert(NvM_GetErrorStatus(NVM_BLOCK_ID_ALL, &nvm) == E_OK && nvm == NVM_REQ_RESTORED_FROM_ROM);
    assert(NvM_GetErrorStatus(9, &nvm) == E_NOT_OK);
    assert(NvM_WriteAll() == E_OK); ticks(110); assert(NvM_GetErrorStatus(NVM_BLOCK_ID_ALL, &nvm) == E_OK && nvm == NVM_REQ_OK && !unlocked);
    assert(NvM_WriteBlock(0, uds) == E_NOT_OK && NvM_ReadBlock(7, uds) == E_NOT_OK);

    assert(SMon_CalibIntegrity == 0 && SMon_CalibImage[291] == 0xAA && SMon_CalibImage[290] == 0x21 && SMon_CalibGetVersion() == 0x21);
    assert(SMon_P_Varef == 3300u && SMon_P_ADC_MaxValue == 4095u && SMon_P_NTC_PullUp_ResistorVale == 91000u && SMon_P_LowDisTimeCyc == 229600u);
    assert(SMon_P_AlphaFilter == 0.90f && SMon_P_ExternalChargerThreshold == -1.0f && SMon_P_BattRestDetectCurrent_A == 0.30f);
    assert(SMon_P_VFB_T30_TwoPointCalibration_ParamA == 8.16f && SMon_P_NTC_L1_TwoPointCalibration_ParamA == 12332u);
    {
        uint8_t image[292]; uint32_t word;
        memcpy(image, SMon_CalibImage, 292);                              /* defaults image: a valid legacy image */
        memcpy(cal, image, 292); SMon_P_Varef = 0; SMon_CalibInit(); assert(SMon_CalibIntegrity == 1 && SMon_P_Varef == 3300u);
        word = 3000u; memcpy(cal + 4u * 11u, &word, 4); word = floatBits(9.5f); memcpy(cal + 4u * 24u, &word, 4);
        SMon_CalibInit(); assert(SMon_CalibIntegrity == 1 && SMon_P_Varef == 3000u && SMon_P_VFB_T30_TwoPointCalibration_ParamA == 9.5f);
        memcpy(cal, image, 292); word = 0x7FC00000u; memcpy(cal + 4u * 24u, &word, 4);       /* NaN float */
        SMon_CalibInit(); assert(SMon_CalibIntegrity == 0 && SMon_P_VFB_T30_TwoPointCalibration_ParamA == 8.16f);
        memcpy(cal, image, 292); word = 0; memcpy(cal + 4u * 12u, &word, 4);                 /* zero divisor */
        SMon_CalibInit(); assert(SMon_CalibIntegrity == 0 && SMon_P_ADC_MaxValue == 4095u);
        memcpy(cal, image, 292); cal[291] = 0x55; SMon_CalibInit(); assert(SMon_CalibIntegrity == 0);   /* format marker */
        memcpy(cal, image, 292); word = 0x324C4143u; memcpy(image + 4, &word, 4); word = Crc_CalculateCRC32(image + 8, 284, 0, 1); memcpy(image, &word, 4);
        memcpy(cal, image, 292); SMon_CalibInit(); assert(SMon_CalibIntegrity == 2);       /* CAL2 envelope */
        cal[96] ^= 1; SMon_CalibInit(); assert(SMon_CalibIntegrity == 0 && cal[96] != image[96]);   /* corrupted: defaults, page untouched */
        memset(cal, 255, 1024); SMon_CalibInit(); assert(SMon_CalibIntegrity == 0 && SMon_P_Varef == 3300u);   /* erased page: RAM defaults only */
    }
    writes = programCount; assert(NvM_WriteAll() == E_OK); assert(NvM_GetErrorStatus(0, &nvm) == E_OK && nvm == NVM_REQ_OK && programCount == writes);

    /* ---- network state: NM3 -> ComM -> CanSM -> CanIf PDU mode -> Com ------------------------------------ */
    {
        uint8_t nm = 0x40, command[2] = {0xF1, requestCrc(1, 1)}, value = 0;
        rxCan(0x3FF, &nm, 0); networkTick(); assert(busMode() != COMM_FULL_COMMUNICATION);          /* DLC 0 is not NM3 */
        rxCan(0x3FF, &nm, 1); networkTick(); assert(busMode() == COMM_FULL_COMMUNICATION && ComM_IsTxAllowed(0) && ComM_IsRxAllowed(0));
        assert(lastId == 0x051 && lastFrame[0] == 0 && lastLength == 1);        /* state announced when TX opens */
        drainBus();
        rxCan(0x050, command, 2); assert(Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &value) == E_OK && value == 1);
        rxCan(0x050, command, 2); assert(Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &value) == E_OK);   /* ZGW repeats are legal */
        command[1] ^= 1; rxCan(0x050, command, 2); assert(Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &value) == E_NOT_OK && Com_GetE2eErrorCount() == 1);
        command[1] ^= 1; command[0] = (uint8_t)((command[0] & 0x8F) | 0x60); rxCan(0x050, command, 2);   /* wrong data id */
        assert(Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &value) == E_NOT_OK && Com_GetE2eErrorCount() == 2);
        command[0] = 0xF1; rxCan(0x050, command, 3); assert(Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &value) == E_NOT_OK);   /* wrong DLC is counted by COM */
        rxCan(0x050, command, 2); assert(Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &value) == E_OK && Com_GetE2eErrorCount() == 0);
        for (unsigned i = 0; i < 200; ++i) { networkTick(); }
        assert((Com_GetRxStatus(COM_SIGNAL_LOADREQUEST) & COM_RXSTATUS_TIMEOUT) && Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &value) == E_NOT_OK);
        assert(CanNm_GetState() == CANNM_TIMED_OUT && busMode() != COMM_FULL_COMMUNICATION && !ComM_IsTxAllowed(0));
        drainBus(); now += 200;
        rxCan(0x3FF, &nm, 1); networkTick(); networkTick(); assert(busMode() == COMM_FULL_COMMUNICATION); drainBus();
        /* bus-off: application TX stops, diagnostics stay available, recovery restores the requested mode */
        registers.ESR = CAN_ESR_BOFF; Can_MainFunction_BusOff(); assert(busMode() == COMM_SILENT_COMMUNICATION && !ComM_IsTxAllowed(0));
        { uint8_t d[8] = {0}; Can_PduType pdu = {0, 8, 0x6F6, d}; assert(Can_Write(0, &pdu) == E_NOT_OK); }
        registers.ESR = 0; Can_MainFunction_BusOff(); assert(busMode() == COMM_FULL_COMMUNICATION && ComM_IsTxAllowed(0)); drainBus();
        nm = 0; rxCan(0x3FF, &nm, 1); networkTick(); networkTick(); assert(CanNm_GetState() == CANNM_NETWORK_RELEASED && busMode() == COMM_SILENT_COMMUNICATION);
        drainBus();
    }

    /* ---- CAN driver mailboxes, CanIf PDU modes and COM transmission ------------------------------------------ */
    {
        uint8_t nm = 0x40, one = 1, zero = 0, data[8] = {0};
        PduInfoType info = {data, 0, 8};
        networkRequest(nm); drainBus();
        Com_SendSignal(COM_SIGNAL_LOADSTATUS, &one); Com_MainFunctionTx();
        assert(lastId == 0x051 && lastLength == 1 && lastFrame[0] == 1); count = canCount;
        Com_MainFunctionTx(); assert(canCount == count);                         /* one request in flight */
        { unsigned errors = Can_TxErrors; completeOne(0); assert(Can_TxErrors == errors + 1); }   /* physical failure: no confirmation */
        for (unsigned i = 0; i < COM_TX_TIMEOUT_TICKS - 1u; ++i) { Com_MainFunctionTx(); }
        assert(canCount == count);
        Com_MainFunctionTx(); Com_MainFunctionTx(); assert(canCount == count + 1 && lastId == 0x051);   /* re-armed and retried */
        completeOne(1); count = canCount; Com_MainFunctionTx(); assert(canCount == count);              /* confirmed: idle until the cycle */
        for (unsigned i = 0; i < COM_LOADSTATUS_PERIOD; ++i) { Com_MainFunctionTx(); } assert(canCount == count + 1 && lastFrame[0] == 1); completeOne(1);
        Com_SendSignal(COM_SIGNAL_LOADSTATUS, &zero); count = canCount; Com_MainFunctionTx(); assert(canCount == count + 1 && lastFrame[0] == 0); completeOne(1);   /* change triggers */
        Com_SendSignal(COM_SIGNAL_LOADSTATUS, &one); Com_MainFunctionTx(); Com_SendSignal(COM_SIGNAL_LOADSTATUS, &zero);   /* newer value queued while in flight */
        completeOne(1); count = canCount; Com_MainFunctionTx(); assert(canCount == count + 1 && lastFrame[0] == 0); completeOne(1);

        /* driver: one mailbox per identifier, timeout abort without confirmation */
        { Can_PduType pdu = {1, 8, 0x6F6, data}; assert(Can_Write(0, &pdu) == E_OK); assert(Can_Write(0, &pdu) == CAN_BUSY);
          pdu.length = 9; assert(Can_Write(0, &pdu) == E_NOT_OK); pdu.length = 8; pdu.id = 0x800; assert(Can_Write(0, &pdu) == E_NOT_OK); }
        now += 101; Can_MainFunction_Write(); assert(!hardwarePending);
        /* CanIf: DLC contract and PDU mode */
        info.SduLength = 8; assert(CanIf_Transmit(CANIF_TXPDU_TELEMETRY_FIRST + 7, &info) == E_OK);
        assert(CanIf_Transmit(CANIF_TXPDU_TELEMETRY_FIRST + 8, &info) == E_OK);
        info.SduLength = 7; assert(CanIf_Transmit(CANIF_TXPDU_TELEMETRY_FIRST + 3, &info) == E_NOT_OK);   /* 0x6F1 needs 8 */
        info.SduLength = 4; assert(CanIf_Transmit(CANIF_TXPDU_TELEMETRY_FIRST + 2, &info) == E_OK);     /* 0x6F0 is a 4 byte frame */
        drainBus();
        info.SduLength = 3; assert(CanIf_Transmit(CANIF_TXPDU_XCP_RES, &info) == E_OK && lastId == 0x7C1 && lastLength == 3);   /* XCP: DLC 1..8 */
        info.SduLength = 9; assert(CanIf_Transmit(CANIF_TXPDU_XCP_RES, &info) == E_NOT_OK && CanIf_Transmit(99, &info) == E_NOT_OK);
        drainBus();
        assert(CanIf_SetPduMode(0, CANIF_SET_TX_OFFLINE) == E_OK);
        info.SduLength = 8; assert(CanIf_Transmit(CANIF_TXPDU_TELEMETRY_FIRST + 8, &info) == E_NOT_OK);   /* application PDU offline */
        { uint8_t diag[8] = {0xF0, 0x01, 0x7E, 0, 0, 0, 0, 0}; PduInfoType d = {diag, 0, 8};
          assert(CanIf_Transmit(CANIF_TXPDU_DIAG_RESP, &d) == E_OK); drainBus(); }                         /* diagnostics unaffected */
        assert(CanIf_SetPduMode(0, CANIF_SET_RX_OFFLINE) == E_OK);
        { uint8_t cmd[2] = {0xF3, requestCrc(3, 1)}, v;
          rxCan(0x050, cmd, 2); assert(Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &v) == E_NOT_OK);        /* application RX offline */
          assert(CanIf_SetPduMode(0, CANIF_SET_ONLINE) == E_OK);
          rxCan(0x050, cmd, 2); assert(Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &v) == E_OK && v == 1); }
        assert(CanIf_SetPduMode(1, CANIF_SET_ONLINE) == E_NOT_OK);
        { CanIf_PduModeType m; assert(CanIf_GetPduMode(0, &m) == E_OK && m == CANIF_ONLINE); }

        /* diagnostic frames share the three mailboxes with application traffic */
        { Can_PduType a = {1, 8, 0x6F5, data}, b = {2, 8, 0x6F6, data}; assert(Can_Write(0, &a) == E_OK && Can_Write(0, &b) == E_OK); }
        udsLength = udsExpected = 0; REQ(0x22, 0xF1, 0x80); assert(udsLength == 7 && uds[0] == 0x62 && uds[3] == 33 && uds[6] == 0xFF && CanTp_IsIdle());
        drainBus();
    }

    /* ---- UDS: sessions, data identifiers, communication control -------------------------------------------------- */
    REQ(0x10, 0x03); assert(udsLength == 6 && !memcmp(uds, "\x50\x03\x00\x32\x01\xF4", 6));
    REQ(0x22, 0xF1, 0x01);
    assert(udsLength == 9 && !memcmp(uds, "\x62\xF1\x01\x21\x21\x21\x21\x21\x21", 9)); /* FCD/ZGW: APP + FBL versions */
    REQ(0x22, 0xF0, 0x01); assert(udsLength == 7 && uds[1] == 0xF0 && uds[3] == 0x78 && uds[6] == 0x12); /* reset counter */
    REQ(0x22, 0xF1, 0x05); assert(udsLength == 5 && uds[3] == 2 && uds[4] == 3);
    REQ(0x22, 0xF1, 0x06); assert(udsLength == 4 && uds[3] == 23);
    REQ(0x22, 0xF1, 0x86); assert(udsLength == 4 && uds[3] == 3);                                   /* active diagnostic session */
    REQ(0x22, 0xF1, 0x00); assert(udsLength == 4 && uds[3] == 1);
    SMon_L1ST = 0; REQ(0x22, 0xF1, 0x1D); assert(udsLength == 4 && uds[3] == 0);
    REQ(0x22, 0xF2, 0x01); expectNrc(0x22, 0x31);
    REQ(0x22, 0xF1, 0x01, 0xF2, 0x01); expectNrc(0x22, 0x31);
    REQ(0x22, 0xF1); expectNrc(0x22, 0x13);
    REQ(0x22, 0xF1, 0x01, 0xF1); expectNrc(0x22, 0x13);
    { uint8_t ff[8] = {0x58, 0x10, 7, 0x22, 0xF1, 1, 0xF1, 3}, cf[8] = {0x58, 0x21, 0xF1, 4, 0, 0, 0, 0};   /* multi-DID request, FF + CF */
      udsLength = udsExpected = 0; rxCan(0x710, ff, 8); ticks(3); assert(lastFrame[1] == 0x30 && lastFrame[2] == 8 && lastFrame[3] == 5);
      rxCan(0x710, cf, 8); ticks(30); assert(udsLength == 21 && uds[0] == 0x62 && uds[9] == 0xF1 && uds[10] == 3);
      rxCan(0x710, ff, 8); ticks(3); cf[1] = 0x22; udsLength = udsExpected = 0; rxCan(0x710, cf, 8); ticks(5); assert(!udsLength); }   /* wrong SN: no response */
    REQ(0x28, 0x01, 0x03); assert(udsLength == 2 && uds[0] == 0x68 && uds[1] == 1 && !ComM_IsTxAllowed(0) && ComM_IsRxAllowed(0));
    { uint8_t live[2] = {0xF2, requestCrc(2, 1)}, v; rxCan(0x050, live, 2); assert(Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &v) == E_OK); }
    count = canCount; { uint8_t zero = 0, one = 1; Com_SendSignal(COM_SIGNAL_LOADSTATUS, &one); Com_SendSignal(COM_SIGNAL_LOADSTATUS, &zero); }
    Com_MainFunctionTx(); assert(canCount == count);                                                   /* TX disabled by CommunicationControl */
    REQ(0x28, 0x02, 0x03); assert(uds[0] == 0x68 && !ComM_IsRxAllowed(0));
    { uint8_t live[2] = {0x74, requestCrc(4, 0)}, v = 9; rxCan(0x050, live, 2); assert(Com_ReceiveSignal(COM_SIGNAL_LOADREQUEST, &v) == E_OK && v == 1); }   /* RX gated: old value kept */
    REQ(0x28, 0x03, 0x03); assert(uds[0] == 0x68 && !ComM_IsRxAllowed(0) && !ComM_IsTxAllowed(0));
    REQ(0x28, 0x00, 0x01); assert(uds[0] == 0x68 && ComM_IsRxAllowed(0) && ComM_IsTxAllowed(0));        /* normal messages only */
    REQ(0x28, 0x03, 0x02); assert(uds[0] == 0x68);                                                    /* NM only: normal traffic untouched */
    { uint8_t nm = 0x40; unsigned n = (unsigned)CanNm_GetInactiveTicks(); rxCan(0x3FF, &nm, 1); assert(CanNm_GetInactiveTicks() == n); }   /* NM RX disabled */
    REQ(0x28, 0x00, 0x02); { uint8_t nm = 0x40; rxCan(0x3FF, &nm, 1); assert(CanNm_GetInactiveTicks() == 0); }
    REQ(0x28, 0x00, 0x03); assert(uds[0] == 0x68); drainBus();
    REQ(0x28, 0x00, 0x04); expectNrc(0x28, 0x31);
    REQ(0x28, 0x00, 0x00); expectNrc(0x28, 0x31);
    REQ(0x28, 0x00, 0x13); expectNrc(0x28, 0x31);
    REQ(0x28, 0x04, 0x01); expectNrc(0x28, 0x12);
    REQ(0x28, 0x00); expectNrc(0x28, 0x13);
    REQ(0x85, 0x02); assert(udsLength == 2 && uds[0] == 0xC5 && uds[1] == 2 && !Dem_IsDtcSettingEnabled());
    Dem_SetEventStatus(DEM_EVENT_OV_KL30, DEM_EVENT_STATUS_FAILED); assert(!Dem_IsEventFailed(DEM_EVENT_OV_KL30));   /* frozen */
    REQ(0x85, 0x01); assert(uds[0] == 0xC5 && Dem_IsDtcSettingEnabled());
    REQ(0x85, 0x03); expectNrc(0x85, 0x12);
    REQ(0x85); expectNrc(0x85, 0x13);
    REQ(0x99); expectNrc(0x99, 0x11);
    REQ(0x27, 0x01); expectNrc(0x27, 0x11);                                                           /* SecurityAccess is not offered */

    /* ---- UDS: sub-function handling, tester present, routines ------------------------------------------------------ */
    REQ(0x3E, 0x00); assert(udsLength == 2 && uds[0] == 0x7E && uds[1] == 0);
    REQ(0x3E, 0x80); expectNoResponse();                                                              /* suppressPosRspMsgIndicationBit */
    REQ(0x3E, 0x01); expectNrc(0x3E, 0x12);
    REQ(0x3E, 0x81); expectNrc(0x3E, 0x12);                                                           /* NRCs are never suppressed */
    REQ(0x3E); expectNrc(0x3E, 0x13);
    REQ(0x10, 0x83); expectNoResponse(); assert(Dcm_ActiveSessionState == 3);                         /* extended, silent */
    REQ(0x10, 0x04); expectNrc(0x10, 0x12);
    REQ(0x10); expectNrc(0x10, 0x13);
    SMon_RetryCnt = 5; SMon_LockSupply = 1; REQ(0x31, 0x01, 0x00, 0x40); assert(udsLength == 4 && !memcmp(uds, "\x71\x01\x00\x40", 4) && !SMon_RetryCnt && !SMon_LockSupply);
    REQ(0x31, 0x01, 0x00, 0x42, 0x01, 0x02); assert(!memcmp(uds, "\x71\x01\x00\x42", 4) && DcmAppl_LoadStatus == 1);
    ticks(300); assert(DcmAppl_LoadStatus == 1); ticks(150); assert(DcmAppl_LoadStatus == DCMAPPL_LOADSTATUS_NONE);   /* 2 s (400 ticks of 5 ms), not 10 s */
    REQ(0x31, 0x01, 0x00, 0x42, 0x02, 0x05); expectNrc(0x31, 0x31);
    REQ(0x31, 0x01, 0x00, 0x42, 0x01, 0x00); expectNrc(0x31, 0x31);
    REQ(0x31, 0x01, 0x00, 0x42, 0x01); expectNrc(0x31, 0x13);
    REQ(0x31, 0x01, 0x00, 0x40, 0x00); expectNrc(0x31, 0x13);
    REQ(0x31, 0x02, 0x00, 0x40); expectNrc(0x31, 0x12);
    REQ(0x31, 0x01, 0x00, 0x99); expectNrc(0x31, 0x31);
    REQ(0x31, 0x01, 0x00); expectNrc(0x31, 0x13);
    REQ(0x10, 0x01); assert(udsLength == 6 && uds[1] == 1);
    REQ(0x28, 0x00, 0x03); expectNrc(0x28, 0x7F);                                                     /* service not available in the default session */
    REQ(0x85, 0x01); expectNrc(0x85, 0x7F);
    REQ(0x31, 0x01, 0x00, 0x40); expectNrc(0x31, 0x7F);
    REQ(0x10, 0x02); expectNrc(0x10, 0x7E);                                                           /* programming only from the extended session */

    /* ---- UDS: ECU reset and programming session entry ---------------------------------------------------------------- */
    { unsigned resets = (unsigned)resetCount;
      Dem_SetEventStatus(DEM_EVENT_SHORT_TO_BATT_L1_P3, DEM_EVENT_STATUS_FAILED);                       /* something new to persist first */
      REQ(0x11, 0x01); ticks(150);                                                                       /* answered after the NvM write */
      assert(udsLength == 2 && uds[0] == 0x51 && uds[1] == 1 && (unsigned)resetCount == resets + 1 && Dcm_ActiveSessionState == 1);
      Dem_SetEventStatus(DEM_EVENT_SHORT_TO_BATT_L1_P3, DEM_EVENT_STATUS_PASSED);
      REQ(0x11, 0x81); ticks(150); expectNoFinalResponse(); assert((unsigned)resetCount == resets + 2);    /* suppressed response, reset still happens */
      REQ(0x11, 0x02); expectNrc(0x11, 0x12); REQ(0x11); expectNrc(0x11, 0x13); assert((unsigned)resetCount == resets + 2);
      SMon_L1ST = 1; REQ(0x11, 0x01); expectNrc(0x11, 0x22); SMon_L1ST = 0; assert((unsigned)resetCount == resets + 2);   /* Flash work not allowed */
      REQ(0x10, 0x03); REQ(0x10, 0x02); expectNrc(0x10, 0x22);                                         /* vehicle state unknown */
      { uint8_t vs = 6; rxCan(0x3A0, &vs, 1); }
      Dem_SetEventStatus(DEM_EVENT_SHORT_TO_BATT_L1_P3, DEM_EVENT_STATUS_FAILED);
      pauseNvm = 1; REQ(0x10, 0x02); ticks(12); assert(pendingResponses >= 1 && udsLength == 3 && uds[2] == 0x78 && (unsigned)resetCount == resets + 2);
      { PduLengthType size; assert(Dcm_StartOfReception(0, 0, 3, &size) == BUFREQ_E_BUSY); }       /* busy while a request is pending */
      pauseNvm = 0; ticks(110);
      assert(udsLength == 6 && !memcmp(uds, "\x50\x02\x00\x32\x01\xF4", 6) && (unsigned)resetCount == resets + 3 && Dcm_ActiveSessionState == 2);
      Dcm_Init(0); assert(Dcm_ActiveSessionState == 1); }
    { uint8_t vs = 5; rxCan(0x3A0, &vs, 1); REQ(0x10, 0x03); REQ(0x10, 0x02); expectNrc(0x10, 0x22); vs = 6; rxCan(0x3A0, &vs, 1); }
    { PduLengthType size = 0; assert(Dcm_StartOfReception(0, 0, 257, &size) == BUFREQ_E_OVFL && Dcm_StartOfReception(0, 0, 0, &size) == BUFREQ_E_NOT_OK);
      assert(Dcm_StartOfReception(1, 0, 3, &size) == BUFREQ_E_NOT_OK); }
    REQ(0x10, 0x01); Dem_SetEventStatus(DEM_EVENT_SHORT_TO_BATT_L1_P3, DEM_EVENT_STATUS_PASSED);

    /* ---- DEM ---------------------------------------------------------------------------------------------------------------------- */
    assert(Dem_SetEventStatus(DEM_NUMBER_OF_EVENTS, DEM_EVENT_STATUS_FAILED) == E_NOT_OK && Dem_SetEventStatus(0, DEM_EVENT_STATUS_PREFAILED) == E_NOT_OK);
    Dem_SetEventStatus(DEM_EVENT_UV_KL30, DEM_EVENT_STATUS_FAILED); Dem_SetEventStatus(DEM_EVENT_UV_KL30, DEM_EVENT_STATUS_FAILED);
    { uint8_t st; boolean failed; Dem_DTCType dtc;
      assert(Dem_GetEventUdsStatus(0, &st) == E_OK && st == 0x2F && Dem_FF[0].occurrenceCnt == 1);
      assert(Dem_GetEventFailed(0, &failed) == E_OK && failed && Dem_GetDTCOfEvent(DEM_EVENT_XCP_USED, DEM_DTC_FORMAT_UDS, &dtc) == E_OK && dtc == 0x61u);
      ticks(110); writes = programCount; Dem_SetEventStatus(DEM_EVENT_UV_KL30, DEM_EVENT_STATUS_FAILED); ticks(10); assert(programCount == writes);
      REQ(0x19, 0x02, 0x01); assert(udsLength == 7 && !memcmp(uds, "\x59\x02\x7F\x00\x00\x50\x2F", 7));        /* testFailed */
      REQ(0x19, 0x02, 0xFF); assert(udsLength == 3 + 4 * DEM_NUMBER_OF_EVENTS && !memcmp(&uds[3], "\x00\x00\x50\x2F\x00\x00\x51\x50", 8));   /* every event has a status bit set */
      REQ(0x19, 0x02, 0x10); assert(udsLength == 3 + 4 * (DEM_NUMBER_OF_EVENTS - 2u));                  /* UV_KL30 (failed) and P3 (passed) have been tested since the last clear */
      REQ(0x19, 0x01, 0x01); assert(udsLength == 6 && !memcmp(uds, "\x59\x01\x7F\x01\x00\x01", 6));
      REQ(0x19, 0x01, 0xFF); assert(udsLength == 6 && !memcmp(uds, "\x59\x01\x7F\x01\x00\x12", 6));
      REQ(0x19, 0x04, 0x00, 0x00, 0x50, 0x01); ticks(50);
      assert(uds[0] == 0x59 && uds[1] == 4 && udsLength == 2 + 5 + 46 && !memcmp(&uds[2], "\x00\x00\x50\x2F\x01\x0C", 6));
      assert(!memcmp(&uds[8], "\xF3\x00\x00\x01\xF3\x01\x1A", 7));                                    /* occurrence count big endian, year */
      assert(!memcmp(&uds[33], "\xF3\x08\x2E\xE0\xF3\x09\x30\xD4\xF3\x0A\x00\x00\xC4\x41\xF3\x0B\x00\x00\x10\x40", 20));   /* L1VFB, T30VFB, raw floats */
      REQ(0x19, 0x04, 0x00, 0x00, 0x50, 0xFF); ticks(50); assert(uds[0] == 0x59 && udsLength == 2 + 5 + 46);
      REQ(0x19, 0x04, 0x00, 0x00, 0x50, 0x02); expectNrc(0x19, 0x31);
      REQ(0x19, 0x04, 0x00, 0x00, 0x51, 0x01); expectNrc(0x19, 0x31);                                  /* no record stored */
      REQ(0x19, 0x04, 0x00, 0x00, 0x99, 0x01); expectNrc(0x19, 0x31);
      REQ(0x19, 0x04, 0x00, 0x00, 0x50); expectNrc(0x19, 0x13);
      REQ(0x19, 0x06, 0x00, 0x00, 0x50, 0x01);
      assert(udsLength == 9 && !memcmp(uds, "\x59\x06\x00\x00\x50\x2F\x01\x00\x01", 9));
      REQ(0x19, 0x06, 0x00, 0x00, 0x50, 0xFF); assert(udsLength == 9 && uds[6] == 1); /* all configured records */
      REQ(0x19, 0x06, 0x00, 0x00, 0x50, 0x02); expectNrc(0x19, 0x31);
      REQ(0x19, 0x06, 0x00, 0x00, 0x51, 0x01); expectNrc(0x19, 0x31); /* no occurrence */
      REQ(0x19, 0x06, 0x00, 0x00, 0x99, 0x01); expectNrc(0x19, 0x31);
      REQ(0x19, 0x06, 0x00, 0x00, 0x50); expectNrc(0x19, 0x13);
      REQ(0x19, 0x05, 0x01); expectNrc(0x19, 0x12);
      REQ(0x19, 0x02); expectNrc(0x19, 0x13);
      REQ(0x19); expectNrc(0x19, 0x13);
      Dem_SetEventStatus(DEM_EVENT_UV_KL30, DEM_EVENT_STATUS_PASSED); Dem_SetOperationCycleState(0, DEM_CYCLE_STATE_END);
      Dem_SetOperationCycleState(0, DEM_CYCLE_STATE_START);
      assert(Dem_GetEventUdsStatus(0, &st) == E_OK && (st & 0x40) && !(st & 2) && (st & 0x08) && (st & 0x20) && !(st & 1));
      assert(Dem_SetOperationCycleState(1, DEM_CYCLE_STATE_START) == E_NOT_OK);
      ticks(110); memset(Dem_DTC_Stat, 0, sizeof Dem_DTC_Stat); memset(Dem_FF, 0, sizeof Dem_FF); NvM_ReadAll(); Dem_Init(0); assert(Dem_FF[0].occurrenceCnt == 1);
      /* operation cycle: pendingDTC is cleared by a completed passing cycle */
      Dem_SetEventStatus(DEM_EVENT_OV_KL30, DEM_EVENT_STATUS_FAILED); Dem_SetEventStatus(DEM_EVENT_OV_KL30, DEM_EVENT_STATUS_PASSED);
      Dem_SetOperationCycleState(0, DEM_CYCLE_STATE_END); assert(Dem_GetEventUdsStatus(1, &st) == E_OK && (st & 0x04));    /* failed this cycle: still pending */
      Dem_SetOperationCycleState(0, DEM_CYCLE_STATE_START); Dem_SetEventStatus(DEM_EVENT_OV_KL30, DEM_EVENT_STATUS_PASSED);
      Dem_SetOperationCycleState(0, DEM_CYCLE_STATE_END); assert(Dem_GetEventUdsStatus(1, &st) == E_OK && !(st & 0x04) && (st & 0x08)); }

    /* ---- Clear DTC: asynchronous, answered only after the cleared state is durable ----------------------------------------- */
    { unsigned cleared = positiveClears;
      SMon_L1ST = 1; REQ(0x14, 0xFF, 0xFF, 0xFF); expectNrc(0x14, 0x22); SMon_L1ST = 0;
      REQ(0x14, 0x00, 0x00, 0x99); expectNrc(0x14, 0x31); REQ(0x14, 0xFF, 0xFF); expectNrc(0x14, 0x13);
      REQ(0x10, 0x03);
      pauseNvm = 1; REQ(0x14, 0xFF, 0xFF, 0xFF); ticks(12); assert(pendingResponses && positiveClears == cleared);
      { unsigned before = pendingResponses; ticks(920); assert(pendingResponses > before && Dcm_ActiveSessionState == 3); }
      pauseNvm = 0; ticks(250); assert(positiveClears == cleared + 1 && Dem_DTC_Stat[0] == 0x50 && Dem_FF[0].occurrenceCnt == 0);   /* waits for the running write, then commits */
      Dem_SetEventStatus(DEM_EVENT_UV_KL30, DEM_EVENT_STATUS_FAILED); ticks(110);
      flashFailure = 1; REQ(0x14, 0xFF, 0xFF, 0xFF); ticks(250);
      assert(udsLength == 3 && uds[0] == 0x7F && uds[1] == 0x14 && uds[2] == 0x72 && positiveClears == cleared + 1 && !unlocked); flashFailure = 0;
      NvM_ReadAll(); Dem_Init(0); assert(Dem_FF[0].occurrenceCnt == 1);                                  /* failed clear never became durable */
      REQ(0x14, 0x00, 0x00, 0x50); ticks(250); assert(positiveClears == cleared + 2 && Dem_FF[0].occurrenceCnt == 0);
      /* Dem API: repeated calls report progress; reports during the clear are applied afterwards. */
      assert(Dem_ClearDTC(DEM_DTC_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY) == DEM_CLEAR_PENDING);
      Dem_SetEventStatus(DEM_EVENT_UV_KL30, DEM_EVENT_STATUS_FAILED); Dem_SetEventStatus(DEM_EVENT_OV_KL30, DEM_EVENT_STATUS_FAILED);
      assert(!Dem_IsEventFailed(DEM_EVENT_UV_KL30) && !Dem_IsEventFailed(DEM_EVENT_OV_KL30));
      assert(Dem_ClearDTC(0x50, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY) == DEM_CLEAR_PENDING);
      { Dem_ReturnClearDTCType r; unsigned n = 0; while ((r = Dem_ClearDTC(0, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY)) == DEM_CLEAR_PENDING && n++ < 200) { tick(); }
        assert(r == DEM_CLEAR_OK); }
      assert(Dem_IsEventFailed(DEM_EVENT_UV_KL30) && Dem_IsEventFailed(DEM_EVENT_OV_KL30));
      assert(Dem_ClearDTC(0x50, DEM_DTC_FORMAT_OBD, DEM_DTC_ORIGIN_PRIMARY_MEMORY) == DEM_CLEAR_WRONG_DTC);
      assert(Dem_ClearDTC(0x50, DEM_DTC_FORMAT_UDS, 2) == DEM_CLEAR_WRONG_DTCORIGIN);
      ticks(110); }

    /* ---- session timer and transport timeout recovery ---------------------------------------------------------------------- */
    autoFc = 0; REQ(0x22, 0xF1, 0x80); ticks(210); assert(CanTp_IsIdle()); autoFc = 1;                  /* tester never sends FC */
    REQ(0x10, 0x03); REQ(0x28, 0x02, 0x03); REQ(0x85, 0x02); assert(!ComM_IsRxAllowed(0) && !Dem_IsDtcSettingEnabled());
    ticks(900); assert(Dcm_ActiveSessionState == 3); REQ(0x3E, 0x80); ticks(900); assert(Dcm_ActiveSessionState == 3);   /* TesterPresent restarts S3 */
    ticks(200); assert(Dcm_ActiveSessionState == 1 && ComM_IsRxAllowed(0) && Dem_IsDtcSettingEnabled());                  /* S3 expiry restores the defaults */

    /* ---- CanH: XCP slave, time stamp, load/status logic and telemetry --------------------------------------------------------- */
    {
        uint8_t nm = 0x40, connect[8] = {0xFF}, getStatus[8] = {0xFD}, bad[8] = {0x01}, disconnect[8] = {0xFE};
        uint8_t t[4];
        uint32_t stamp = 26u | (10u << 6) | (19u << 10) | (13u << 15) | (42u << 20) | (7u << 26);
        networkRequest(nm); drainBus(); assert(CanH_CommunicationState != FULL_COMMUNICATION);
        CanH_MainFunction(); assert(CanH_CommunicationState == FULL_COMMUNICATION && CanH_NM3PN1_Value == 0x40);
        pumpCom(40); drainBus(); idCount[0x7C1] = 0;
        rxCan(0x7C0, connect, 8); assert(Dem_IsEventFailed(DEM_EVENT_XCP_USED) && Xcp_State.connected); Xcp_MainFunction();
        assert(lastId == 0x7C1 && lastLength == 8 && lastFrame[0] == 0xFF && lastFrame[1] == 0x01 && lastFrame[3] == 8);
        rxCan(0x7C0, getStatus, 8); Xcp_MainFunction(); assert(idCount[0x7C1] == 1);                    /* previous response still pending */
        completeOne(0);                                                                                    /* lost: retransmitted after the timeout */
        for (unsigned i = 0; i < 149; ++i) { Xcp_MainFunction(); } assert(idCount[0x7C1] == 1);
        Xcp_MainFunction(); Xcp_MainFunction(); assert(idCount[0x7C1] == 2); completeOne(1); Xcp_MainFunction(); assert(idCount[0x7C1] == 2);
        rxCan(0x7C0, getStatus, 8); Xcp_MainFunction(); assert(idCount[0x7C1] == 3 && lastFrame[0] == 0xFF && lastFrame[1] == 1); completeOne(1);
        rxCan(0x7C0, bad, 8); Xcp_MainFunction(); assert(lastFrame[0] == 0xFE && lastFrame[1] == 0x20); completeOne(1);
        rxCan(0x7C0, disconnect, 8); Xcp_MainFunction(); assert(!Xcp_State.connected && !Dem_IsEventFailed(DEM_EVENT_XCP_USED)); completeOne(1);
        ComM_CommunicationControl(0, 1, 1); count = idCount[0x7C1]; rxCan(0x7C0, getStatus, 8); Xcp_MainFunction(); assert(idCount[0x7C1] == count);   /* TX disabled */
        ComM_CommunicationControl(0, 2, 1); rxCan(0x7C0, connect, 8); assert(!Xcp_State.connected);                                               /* RX disabled */
        ComM_CommunicationControl(0, 0, 3); drainBus();
        t[0] = (uint8_t)stamp; t[1] = (uint8_t)(stamp >> 8); t[2] = (uint8_t)(stamp >> 16); t[3] = (uint8_t)(stamp >> 24);
        rxCan(0x202, t, 4); assert(CSBSDAT_Year == 26 && CSBSDAT_Month == 10 && CSBSDAT_Day == 19 && CSBSDAT_Hour == 13 && CSBSDAT_Minute == 42 && CSBSDAT_Second == 7);
        rxCan(0x202, t, 3); rxCan(0x7C0, 0, 0);

        /* load status and telemetry travel through Com/PduR/CanIf */
        SMon_L1ST = 1; CanH_MainFunction(); pumpCom(40); assert(lastById[0x051][0] == 1);
        SMon_Battery.Charge_Ah_Acc = 1.5f; SMon_Battery.Discharge_Ah_Acc = 2.5f; SMon_Battery.SoC_pct = 55.5f; SMon_Battery.Charging = 1; SMon_Battery.WeakBattery = 1;
        SMon_VfbT30 = 0x1234; SMon_VfbL1 = 0x5678; SMon_ISenseL1_Float = 3.25f; SMon_I2TCounter = 0x01020304u; SMon_NTC_Temperature_L1 = 31.5f; SMon_McuTempValue = 41.25f;
        CanH_PublishMeasurements(); pumpCom(40);
        memset(idCount, 0, sizeof idCount); CanH_MainCounter = 0;
        CanH_MainFunction(); pumpCom(20);                                                                  /* counter 0: battery frames, no fast frames yet */
        for (unsigned id = 0x6F1; id <= 0x6F7; ++id) { assert(idCount[id] == 1); }
        assert(idCount[0x6EE] == 0 && idCount[0x6EF] == 0 && idCount[0x6F0] == 0);
        { float f; memcpy(&f, &lastById[0x6F1][0], 4); assert(f == 1.5f); memcpy(&f, &lastById[0x6F1][4], 4); assert(f == 2.5f); }
        { float f; memcpy(&f, &lastById[0x6F3][0], 4); assert(f == 55.5f); }
        assert(lastLenById[0x6F7] == 1 && lastById[0x6F7][0] == (0x02 | 0x80) && lastLenById[0x6F1] == 8);
        CanH_MainFunction(); pumpCom(20);                                                                  /* counter 1: fast frames only */
        assert(idCount[0x6EE] == 1 && idCount[0x6EF] == 1 && idCount[0x6F0] == 1 && idCount[0x6F1] == 1);
        assert(lastLenById[0x6EF] == 8 && lastById[0x6EF][0] == 0x34 && lastById[0x6EF][1] == 0x12 && lastById[0x6EF][2] == 0x78 && lastById[0x6EF][3] == 0x56);
        { float f; memcpy(&f, &lastById[0x6EF][4], 4); assert(f == 3.25f); memcpy(&f, &lastById[0x6EE][0], 4); assert(f == 31.5f); memcpy(&f, &lastById[0x6EE][4], 4); assert(f == 41.25f); }
        assert(lastLenById[0x6F0] == 4 && lastById[0x6F0][0] == 4 && lastById[0x6F0][3] == 1);
        for (unsigned i = 0; i < 38; ++i) { CanH_MainFunction(); pumpCom(20); } assert(idCount[0x6F1] == 1);   /* battery frames every 40 cycles */
        CanH_MainFunction(); pumpCom(20); assert(idCount[0x6F1] == 2);
        { uint8_t zero = 0; networkRequest(zero); drainBus(); memset(idCount, 0, sizeof idCount);        /* NM released: no more traffic */
          CanH_MainFunction(); pumpCom(20); assert(idCount[0x6EE] == 0 && idCount[0x6F1] == 0); networkRequest(nm); drainBus(); }

        /* consumption of the Com RX signals: load command, vehicle state and their diagnostics */
        { uint8_t cmd[2] = {0xF5, requestCrc(5, 1)}, vs = 4;
          rxCan(0x050, cmd, 2); rxCan(0x3A0, &vs, 1); CanH_MainFunction(); assert(SMon_CmdStat == 1 && CanH_VehicleStatus == 4 && !CanH_MissingLoadRequest);
          vs = 7; rxCan(0x3A0, &vs, 1); CanH_MainFunction(); assert(CanH_VehicleStatus == 0 && Dem_IsEventFailed(DEM_EVENT_VEHICLESTATUS_INVALID));
          vs = 3; rxCan(0x3A0, &vs, 1); CanH_MainFunction(); assert(CanH_VehicleStatus == 3 && !Dem_IsEventFailed(DEM_EVENT_VEHICLESTATUS_INVALID));
          ComM_CommunicationControl(0, 2, 1); CanH_MainFunction(); assert(SMon_CmdStat == 0); ComM_CommunicationControl(0, 0, 1);               /* RX disabled by UDS */
          cmd[0] = 0xF6; cmd[1] = requestCrc(6, 1); rxCan(0x050, cmd, 2); CanH_MainFunction(); assert(SMon_CmdStat == 1);
          for (unsigned i = 0; i < COM_RX_DEADLINE_TICKS; ++i) { Com_MainFunctionRx(); }
          CanH_MainFunction(); assert(CanH_MissingLoadRequest == 201 && SMon_CmdStat == 0 && Dem_IsEventFailed(DEM_EVENT_LOADREQUEST_MISSING) && Dem_IsEventFailed(DEM_EVENT_VEHICLEDATA_TIMEOUT));
          cmd[0] = 0xF7; cmd[1] = requestCrc(7, 1); rxCan(0x050, cmd, 2); vs = 4; rxCan(0x3A0, &vs, 1); CanH_MainFunction();
          assert(CanH_MissingLoadRequest == 0 && !Dem_IsEventFailed(DEM_EVENT_LOADREQUEST_MISSING) && !Dem_IsEventFailed(DEM_EVENT_VEHICLEDATA_TIMEOUT));
          cmd[1] ^= 1; for (unsigned i = 0; i < 51; ++i) { rxCan(0x050, cmd, 2); } CanH_MainFunction(); assert(Dem_IsEventFailed(DEM_EVENT_LOADREQUEST_E2E) && CanH_E2eErrCnt == 51);
          cmd[1] ^= 1; rxCan(0x050, cmd, 2); CanH_MainFunction(); assert(!Dem_IsEventFailed(DEM_EVENT_LOADREQUEST_E2E)); }
        drainBus(); ticks(110);
    }

    /* ---- DEM broadcast frame (0x7FE) and persistence of freeze frames -------------------------------------------------- */
    {
        { uint8_t nm = 0x40; networkRequest(nm); }
        for (unsigned i = 0; i < 60; ++i) { ticks(2); pumpCom(1); } drainBus(); memset(idCount, 0, sizeof idCount);   /* flush earlier broadcasts */
        Dem_SetEventStatus(DEM_EVENT_L1_LOCKED, DEM_EVENT_STATUS_FAILED); ticks(3); pumpCom(3);
        assert(idCount[0x7FE] == 1 && lastLenById[0x7FE] == 4 && lastById[0x7FE][0] == 0x54 && lastById[0x7FE][1] == 0 && lastById[0x7FE][3] == (uint8_t)((0x2Fu & 0x3Fu) << 2));
    }

    SMon_L1ST = 0;                                                     /* the load output is off again: Flash work is allowed */

    /* ---- legacy journal migration never erases recoverable data ---------------------------------------------------------------------- */
    {
        uint8_t *nv = (uint8_t *)0x0800F800u, beforeMigration[2048]; uint32_t magic = 0xB6B6B6B6u; unsigned end;
        memset(nv, 255, 2048); memcpy(nv, &magic, 4); memcpy(nv + 4, &magic, 4);
        Dem_DTC_Stat[0] = 0x2F; Dem_FF[0].occurrenceCnt = 7; SMon_Battery.SoC_pct = 67.5f;
        end = appendLegacy(8, 1, Dem_DTC_Stat, sizeof Dem_DTC_Stat); end = appendLegacy(end, 2, Dem_FF, sizeof Dem_FF); end = appendLegacy(end, 3, &SMon_Battery, sizeof SMon_Battery); assert(end < 1024);
        NvM_ReadAll(); Dem_Init(0); assert(Dem_FF[0].occurrenceCnt == 7 && SMon_Battery.SoC_pct == 67.5f);
        assert(NvM_WriteAll() == E_OK); NvM_MainFunction(); NvM_MainFunction();                              /* simulate a power cut during migration */
        memset(Dem_FF, 0, sizeof Dem_FF); NvM_ReadAll(); assert(Dem_FF[0].occurrenceCnt == 7);
        assert(NvM_WriteAll() == E_OK); for (unsigned i = 0; i < 110; ++i) { NvM_MainFunction(); } assert(NvM_GetErrorStatus(0, &nvm) == E_OK && nvm == NVM_REQ_OK);
        memset(Dem_FF, 0, sizeof Dem_FF); NvM_ReadAll(); assert(Dem_FF[0].occurrenceCnt == 7);
        { uint8_t block[432]; memset(Dem_FF, 0, sizeof Dem_FF); assert(NvM_ReadBlock(NVM_BLOCK_ID_DEM_FF, block) == E_OK); memcpy(Dem_FF, block, sizeof block); assert(Dem_FF[0].occurrenceCnt == 7); }
        /* the newest legacy record crossing both pages cannot be migrated destructively */
        memset(nv, 255, 2048); memcpy(nv, &magic, 4); memcpy(nv + 4, &magic, 4);
        end = appendLegacy(8, 1, Dem_DTC_Stat, sizeof Dem_DTC_Stat); end = appendLegacy(end, 2, Dem_FF, sizeof Dem_FF);
        end = appendLegacy(end, 3, &SMon_Battery, sizeof SMon_Battery); end = appendLegacy(end, 2, Dem_FF, sizeof Dem_FF); assert(end > 1024);
        memcpy(beforeMigration, nv, 2048); NvM_ReadAll();
        assert(NvM_WriteAll() == E_NOT_OK && NvM_GetErrorStatus(0, &nvm) == E_OK && nvm == NVM_REQ_MIGRATION_REQUIRED && !memcmp(beforeMigration, nv, 2048));
        memset(nv, 0x5A, 2048); NvM_ReadAll(); assert(NvM_WriteAll() == E_NOT_OK && !memcmp(nv, "\x5A\x5A\x5A\x5A", 4));   /* unknown content is retained */
    }
    assert(!mockPrimask && !unlocked);
    puts("Stack: Can/CanIf/CanTp/PduR/Com/NM/ComM/CanSM/CanH/XCP/DCM/DEM/NvM/Fee paths, UDS conformance and failure assertions passed");
    return 0;
}
