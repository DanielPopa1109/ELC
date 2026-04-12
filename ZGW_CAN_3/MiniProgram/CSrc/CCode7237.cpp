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

#define CAN_TX_CHN            CH1
#define CAN_STD_ID            0x3FF
#define CAN_DLC               1u
#define CAN_PERIOD_MS         5u
#define CAN_TOTAL_MS          100u
#define CAN_BURST_COUNT       (CAN_TOTAL_MS / CAN_PERIOD_MS)
#define RELAY_ON_TIME_MS      500u

static const char* SV_TRIGGER  = "WAKEUP";
static const char* SV_BUSY     = "Test.Trigger3FFRelayBusy";
static const char* SV_ERROR    = "Test.Trigger3FFRelayError";
static const char* SV_WUP_STAT = "WUP_STAT";
static const char* RELAY_COM   = "\\\\.\\COM6";

/* Common LCUS-1 relay bytes. */
static const u8 RELAY_CMD_ON[4]  = { 0xA0u, 0x01u, 0x01u, 0xA2u };
static const u8 RELAY_CMD_OFF[4] = { 0xA0u, 0x01u, 0x00u, 0xA1u };

enum
{
    ERR_NONE       = 0,
    ERR_RELAY_OPEN = 1,
    ERR_RELAY_IO   = 2
};

static HANDLE gRelayHandle = INVALID_HANDLE_VALUE;
static s32    gInitialized = 0;
static s32    gPrevTrigger = 0;
static s32    gActive      = 0;
static s32    gRelayIsOn   = 0;
static u32    gElapsedMs   = 0u;
static u32    gNextCanTxMs = 0u;
static u32    gCanTxCount  = 0u;

static void set_busy(const s32 value)
{
    (void)app.set_system_var_int32(SV_BUSY, value);
}

static void set_error(const s32 value)
{
    (void)app.set_system_var_int32(SV_ERROR, value);
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

    SetupComm(gRelayHandle, 64u, 64u);

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

    PurgeComm(gRelayHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);
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

    FlushFileBuffers(gRelayHandle);
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

static void send_3ff_60(void)
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

static void sequence_init_once(void)
{
    if (gInitialized != 0)
    {
        return;
    }

    gInitialized = 1;

    (void)app.create_system_var(SV_BUSY,     svtInt32, "0", "Sequence active");
    (void)app.create_system_var(SV_ERROR,    svtInt32, "0", "0=OK,1=COM open fail,2=relay I/O fail");
    (void)app.create_system_var(SV_WUP_STAT, svtInt32, "0", "1=relay on, 0=relay off");

    (void)app.set_system_var_int32(SV_BUSY, 0);
    (void)app.set_system_var_int32(SV_ERROR, ERR_NONE);
    (void)app.set_system_var_int32(SV_WUP_STAT, 0);

    gPrevTrigger = 0;
    gActive      = 0;
    gRelayIsOn   = 0;
    gElapsedMs   = 0u;
    gNextCanTxMs = 0u;
    gCanTxCount  = 0u;

    close_relay_port();
}

static void sequence_start(void)
{
    gActive      = 1;
    gRelayIsOn   = 0;
    gElapsedMs   = 0u;
    gNextCanTxMs = 0u;
    gCanTxCount  = 0u;

    set_busy(1);
    set_error(ERR_NONE);
    set_wup_stat(0);

    if (!open_relay_port())
    {
        set_error(ERR_RELAY_OPEN);
        log_nok("Relay COM open failed");
    }
    else
    {
        if (!relay_on())
        {
            set_error(ERR_RELAY_IO);
            set_wup_stat(0);
            log_nok("Relay ON command failed");
            close_relay_port();
        }
        else
        {
            gRelayIsOn = 1;
            set_wup_stat(1);
        }
    }
}

static void sequence_stop(void)
{
    if (gRelayIsOn != 0)
    {
        if (!relay_off())
        {
            set_error(ERR_RELAY_IO);
            log_nok("Relay OFF command failed");
        }
    }

    close_relay_port();

    gActive      = 0;
    gRelayIsOn   = 0;
    gElapsedMs   = 0u;
    gNextCanTxMs = 0u;
    gCanTxCount  = 0u;

    set_busy(0);
    set_wup_stat(0);
}

static void sequence_process_1ms(void)
{
    if (gActive == 0)
    {
        return;
    }

    if ((gCanTxCount < CAN_BURST_COUNT) && (gElapsedMs >= gNextCanTxMs))
    {
        send_3ff_60();
        gCanTxCount++;
        gNextCanTxMs += CAN_PERIOD_MS;
    }

    if ((gRelayIsOn != 0) && (gElapsedMs >= RELAY_ON_TIME_MS))
    {
        if (!relay_off())
        {
            set_error(ERR_RELAY_IO);
            log_nok("Relay OFF command failed");
        }

        close_relay_port();
        gRelayIsOn = 0;
        set_wup_stat(0);
    }

    gElapsedMs++;

    if (gElapsedMs > RELAY_ON_TIME_MS)
    {
        sequence_stop();
    }
}
// CODE BLOCK END Global_Definitions 

// CODE BLOCK BEGIN Step_Function  MQ__
// Main step function being executed every 1 ms
void step(void) { try { // interval = 1 ms
    s32 trigger = 0;

    sequence_init_once();

    (void)app.get_system_var_int32(SV_TRIGGER, &trigger);

    if ((gActive == 0) && (gPrevTrigger == 0) && (trigger == 1))
    {
        sequence_start();
    }

    gPrevTrigger = trigger;

    sequence_process_1ms();

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END Step_Function 

// CODE BLOCK BEGIN Configuration
/* 
[UI]
UICommon=-1,-1,-1,0,QyBDb2RlIEVkaXRvciBbQ0NvZGU3MjM3XQ__,100,163,5441474283909822556,0
ScriptName=CCode7237
DisplayName=CCode7237
DBDeps=ZGW_CAN_3
LastBuildTime=2026-04-10 13:39:25*/
// CODE BLOCK END Configuration

