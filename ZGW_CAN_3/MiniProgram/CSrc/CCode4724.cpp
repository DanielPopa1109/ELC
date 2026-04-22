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
#define CAN_BURST_PERIOD_MS        5u
#define CAN_BURST_TOTAL_MS         100u
#define CAN_BURST_COUNT            (CAN_BURST_TOTAL_MS / CAN_BURST_PERIOD_MS)

#define RELAY_ON_TIME_MS           500u
#define POST_BURST_WAIT_MS         1000u

#define T_DELAY_START_MS           0u
#define T_DELAY_END_MS             3740u
#define T_DELAY_STEP_MS            1u

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
    PHASE_WAIT
};

static HANDLE gRelayHandle = INVALID_HANDLE_VALUE;

static s32 gInitialized = 0;

static s32 gPrevStartNm3 = 0;
static s32 gPrevStartWup = 0;
static s32 gPrevStartWupNm3 = 0;

static s32 gMode = MODE_NONE;
static s32 gPhase = PHASE_IDLE;

static u32 gTDelayMs = 0u;
static u32 gPhaseTimerMs = 0u;

static s32 gRelayIsOn = 0;
static s32 gBurstActive = 0;

static u32 gBurstNextTxMs = 0u;
static u32 gBurstTxCount = 0u;

static ULONGLONG gLastTickMs = 0ull;

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
    gBurstNextTxMs = 0u;
    gBurstTxCount = 0u;
    gPhaseTimerMs = 0u;
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
    gTDelayMs = T_DELAY_START_MS;

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
    gPhaseTimerMs = 0u;

    gRelayIsOn = 0;
    gBurstActive = 0;
    gBurstNextTxMs = 0u;
    gBurstTxCount = 0u;
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
        gBurstNextTxMs = 0u;
    }
}

static void start_test(const s32 mode)
{
    gMode = mode;
    gPhase = PHASE_IDLE;
    gTDelayMs = T_DELAY_START_MS;
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

static void process_action_phase_1ms(void)
{
    if (mode_uses_nm3(gMode) != 0)
    {
        while ((gBurstActive != 0) &&
               (gBurstTxCount < CAN_BURST_COUNT) &&
               (gPhaseTimerMs >= gBurstNextTxMs))
        {
            send_nm3_frame();
            gBurstTxCount++;
            gBurstNextTxMs += CAN_BURST_PERIOD_MS;

            if (gBurstTxCount >= CAN_BURST_COUNT)
            {
                gBurstActive = 0;
            }
        }
    }

    if (mode_uses_wup(gMode) != 0)
    {
        if ((gRelayIsOn != 0) && (gPhaseTimerMs >= RELAY_ON_TIME_MS))
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
        gPhase = PHASE_WAIT;
        gPhaseTimerMs = 0u;
    }
}

static void process_wait_phase_1ms(void)
{
    const u32 totalWaitMs = POST_BURST_WAIT_MS + gTDelayMs;

    if (gPhaseTimerMs >= totalWaitMs)
    {
        if (gTDelayMs >= T_DELAY_END_MS)
        {
            stop_test(1);
            return;
        }

        gTDelayMs += T_DELAY_STEP_MS;
        start_action_phase();
    }
}

static void process_1ms_tick(void)
{
    if (gMode == MODE_NONE)
    {
        return;
    }

    if (gPhase == PHASE_ACTION)
    {
        process_action_phase_1ms();
    }
    else if (gPhase == PHASE_WAIT)
    {
        process_wait_phase_1ms();
    }

    gPhaseTimerMs++;
}

static void process_elapsed_time(void)
{
    ULONGLONG nowMs;
    ULONGLONG diffMs;
    u32 ticksToProcess = 0u;

    nowMs = GetTickCount64();

    if (gLastTickMs == 0ull)
    {
        gLastTickMs = nowMs;
        return;
    }

    if (nowMs <= gLastTickMs)
    {
        return;
    }

    diffMs = nowMs - gLastTickMs;

    if (diffMs > 1000ull)
    {
        diffMs = 1000ull;
    }

    ticksToProcess = (u32)diffMs;
    gLastTickMs = nowMs;

    while (ticksToProcess > 0u)
    {
        process_1ms_tick();
        ticksToProcess--;

        if (gMode == MODE_NONE)
        {
            break;
        }
    }
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

    set_state_vars(0, 0, 0);
    set_wup_stat(0);

    gPrevStartNm3 = 0;
    gPrevStartWup = 0;
    gPrevStartWupNm3 = 0;

    gMode = MODE_NONE;
    gPhase = PHASE_IDLE;
    gTDelayMs = T_DELAY_START_MS;
    gPhaseTimerMs = 0u;

    gLastTickMs = GetTickCount64();

    close_relay_port();
    reset_runtime_state();
}
// CODE BLOCK END Global_Definitions 

// CODE BLOCK BEGIN Step_Function  MQ__
// Main step function being executed every 1 ms
void step(void) { try { // interval = 1 ms
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

    process_elapsed_time();

    gPrevStartNm3 = startNm3;
    gPrevStartWup = startWup;
    gPrevStartWupNm3 = startWupNm3;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END Step_Function 

// CODE BLOCK BEGIN Configuration
/* 
[UI]
UICommon=0,-1,-1,0,QyBDb2RlIEVkaXRvciBbQ0NvZGU0NzI0XQ__,100,225,2447706486331042994,0
ScriptName=CCode4724
DisplayName=CCode4724
DBDeps=ZGW_CAN_3
LastBuildTime=2026-04-21 23:14:32*/
// CODE BLOCK END Configuration

