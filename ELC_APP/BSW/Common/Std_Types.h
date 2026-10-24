/* Std_Types.h - AUTOSAR-style standard types for the ELC (STM32F103) lab stack.
 *
 * ELC follows the ZGW lab approach: AUTOSAR-shaped module APIs and static
 * configuration, called directly between modules. There is no RTE. */
#ifndef ELC_STD_TYPES_H
#define ELC_STD_TYPES_H

#include <stdint.h>

typedef uint8_t  boolean;
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t   sint8;
typedef int16_t  sint16;
typedef int32_t  sint32;
typedef float    float32;

typedef uint8 Std_ReturnType;

#ifndef E_OK
#define E_OK     0u
#endif
#ifndef E_NOT_OK
#define E_NOT_OK 1u
#endif

#ifndef STD_ON
#define STD_ON   1u
#define STD_OFF  0u
#endif

#ifndef TRUE
#define TRUE     1u
#endif
#ifndef FALSE
#define FALSE    0u
#endif

#ifndef NULL_PTR
#define NULL_PTR ((void *)0)
#endif

#endif /* ELC_STD_TYPES_H */
