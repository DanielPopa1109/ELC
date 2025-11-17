#include "stdint.h"

typedef struct
{
	uint32_t reset;
	uint32_t info;
} FaultInfo_t;

extern void EcuM_main();
