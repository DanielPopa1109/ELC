/* ComM_Types.h - communication modes shared by ComM and CanSM. */
#ifndef ELC_BSW_COMM_TYPES_H
#define ELC_BSW_COMM_TYPES_H

#include "ComStack_Types.h"

/* ELC has no bus sleep: bxCAN stays started with RX online so diagnostics remain reachable.
 * COMM_SILENT_COMMUNICATION is the "receive only" state (application TX off);
 * COMM_NO_COMMUNICATION requests are served as silent. */
typedef enum
{
    COMM_NO_COMMUNICATION = 0u,
    COMM_SILENT_COMMUNICATION,
    COMM_FULL_COMMUNICATION
} ComM_ModeType;

#endif /* ELC_BSW_COMM_TYPES_H */
