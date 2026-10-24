/* CanTp_Cfg.c - ELC diagnostic channel: CAN 0x710 both directions, extended
 * addressing. N_TA 0x58 is this ECU (received requests), 0xF0 is the ZGW receiver
 * (transmitted responses and flow control), as configured in ZGW Dcm_Cfg.c / CanTp.c.
 * ZGW CanTp_SetTxAddress() uses its configured txNta (tester address 0x41, DCM_EXT_ADDR_TESTER)
 * for frames it sends outside a forwarded-request transmission, i.e. for the FC that answers a
 * multi-frame ECU response; both 0x58 and 0x41 are therefore accepted for FC. Source-derived,
 * not yet confirmed on a bus trace. */
#include "CanTp.h"

static const CanTp_ChannelConfigType CanTp_Channels[1] =
{
    {
        CANTP_RXNPDU_DIAG, CANTP_RXNSDU_DIAG, CANTP_TXNSDU_DIAG, CANTP_TXNPDU_DIAG,
        0x58u,                     /* rxAddress */
        0x41u,                     /* fcAddress */
        0xF0u,                     /* txAddress */
        8u,                        /* BS */
        5u,                        /* STmin [ms] */
        0x00u,                     /* padding */
        3u,                        /* N_WFTmax */
        CANTP_MS_TO_TICKS(1000u),  /* N_As */
        CANTP_MS_TO_TICKS(1000u),  /* N_Ar */
        CANTP_MS_TO_TICKS(1000u),  /* N_Bs */
        CANTP_MS_TO_TICKS(1000u)   /* N_Cr */
    }
};

const CanTp_ConfigType CanTp_Config = { CanTp_Channels, 1u };
