// GEN BLOCK BEGIN Include
#define TSMP_IMPL
#include "TSMaster.h"
#include "MPLibrary.h"
#include "Database.h"
#include "TSMasterBaseInclude.h"
#include "Configuration.h"
// GEN BLOCK END Include

// CODE BLOCK BEGIN Global_Definitions 
#define CAN_CH         1
#define CAN_ID         0x50        // <-- replace with your CAN ID
#define DLC            8
#define DATA_ID        0x16         // 22 decimal
#define CRC_POLY       0x07         // <-- change if required
#define CRC_INIT       0xFF

static u8 aliveCounter = 0;
static u8 startCondition = 0;

/* CRC8 calculation */
u8 CalcCRC8(u8 *data, u8 len)
{
    u8 crc = 0x00;

    for (u8 i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (u8 b = 0; b < 8; b++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* Send Load Control Message */
void SendLoadControl(u8 loadRequest)
{
    u8 msg[5];
    u8 crc;

    msg[1] = aliveCounter;
    msg[2] = DATA_ID;
    msg[3] = loadRequest;
    msg[4] = 0x00;
    
    crc = CalcCRC8(&msg[1], 4);
    msg[0] = crc;

    aliveCounter++;
    if (aliveCounter > 14)
        aliveCounter = 0;

TCAN f0 = {0,0x1,5,0,0x50,705108399,{msg[0], msg[1], msg[2], msg[3], msg[4]}};
com.transmit_can_async(&f0); 
}
// CODE BLOCK END Global_Definitions 

// CODE BLOCK BEGIN On_Shortcut QC_Test_Activate MTY0NDk_
// On shortcut "QC_Test_Activate" with shortcut = Ctrl+A
void on_shortcut_QC_Test_Activate(const s32 AShortcut) { try { // On shortcut = Ctrl+A
                      startCondition = 1;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END On_Shortcut QC_Test_Activate

// CODE BLOCK BEGIN On_Shortcut QC_Test_Deactivate MTY0NTA_
// On shortcut "QC_Test_Deactivate" with shortcut = Ctrl+B
void on_shortcut_QC_Test_Deactivate(const s32 AShortcut) { try { // On shortcut = Ctrl+B
                       startCondition = 0;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END On_Shortcut QC_Test_Deactivate

// CODE BLOCK BEGIN Step_Function  MQ__
// Main step function being executed every 1 ms
void step(void) { try { // interval = 1 ms
static int cnt = 0;
static int cnt2 = 0;
static u8 loadstatus = 0;

if(0 != startCondition)
{
     if(cnt == 0)
     {
            loadstatus = 0u;
            SendLoadControl(loadstatus);
     }
     
     if(cnt == 100)
     {
            loadstatus = 1u;
            SendLoadControl(loadstatus);
     }
     
     if(cnt == 1020000)
     {
            loadstatus = 0;
            cnt = 0;
            SendLoadControl(loadstatus);
     } 

     if(cnt2 % 100 == 0)
     {
             SendLoadControl(loadstatus);
     } 

     cnt++;
     cnt2++;
     
}
else
{
    cnt = 0;
    cnt2 = 0;
    loadstatus = 0;
}

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END Step_Function 

// CODE BLOCK BEGIN Configuration
/* 
[UI]
UICommon=0,-1,-1,0,QyBDb2RlIEVkaXRvciBbQ0NvZGU1MzU0XQ__,100,154,2545659729352293659,0
ScriptName=CCode5354
DisplayName=CCode5354
DBDeps=ZGW_CAN_3
LastBuildTime=2026-01-31 12:14:18
*/
// CODE BLOCK END Configuration

