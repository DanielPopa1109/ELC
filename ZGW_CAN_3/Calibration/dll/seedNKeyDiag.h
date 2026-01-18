//*****************************************************************************
/// @file        seedNKeyXcp.h
/// @author      Alex Lunkenheimer (ETAS/PAC-Lr)
/// @responsible author
/// @brief       DLL-Main for Seed'n Key for XCP
/// @history     2002-09-16,Lr: - draft
///              2003-02-07,Lr: - DLL-API using extern "C"
///                             - adapted privileges to XCP-Spec V1.0,Rev22
//*****************************************************************************
#ifndef SEED_N_KEY_DIAGNOSTIC_H
#define SEED_N_KEY_DIAGNOSTIC_H

//*****************************************************************************
//                               includes
//*****************************************************************************
#include <windows.h>

//*****************************************************************************
//                               typedefs
//*****************************************************************************
typedef DWORD TXcpSkExtFncRet;        // return code
#define KEYGEN_API EXTERN_C __declspec(dllexport) TXcpSkExtFncRet __cdecl

//*****************************************************************************
//                               constants
//*****************************************************************************
#define  KGRE_Ok                      0x00
#define KGRE_ERROR_TYPE_01   0x01

//*****************************************************************************
//                               prototypes
//*****************************************************************************
KEYGEN_API GenerateKeyEx(
	const unsigned char* ipSeedArray,          /* Array for the seed [in] */
	unsigned int iSeedArraySize,               /* Length of the array for the seed [in] */
	const unsigned int iSecurityLevel,         /* Security level [in] */
	const char* ipVariant,                     /* Name of the active variant [in] */
	unsigned char* iopKeyArray,                /* Array for the key [in, out] */
	unsigned int iMaxKeyArraySize,             /* Maximum length of the array for the key [in] */
	unsigned int& oActualKeyArraySize);        /* Length of the key [out] */

#endif
