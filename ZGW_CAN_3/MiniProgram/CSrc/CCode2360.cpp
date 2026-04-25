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

TCAN f0 = {0, 0x1, 5, 0, 0x202, 705108399,
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


// static uint32_t tick_ms = 0u;
// static uint32_t last_burst_start_ms = 0u;
// static uint32_t last_tupdate_ms = 0u;
// 
// static uint32_t t_increment_ms = 1u;   /* t_init = 0 ms -> first increment = 1 ms */
// 
// static uint8_t burst_active = 0u;
// static uint8_t burst_msg_idx = 0u;     /* 0..4 */
// static uint32_t burst_next_tx_ms = 0u;
// static double startTestSig = 0u;
// 
// com.can_rbs_get_signal_value_by_address("0/ZGW_CAN_3/ZGW/VehicleData/VehicleStatus", &startTestSig);
// 
// if(2u ==    startTestSig)
// {
//      tick_ms++;
// 
// TCAN nm3_msg = {0, 0x1, 1, 0, 0x3FF, 705108399,
//                 {0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
// 
// /* update t_increment every 1025 ms */
// if ((tick_ms - last_tupdate_ms) >= 1025u)
// {
//     last_tupdate_ms = tick_ms;
//     t_increment_ms++;
// }
// 
// /* start a new burst when period expires and no burst is active */
// if ((burst_active == 0u) && ((tick_ms - last_burst_start_ms) >= t_increment_ms))
// {
//     burst_active = 1u;
//     burst_msg_idx = 0u;
//     last_burst_start_ms = tick_ms;
//     burst_next_tx_ms = tick_ms;   /* first message immediately */
// }
// 
// /* transmit burst of 5 messages, 5 ms apart */
// if (burst_active != 0u)
// {
//     if (tick_ms >= burst_next_tx_ms)
//     {
//         com.transmit_can_async(&nm3_msg);
// 
//         burst_msg_idx++;
//         burst_next_tx_ms += 5u;
// 
//         if (burst_msg_idx >= 5u)
//         {
//             burst_active = 0u;
//         }
//     }
// }
// 
// }
// else
// {
//   tick_ms = 0u;
//   last_burst_start_ms = 0u;
//   last_tupdate_ms = 0u;
// 
//   t_increment_ms = 1u;   /* t_init = 0 ms -> first increment = 1 ms */
// 
//   burst_active = 0u;
//   burst_msg_idx = 0u;     /* 0..4 */
//   burst_next_tx_ms = 0u;
// }

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END Step_Function 

// CODE BLOCK BEGIN Configuration
/* 
[UI]
UICommon=-1,-1,-1,0,QyBDb2RlIEVkaXRvciBbQ0NvZGUyMzYwXQ__,100,175,4505007066452424418,0
ScriptName=CCode2360
DisplayName=CCode2360
DBDeps=ZGW_CAN_3
LastBuildTime=2026-04-04 21:14:39*/
// CODE BLOCK END Configuration

