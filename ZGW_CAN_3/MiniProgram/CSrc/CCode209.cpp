// GEN BLOCK BEGIN Include
#define TSMP_IMPL
#include "TSMaster.h"
#include "MPLibrary.h"
#include "Database.h"
#include "TSMasterBaseInclude.h"
#include "Configuration.h"
// GEN BLOCK END Include

// CODE BLOCK BEGIN Global_Definitions 
#include <windows.h>
#include <string.h>

#define CAN_TX_CHN                 CH1
#define CAN_STD_ID                 0x3FF
#define CAN_DLC                    1u

/* step() is called every 0.5 ms */
#define STEP_TICK_US               500u
#define MS_TO_TICKS(ms)            ((u32)((ms) * 2u))

#define CAN_BURST_PERIOD_TICKS     MS_TO_TICKS(5u)       /* 5 ms  -> 10 ticks */
#define CAN_BURST_TOTAL_TICKS      MS_TO_TICKS(100u)     /* 100 ms -> 200 ticks */
#define CAN_BURST_COUNT            (CAN_BURST_TOTAL_TICKS / CAN_BURST_PERIOD_TICKS)

#define RELAY_ON_TIME_TICKS        MS_TO_TICKS(500u)     /* 500 ms -> 1000 ticks */
#define POST_BURST_WAIT_TICKS      MS_TO_TICKS(500u)     /* 500 ms -> 1000 ticks */

#define ECU_RESP_TIMEOUT_TICKS     MS_TO_TICKS(100u)     /* 100 ms */
#define ECU_RESP_ID                0x6EFu

#define T_DELAY_START_TICKS        0u                    /* 0 ms */
#define T_DELAY_END_TICKS          MS_TO_TICKS(3000u)    /* 3000 ms -> 7480 ticks */
#define T_DELAY_STEP_TICKS         1u                    /* 0.5 ms increment */

static const char* SV_NM3_TX_STAT = "NM3_TX_STAT";

static const char* SV_START_NM3       = "CWT_NM3";
static const char* SV_START_WUP       = "CWT_WUP";
static const char* SV_START_WUP_NM3   = "CWT_WUP_NM3";

static const char* SV_STATE_NM3       = "CWP_NM3_STATE";
static const char* SV_STATE_WUP       = "CWP_WUP_STATE";
static const char* SV_STATE_WUP_NM3   = "CWP_WUP_NM3_STATE";

static const char* SV_WUP_STAT        = "WUP_STAT";
static const char* RELAY_COM          = "\\\\.\\COM8";

static const u8 RELAY_CMD_ON[4]  = { 0xA0u, 0x01u, 0x01u, 0xA2u };
static const u8 RELAY_CMD_OFF[4] = { 0xA0u, 0x01u, 0x00u, 0xA1u };

enum
{
    MODE_NONE = 0,
    MODE_NM3,
    MODE_WUP,
    MODE_WUP_NM3
};

enum
{
    PHASE_IDLE = 0,
    PHASE_ACTION,
    PHASE_RESP_WAIT,
    PHASE_WAIT
};

static HANDLE gRelayHandle = INVALID_HANDLE_VALUE;

static s32 gInitialized = 0;

static s32 gPrevStartNm3 = 0;
static s32 gPrevStartWup = 0;
static s32 gPrevStartWupNm3 = 0;

static s32 gMode = MODE_NONE;
static s32 gPhase = PHASE_IDLE;

static u32 gTDelayTicks = 0u;
static u32 gPhaseTimerTicks = 0u;

static s32 gRelayIsOn = 0;
static s32 gBurstActive = 0;

static u32 gBurstNextTxTicks = 0u;
static u32 gBurstTxCount = 0u;

static volatile s32 gWaitForEcuResp = 0;
static volatile s32 gEcuRespSeen = 0;

static void set_state_vars(const s32 nm3, const s32 wup, const s32 wup_nm3)
{
    (void)app.set_system_var_int32(SV_STATE_NM3, nm3);
    (void)app.set_system_var_int32(SV_STATE_WUP, wup);
    (void)app.set_system_var_int32(SV_STATE_WUP_NM3, wup_nm3);
}

static void set_wup_stat(const s32 value)
{
    (void)app.set_system_var_int32(SV_WUP_STAT, value);
}

static void close_relay_port(void)
{
    if (gRelayHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(gRelayHandle);
        gRelayHandle = INVALID_HANDLE_VALUE;
    }
}

static s32 open_relay_port(void)
{
    DCB dcb;
    COMMTIMEOUTS timeouts;

    gRelayHandle = CreateFileA(RELAY_COM,
                               GENERIC_READ | GENERIC_WRITE,
                               0,
                               NULL,
                               OPEN_EXISTING,
                               0,
                               NULL);

    if (gRelayHandle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(gRelayHandle, &dcb))
    {
        close_relay_port();
        return 0;
    }

    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;
    dcb.fParity  = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl  = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;
    dcb.fInX  = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(gRelayHandle, &dcb))
    {
        close_relay_port();
        return 0;
    }

    (void)SetupComm(gRelayHandle, 64u, 64u);

    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout         = 10u;
    timeouts.ReadTotalTimeoutConstant    = 10u;
    timeouts.ReadTotalTimeoutMultiplier  = 0u;
    timeouts.WriteTotalTimeoutConstant   = 10u;
    timeouts.WriteTotalTimeoutMultiplier = 0u;

    if (!SetCommTimeouts(gRelayHandle, &timeouts))
    {
        close_relay_port();
        return 0;
    }

    (void)PurgeComm(gRelayHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return 1;
}

static s32 relay_write_raw(const u8* data, const DWORD len)
{
    DWORD written = 0u;

    if (gRelayHandle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    if (!WriteFile(gRelayHandle, data, len, &written, NULL))
    {
        return 0;
    }

    if (written != len)
    {
        return 0;
    }

    (void)FlushFileBuffers(gRelayHandle);
    return 1;
}

static s32 relay_on(void)
{
    return relay_write_raw(RELAY_CMD_ON, 4u);
}

static s32 relay_off(void)
{
    return relay_write_raw(RELAY_CMD_OFF, 4u);
}

static void send_nm3_frame(void)
{
	char buf[64];
sprintf(buf, "TX 0x3FF at phaseTick=%u", gPhaseTimerTicks);
log("TX 0x3FF at phaseTick=%u", gPhaseTimerTicks);
(void)app.set_system_var_int32(SV_NM3_TX_STAT , 1);


    TCAN msg;
    s32 i;

    memset(&msg, 0, sizeof(msg));
    msg.FIdxChn     = CAN_TX_CHN;
    msg.FProperties = MASK_CANProp_DIR_TX;
    msg.FDLC        = CAN_DLC;
    msg.FIdentifier = CAN_STD_ID;
    msg.FTimeUs     = 0;

    for (i = 0; i < 8; i++)
    {
        msg.FData[i] = 0u;
    }

    msg.FData[0] = 0x60u;

    com.transmit_can_async(&msg);
}

static s32 mode_uses_nm3(const s32 mode)
{
    return ((mode == MODE_NM3) || (mode == MODE_WUP_NM3)) ? 1 : 0;
}

static s32 mode_uses_wup(const s32 mode)
{
    return ((mode == MODE_WUP) || (mode == MODE_WUP_NM3)) ? 1 : 0;
}

static void reset_runtime_state(void)
{
    gRelayIsOn = 0;
    gBurstActive = 0;
    gBurstNextTxTicks = 0u;
    gBurstTxCount = 0u;
    gPhaseTimerTicks = 0u;
    gWaitForEcuResp = 0;
    gEcuRespSeen = 0;
    set_wup_stat(0);
}

static void stop_test(const s32 finished)
{
    s32 oldMode = gMode;

    if (gRelayIsOn != 0)
    {
        (void)relay_off();
    }

    close_relay_port();
    reset_runtime_state();

    gMode = MODE_NONE;
    gPhase = PHASE_IDLE;
    gTDelayTicks = T_DELAY_START_TICKS;

    if (finished != 0)
    {
        if (oldMode == MODE_NM3)
        {
            set_state_vars(2, 0, 0);
        }
        else if (oldMode == MODE_WUP)
        {
            set_state_vars(0, 2, 0);
        }
        else if (oldMode == MODE_WUP_NM3)
        {
            set_state_vars(0, 0, 2);
        }
        else
        {
            set_state_vars(0, 0, 0);
        }
    }
    else
    {
        set_state_vars(0, 0, 0);
    }
}

static void start_action_phase(void)
{
    gPhase = PHASE_ACTION;
    gPhaseTimerTicks = 0u;

    gRelayIsOn = 0;
    gBurstActive = 1;
    gBurstTxCount = 1u;
    gBurstNextTxTicks = CAN_BURST_PERIOD_TICKS;
    /* send first frame immediately */
    send_nm3_frame();

    gWaitForEcuResp = 0;
    gEcuRespSeen = 0;
    set_wup_stat(0);

    if (mode_uses_wup(gMode) != 0)
    {
        if (!open_relay_port())
        {
            log_nok("Relay COM open failed");
            stop_test(0);
            return;
        }

        if (!relay_on())
        {
            log_nok("Relay ON command failed");
            close_relay_port();
            stop_test(0);
            return;
        }

        gRelayIsOn = 1;
        set_wup_stat(1);
    }

    if (mode_uses_nm3(gMode) != 0)
    {
        gBurstActive = 1;
        gBurstTxCount = 0u;
        gBurstNextTxTicks = 0u;
    }
}

static void start_resp_wait_phase(void)
{
    gPhase = PHASE_RESP_WAIT;
    gPhaseTimerTicks = 0u;
    gWaitForEcuResp = 1;
    gEcuRespSeen = 0;
}

static void start_wait_phase(void)
{
    gPhase = PHASE_WAIT;
    gPhaseTimerTicks = 0u;
    gWaitForEcuResp = 0;
    gEcuRespSeen = 0;
}

static void start_test(const s32 mode)
{
    gMode = mode;
    gPhase = PHASE_IDLE;
    gTDelayTicks = T_DELAY_START_TICKS;
    reset_runtime_state();

    if (mode == MODE_NM3)
    {
        set_state_vars(1, 0, 0);
    }
    else if (mode == MODE_WUP)
    {
        set_state_vars(0, 1, 0);
    }
    else if (mode == MODE_WUP_NM3)
    {
        set_state_vars(0, 0, 1);
    }
    else
    {
        set_state_vars(0, 0, 0);
    }

    start_action_phase();
}

static s32 active_trigger_is_one(const s32 startNm3, const s32 startWup, const s32 startWupNm3)
{
    if (gMode == MODE_NM3)
    {
        return (startNm3 == 1) ? 1 : 0;
    }
    if (gMode == MODE_WUP)
    {
        return (startWup == 1) ? 1 : 0;
    }
    if (gMode == MODE_WUP_NM3)
    {
        return (startWupNm3 == 1) ? 1 : 0;
    }
    return 0;
}

static s32 action_is_complete(void)
{
    s32 relayDone = 0;
    s32 burstDone = 0;

    if (mode_uses_wup(gMode) != 0)
    {
        relayDone = (gRelayIsOn == 0) ? 1 : 0;
    }
    else
    {
        relayDone = 1;
    }

    if (mode_uses_nm3(gMode) != 0)
    {
        burstDone = (gBurstActive == 0) ? 1 : 0;
    }
    else
    {
        burstDone = 1;
    }

    return ((relayDone != 0) && (burstDone != 0)) ? 1 : 0;
}

static void process_action_phase_1tick(void)
{
    if (mode_uses_nm3(gMode) != 0)
    {
        while ((gBurstActive != 0) &&
               (gBurstTxCount < CAN_BURST_COUNT) &&
               (gPhaseTimerTicks >= gBurstNextTxTicks))
        {
            send_nm3_frame();
            gBurstTxCount++;
            gBurstNextTxTicks += CAN_BURST_PERIOD_TICKS;

            if (gBurstTxCount >= CAN_BURST_COUNT)
            {
                gBurstActive = 0;
            }
        }
    }

    if (mode_uses_wup(gMode) != 0)
    {
        if ((gRelayIsOn != 0) && (gPhaseTimerTicks >= RELAY_ON_TIME_TICKS))
        {
            if (!relay_off())
            {
                log_nok("Relay OFF command failed");
            }

            close_relay_port();
            gRelayIsOn = 0;
            set_wup_stat(0);
        }
    }

    if (action_is_complete() != 0)
    {
        if (mode_uses_nm3(gMode) != 0)
        {
            start_resp_wait_phase();
        }
        else
        {
            start_wait_phase();
        }
    }
}

static void process_resp_wait_phase_1tick(void)
{
    if (gEcuRespSeen != 0)
    {
        start_wait_phase();
        return;
    }

    if (gPhaseTimerTicks >= ECU_RESP_TIMEOUT_TICKS)
    {
        if (gMode == MODE_NM3)
        {
            log_nok("NM3 wakeup failed: no 0x6EF within 100 ms");
        }
        else if (gMode == MODE_WUP_NM3)
        {
            log_nok("WUP+NM3 wakeup failed: no 0x6EF within 100 ms");
        }
        else
        {
            log_nok("Wakeup failed: no 0x6EF within 100 ms");
        }

        start_wait_phase();
    }
}

static void process_wait_phase_1tick(void)
{
    const u32 totalWaitTicks = POST_BURST_WAIT_TICKS + gTDelayTicks;

    if (gPhaseTimerTicks >= totalWaitTicks)
    {
        if (gTDelayTicks >= T_DELAY_END_TICKS)
        {
            stop_test(1);
            return;
        }

        gTDelayTicks += T_DELAY_STEP_TICKS;
        start_action_phase();
    }
}

static void process_1tick(void)
{
    if (gMode == MODE_NONE)
    {
        return;
    }

    if (gPhase == PHASE_ACTION)
    {
        process_action_phase_1tick();
    }
    else if (gPhase == PHASE_RESP_WAIT)
    {
        process_resp_wait_phase_1tick();
    }
    else if (gPhase == PHASE_WAIT)
    {
        process_wait_phase_1tick();
    }

    gPhaseTimerTicks++;
}

static void init_once(void)
{
    if (gInitialized != 0)
    {
        return;
    }

    gInitialized = 1;

    (void)app.create_system_var(SV_START_NM3,     svtInt32, "0", "Start NM3 cyclic wake-up test");
    (void)app.create_system_var(SV_START_WUP,     svtInt32, "0", "Start WUP cyclic wake-up test");
    (void)app.create_system_var(SV_START_WUP_NM3, svtInt32, "0", "Start WUP+NM3 cyclic wake-up test");

    (void)app.create_system_var(SV_STATE_NM3,     svtInt32, "0", "0=idle 1=ongoing 2=finished");
    (void)app.create_system_var(SV_STATE_WUP,     svtInt32, "0", "0=idle 1=ongoing 2=finished");
    (void)app.create_system_var(SV_STATE_WUP_NM3, svtInt32, "0", "0=idle 1=ongoing 2=finished");

    (void)app.create_system_var(SV_WUP_STAT,      svtInt32, "0", "1=relay on 0=relay off");

 (void)app.create_system_var(SV_NM3_TX_STAT,      svtInt32, "0", "NM3_TX_STAT");


    set_state_vars(0, 0, 0);
    set_wup_stat(0);

    gPrevStartNm3 = 0;
    gPrevStartWup = 0;
    gPrevStartWupNm3 = 0;

    gMode = MODE_NONE;
    gPhase = PHASE_IDLE;
    gTDelayTicks = T_DELAY_START_TICKS;
    gPhaseTimerTicks = 0u;

    close_relay_port();
    reset_runtime_state();
}
// CODE BLOCK END Global_Definitions 

// CODE BLOCK BEGIN On_CAN_Rx NewOn_CAN_Rx1 MCwtMSwxNzc1
// On CAN message reception handler "NewOn_CAN_Rx1" for identifier = 0x6EF
void on_can_rx_NewOn_CAN_Rx1(const TCAN* ACAN) { try {  // for identifier = 0x6EF
    if (ACAN == NULL)
    {
        return;
    }

    /* optional channel filter */
    /* if (ACAN->FIdxChn != CH1) return; */

    if ((gWaitForEcuResp != 0) &&
        (gMode != MODE_NONE) &&
        (mode_uses_nm3(gMode) != 0) &&
        (ACAN->FIdentifier == ECU_RESP_ID))
    {
        gEcuRespSeen = 1;
    }

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END On_CAN_Rx NewOn_CAN_Rx1

// CODE BLOCK BEGIN Step_Function  MC41
// Main step function being executed every 0.5 ms
void step(void) { try { // interval = 0.5 ms
    s32 startNm3 = 0;
    s32 startWup = 0;
    s32 startWupNm3 = 0;

    s32 edgeNm3 = 0;
    s32 edgeWup = 0;
    s32 edgeWupNm3 = 0;

    init_once();

    (void)app.get_system_var_int32(SV_START_NM3, &startNm3);
    (void)app.get_system_var_int32(SV_START_WUP, &startWup);
    (void)app.get_system_var_int32(SV_START_WUP_NM3, &startWupNm3);

    edgeNm3    = ((gPrevStartNm3 == 0) && (startNm3 == 1)) ? 1 : 0;
    edgeWup    = ((gPrevStartWup == 0) && (startWup == 1)) ? 1 : 0;
    edgeWupNm3 = ((gPrevStartWupNm3 == 0) && (startWupNm3 == 1)) ? 1 : 0;

    if (gMode == MODE_NONE)
    {
        if (edgeWupNm3 != 0)
        {
            start_test(MODE_WUP_NM3);
        }
        else if (edgeWup != 0)
        {
            start_test(MODE_WUP);
        }
        else if (edgeNm3 != 0)
        {
            start_test(MODE_NM3);
        }
    }
    else
    {
        if (active_trigger_is_one(startNm3, startWup, startWupNm3) == 0)
        {
            stop_test(0);
        }
    }

    process_1tick();

    gPrevStartNm3 = startNm3;
    gPrevStartWup = startWup;
    gPrevStartWupNm3 = startWupNm3;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END Step_Function 

// CODE BLOCK BEGIN Configuration
/* 
[UI]
UICommon=-1,-1,-1,0,QyBDb2RlIEVkaXRvciBbQ0NvZGUyMDld,100,207,3844949029240589717,0
ScriptName=CCode209
DisplayName=CCode209
DBDeps=ZGW_CAN_3
LastBuildTime=2026-08-26 21:47:30*/
// CODE BLOCK END Configuration

