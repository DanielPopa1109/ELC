// GEN BLOCK BEGIN Include
#define TSMP_IMPL
#include "TSMaster.h"
#include "MPLibrary.h"
#include "Database.h"
#include "TSMasterBaseInclude.h"
#include "Configuration.h"
// GEN BLOCK END Include

// GEN BLOCK BEGIN Custom_Function
s32 on_after_tsmaster_open(void);
s32 on_before_tsmaster_close(void);
s32 on_after_project_open(void);
s32 on_before_project_close(void);
s32 on_before_app_connect(void);
s32 on_after_app_connect(void);
s32 on_before_app_disconnect(void);
s32 on_after_app_disconnect(void);
s32 on_before_project_save(void);
s32 on_after_project_save(void);
// GEN BLOCK END Custom_Function

// CODE BLOCK BEGIN Custom_Function on_after_tsmaster_open dm9pZA__
// Custom Function "on_after_tsmaster_open"
s32 on_after_tsmaster_open(void) { try { // Custom Function: demo user event handler for TSMaster open
log("On TSMaster open event fired");
return 0;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function on_after_tsmaster_open

// CODE BLOCK BEGIN Custom_Function on_before_tsmaster_close dm9pZA__
// Custom Function "on_before_tsmaster_close"
s32 on_before_tsmaster_close(void) { try { // Custom Function: demo user event handler for TSMaster close
log("On TSMaster close event fired");
return 0;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function on_before_tsmaster_close

// CODE BLOCK BEGIN Custom_Function on_after_project_open dm9pZA__
// Custom Function "on_after_project_open"
s32 on_after_project_open(void) { try { // Custom Function: demo user event handler for TSMaster project open
log("On TSMaster project open event fired");
return 0;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function on_after_project_open

// CODE BLOCK BEGIN Custom_Function on_before_project_close dm9pZA__
// Custom Function "on_before_project_close"
s32 on_before_project_close(void) { try { // Custom Function: demo user event handler for TSMaster project close
log("On TSMaster project close event fired");
return 0;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function on_before_project_close

// CODE BLOCK BEGIN Custom_Function on_before_app_connect dm9pZA__
// Custom Function "on_before_app_connect"
s32 on_before_app_connect(void) { try { // Custom Function: demo user event handler for TSMaster application connect
log("On TSMaster before app connect event fired");
return 0;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function on_before_app_connect

// CODE BLOCK BEGIN Custom_Function on_after_app_connect dm9pZA__
// Custom Function "on_after_app_connect"
s32 on_after_app_connect(void) { try { // Custom Function: demo user event handler for TSMaster application connect
log("On TSMaster after app connect event fired");
return 0;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function on_after_app_connect

// CODE BLOCK BEGIN Custom_Function on_before_app_disconnect dm9pZA__
// Custom Function "on_before_app_disconnect"
s32 on_before_app_disconnect(void) { try { // Custom Function: demo user event handler for TSMaster application disconnect
log("On TSMaster before app disconnect event fired");
return 0;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function on_before_app_disconnect

// CODE BLOCK BEGIN Custom_Function on_after_app_disconnect dm9pZA__
// Custom Function "on_after_app_disconnect"
s32 on_after_app_disconnect(void) { try { // Custom Function: demo user event handler for TSMaster application disconnect
log("On TSMaster after app disconnect event fired");
return 0;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function on_after_app_disconnect

// CODE BLOCK BEGIN Custom_Function on_before_project_save dm9pZA__
// Custom Function "on_before_project_save"
s32 on_before_project_save(void) { try { // Custom Function: demo user event handler for TSMaster project save
log("On TSMaster before project save event fired");
return 0;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function on_before_project_save

// CODE BLOCK BEGIN Custom_Function on_after_project_save dm9pZA__
// Custom Function "on_after_project_save"
s32 on_after_project_save(void) { try { // Custom Function: demo user event handler for TSMaster project save
log("On TSMaster after project save event fired");
return 0;

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); return(IDX_ERR_MP_CODE_CRASH); }}
// CODE BLOCK END Custom_Function on_after_project_save

// CODE BLOCK BEGIN Step_Function  NQ__
// Main step function being executed every 5 ms
void step(void) { try { // interval = 5 ms

} catch (...) { log_nok("CRASH detected"); app.terminate_application(); }}
// CODE BLOCK END Step_Function 

// CODE BLOCK BEGIN Configuration
/* 
[UI]
UICommon=0,-1,-1,0,QyBDb2RlIEVkaXRvciBbdXNlciBldmVudHNd,100,213,1762883143543930451,0
ScriptName=user_events
DisplayName=user events
IsMPLib=1
DBDeps=ZGW_CAN_3
LastBuildTime=2025-11-18 22:54:11
VarListClose=1
APICnt=10

[UI_Doc]
Cnt=10
d0=on_after_tsmaster_open,ZGVtbyB1c2VyIGV2ZW50IGhhbmRsZXIgZm9yIFRTTWFzdGVyIG9wZW4_,,0
d1=on_before_tsmaster_close,ZGVtbyB1c2VyIGV2ZW50IGhhbmRsZXIgZm9yIFRTTWFzdGVyIGNsb3Nl,,0
d2=on_after_project_open,ZGVtbyB1c2VyIGV2ZW50IGhhbmRsZXIgZm9yIFRTTWFzdGVyIHByb2plY3Qgb3Blbg__,,0
d3=on_before_project_close,ZGVtbyB1c2VyIGV2ZW50IGhhbmRsZXIgZm9yIFRTTWFzdGVyIHByb2plY3QgY2xvc2U_,,0
d4=on_before_app_connect,ZGVtbyB1c2VyIGV2ZW50IGhhbmRsZXIgZm9yIFRTTWFzdGVyIGFwcGxpY2F0aW9uIGNvbm5lY3Q_,,0
d5=on_after_app_connect,ZGVtbyB1c2VyIGV2ZW50IGhhbmRsZXIgZm9yIFRTTWFzdGVyIGFwcGxpY2F0aW9uIGNvbm5lY3Q_,,0
d6=on_before_app_disconnect,ZGVtbyB1c2VyIGV2ZW50IGhhbmRsZXIgZm9yIFRTTWFzdGVyIGFwcGxpY2F0aW9uIGRpc2Nvbm5lY3Q_,,0
d7=on_after_app_disconnect,ZGVtbyB1c2VyIGV2ZW50IGhhbmRsZXIgZm9yIFRTTWFzdGVyIGFwcGxpY2F0aW9uIGRpc2Nvbm5lY3Q_,,0
d8=on_before_project_save,ZGVtbyB1c2VyIGV2ZW50IGhhbmRsZXIgZm9yIFRTTWFzdGVyIHByb2plY3Qgc2F2ZQ__,,0
d9=on_after_project_save,ZGVtbyB1c2VyIGV2ZW50IGhhbmRsZXIgZm9yIFRTTWFzdGVyIHByb2plY3Qgc2F2ZQ__,,0
*/
// CODE BLOCK END Configuration

