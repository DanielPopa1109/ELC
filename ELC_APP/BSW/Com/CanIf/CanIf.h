/* CanIf.h - CAN Interface for the ELC lab stack.
 *
 * Maps CAN identifiers to PDU ids through the static tables in CanIf_Cfg.c
 * and dispatches to the configured upper layer (CanTp, PduR, CanNm, application).
 * PDU mode (online/offline) is controlled per controller by CanSM; PDUs flagged
 * CANIF_PDU_ALWAYS_ONLINE (diagnostics) are not affected. */
#ifndef ELC_BSW_CANIF_H
#define ELC_BSW_CANIF_H

#include "../Can/Can.h"   /* relative: the CubeMX header Inc/can.h has the same name on case-insensitive systems */
#include "ComStack_Cfg.h"

#define CANIF_PDU_ALWAYS_ONLINE 0x01u   /* not affected by CanIf_SetPduMode (diagnostics) */
#define CANIF_PDU_VARIABLE_DLC  0x02u   /* TX: 1..dlc bytes allowed instead of exactly dlc */

typedef enum
{
    CANIF_OFFLINE = 0u,
    CANIF_TX_OFFLINE,
    CANIF_ONLINE
} CanIf_PduModeType;

typedef enum
{
    CANIF_SET_OFFLINE = 0u,
    CANIF_SET_RX_OFFLINE,
    CANIF_SET_RX_ONLINE,
    CANIF_SET_TX_OFFLINE,
    CANIF_SET_TX_ONLINE,
    CANIF_SET_ONLINE
} CanIf_PduSetModeType;

typedef enum
{
    CANIF_UPPER_CANTP = 0u,
    CANIF_UPPER_PDUR,
    CANIF_UPPER_CANNM,
    CANIF_UPPER_APPL
} CanIf_UpperLayerType;

typedef struct
{
    Can_IdType           canId;
    uint8                minDlc;      /* frames with a shorter DLC are dropped */
    uint8                flags;
    CanIf_UpperLayerType upper;
    PduIdType            upperPduId;
} CanIf_RxPduConfigType;

typedef struct
{
    Can_IdType           canId;
    uint8                dlc;         /* CanIf_Transmit() requires exactly this length */
    uint8                flags;
    CanIf_UpperLayerType upper;       /* receiver of the TX confirmation */
    PduIdType            upperPduId;
} CanIf_TxPduConfigType;

typedef struct
{
    const CanIf_RxPduConfigType *rxPdus;
    uint8                        numRxPdus;
    const CanIf_TxPduConfigType *txPdus;
    uint8                        numTxPdus;
} CanIf_ConfigType;

extern const CanIf_ConfigType CanIf_Config;

void CanIf_Init(const CanIf_ConfigType *ConfigPtr);
Std_ReturnType CanIf_Transmit(PduIdType CanIfTxSduId, const PduInfoType *PduInfoPtr);
Std_ReturnType CanIf_SetPduMode(uint8 ControllerId, CanIf_PduSetModeType PduModeRequest);
Std_ReturnType CanIf_GetPduMode(uint8 ControllerId, CanIf_PduModeType *PduModePtr);
Std_ReturnType CanIf_GetControllerMode(uint8 ControllerId, Can_ControllerStateType *ControllerModePtr);

/* Callbacks from the CAN driver. */
void CanIf_RxIndication(const Can_HwType *Mailbox, const PduInfoType *PduInfoPtr);
void CanIf_TxConfirmation(PduIdType CanTxPduId);
void CanIf_ControllerBusOff(uint8 ControllerId);
void CanIf_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode);

#endif /* ELC_BSW_CANIF_H */
