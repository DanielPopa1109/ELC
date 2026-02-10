// GEN BLOCK BEGIN Include
#define TSMP_IMPL
#include "TSMaster.h"
#include "MPLibrary.h"
#include "Database.h"
#include "TSMasterBaseInclude.h"
#include "Configuration.h"
// GEN BLOCK END Include

// CODE BLOCK BEGIN Global_Definitions 
               #include <time.h>
// CODE BLOCK END Global_Definitions 

// CODE BLOCK BEGIN Step_Function  MQ__
// Main step function being executed every 1 ms
void step(void) { try { // interval = 1 ms
                                    time_t rawtime = time(NULL);
struct tm* timeinfo = localtime(&rawtime);
int full_year = timeinfo->tm_year + 1900;
int year_two_digits = full_year % 100;
static int cnt;
static int cnt2;

cnt++;

u32 t =
    (year_two_digits & 0x3F) |
    ((timeinfo->tm_mon + 1 & 0x0F) << 6) |
    ((timeinfo->tm_mday & 0x1F) << 10) |
    ((timeinfo->tm_hour & 0x1F) << 15) |
    ((timeinfo->tm_min & 0x3F) << 20) |
    ((timeinfo->tm_sec & 0x3F) << 26);

TCAN f0 = {0, 0x1, 7, 0, 0x202, 705108399,
           {
               (u8)(t & 0xFF),
               (u8)((t >> 8) & 0xFF),
               (u8)((t >> 16) & 0xFF),
               (u8)((t >> 24) & 0xFF),
               (u8)cnt2,
               0x0,
               0x0,
               0x0
           }};

if(cnt != 0 && (cnt % 800 == 0))
{
   com.transmit_can_async(&f0);
}
else cnt2++;  

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END Step_Function 

// CODE BLOCK BEGIN Configuration
/* 
[UI]
UICommon=-1,-1,-1,0,QyBDb2RlIEVkaXRvciBbQ0NvZGUyMzYwXQ__,100,182,4505007066452424418,0
ScriptName=CCode2360
DisplayName=CCode2360
DBDeps=ZGW_CAN_3
LastBuildTime=2026-02-06 12:31:44
*/
// CODE BLOCK END Configuration

