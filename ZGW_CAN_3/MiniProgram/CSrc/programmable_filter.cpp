// GEN BLOCK BEGIN Include
#define TSMP_IMPL
#include "TSMaster.h"
#include "MPLibrary.h"
#include "Database.h"
#include "TSMasterBaseInclude.h"
#include "Configuration.h"
// GEN BLOCK END Include

// GEN BLOCK BEGIN Custom_Function
s32 filter_can_frame(const PCANFD ACAN);
s32 filter_lin_frame(const PLIN ALIN);
s32 filter_flexray_frame(const PFlexRay AFlexRay);
s32 filter_ethernet_frame(const PEthernetHeader AEthernet);
// GEN BLOCK END Custom_Function

// CODE BLOCK BEGIN Custom_Function filter_can_frame Y29uc3QgUENBTkZEIEFDQU4_
// Custom Function "filter_can_frame"
s32 filter_can_frame(const PCANFD ACAN) { try { // Custom Function: 
/*
  Note:
  [1] this is callback entry for EACH CAN / CAN FD frame, 
      write simple code here, do not block this thread !
  [2] function return value represents filter result, 
      return 0 means this frame is blocked
      return non 0 means this frame is passed to measurement window
  [3] you can associate this function with any measurement window,
      or you can create different filter functions with same arguments for
      different measurement window 
*/

// example
// if (ACAN->FIdxChn != CH1) return 0;   // channel filter
// if (ACAN->is_tx) return 0;            // direction filter
// if (ACAN->FData[0] != 0x55) return 0; // data filter

       

return 1;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function filter_can_frame

// CODE BLOCK BEGIN Custom_Function filter_lin_frame Y29uc3QgUExJTiBBTElO
// Custom Function "filter_lin_frame"
s32 filter_lin_frame(const PLIN ALIN) { try { // Custom Function: 
/*
  Note:
  [1] this is callback entry for EACH LIN frame, 
      write simple code here, do not block this thread !
  [2] function return value represents filter result, 
      return 0 means this frame is blocked
      return non 0 means this frame is passed to measurement window
  [3] you can associate this function with any measurement window,
      or you can create different filter functions with same arguments for
      different measurement window 
*/

// example
// if (ALIN->FIdxChn != CH1) return 0;   // channel filter
// if (ALIN->is_tx) return 0;            // direction filter
// if (ALIN->FData[0] != 0x15) return 0; // data filter



return 1;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function filter_lin_frame

// CODE BLOCK BEGIN Custom_Function filter_flexray_frame Y29uc3QgUEZsZXhSYXkgQUZsZXhSYXk_
// Custom Function "filter_flexray_frame"
s32 filter_flexray_frame(const PFlexRay AFlexRay) { try { // Custom Function: 
/*
  Note:
  [1] this is callback entry for EACH FlexRay frame, 
      write simple code here, do not block this thread !
  [2] function return value represents filter result, 
      return 0 means this frame is blocked
      return non 0 means this frame is passed to measurement window
  [3] you can associate this function with any measurement window,
      or you can create different filter functions with same arguments for
      different measurement window 
*/

// example
// if (AFlexRay->FIdxChn != CH1) return 0;   // channel filter
// if (AFlexRay->is_tx) return 0;            // direction filter
// if (AFlexRay->FData[0] != 0x55) return 0; // data filter



return 1;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function filter_flexray_frame

// CODE BLOCK BEGIN Custom_Function filter_ethernet_frame Y29uc3QgUEV0aGVybmV0SGVhZGVyIEFFdGhlcm5ldA__
// Custom Function "filter_ethernet_frame"
s32 filter_ethernet_frame(const PEthernetHeader AEthernet) { try { // Custom Function: 
/*
  Note:
  [1] this is callback entry for EACH Ethernet frame, 
      write simple code here, do not block this thread !
  [2] function return value represents filter result, 
      return 0 means this frame is blocked
      return non 0 means this frame is passed to measurement window
  [3] you can associate this function with any measurement window,
      or you can create different filter functions with same arguments for
      different measurement window 
*/

// example
// if (AEthernet->FIdxChn != CH1) return 0;                            // channel filter
// if (AEthernet->get_tx()) return 0;                                  // direction filter
// if (*(AEthernet->ethernet_payload_addr() + 0x01) != 0x55) return 0; // data filter



return 1;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function filter_ethernet_frame

// CODE BLOCK BEGIN Step_Function  NQ__
// Main step function being executed every 5 ms
void step(void) { try { // interval = 5 ms

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END Step_Function 

// CODE BLOCK BEGIN Configuration
/* 
[UI]
UICommon=0,-1,-1,0,QyBDb2RlIEVkaXRvciBbUHJvZ3JhbW1hYmxlIEZpbHRlcl0_,100,179,8325750100665931534,0
ScriptName=programmable_filter
DisplayName=Programmable Filter
IsMPLib=1
DBDeps=ZGW_CAN_3
VarListClose=1
APICnt=4

[UI_Doc]
Cnt=1
d0=filter_can_frame,,,1,MjIsLTEsQUNBTiw_
*/
// CODE BLOCK END Configuration

