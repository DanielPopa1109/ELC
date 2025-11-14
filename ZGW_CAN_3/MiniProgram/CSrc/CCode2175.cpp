// GEN BLOCK BEGIN Include
#define TSMP_IMPL
#include "TSMaster.h"
#include "MPLibrary.h"
#include "Database.h"
#include "TSMasterBaseInclude.h"
#include "Configuration.h"
// GEN BLOCK END Include

#include <time.h>

// CODE BLOCK BEGIN Step_Function  NQ__
// Main step function being executed every 5 ms
void step(void) { try { // interval = 5 ms
time_t rawtime = time(NULL);
struct tm* timeinfo = localtime(&rawtime);
int full_year = timeinfo->tm_year + 1900;
int year_two_digits = full_year % 100;
static int cnt;
static int cnt2;

cnt++;
TCAN f0 = {0,0x1,7,0,0x202,705108399,{(u8)year_two_digits, (u8)(timeinfo->tm_mon + 1), (u8)timeinfo->tm_mday, (u8)timeinfo->tm_hour,  (u8)timeinfo->tm_min, (u8)timeinfo->tm_sec, (u8)cnt2, 0x0}};
if(cnt != 0 && (cnt % 180 == 0))
{
   com.transmit_can_async(&f0);
}
else cnt2++;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END Step_Function 

// CODE BLOCK BEGIN Configuration
/* 
[UI]
UICommon=-1,-1,-1,0,QyBDb2RlIEVkaXRvciBbQ0NvZGUyMTc1XQ__,100,222,7021393766468547402,0
ScriptName=CCode2175
DisplayName=CCode2175
DBDeps=ZGW_CAN_3
LastBuildTime=2025-11-09 20:47:41
*/
// CODE BLOCK END Configuration

