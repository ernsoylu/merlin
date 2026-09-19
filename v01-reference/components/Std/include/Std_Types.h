#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>

typedef uint8_t boolean;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int32_t sint32;
typedef int64_t sint64;
typedef float float32;
typedef uint8 Std_ReturnType;

#define STD_FALSE ((boolean)0U)
#define STD_TRUE ((boolean)1U)
#define E_OK ((Std_ReturnType)0U)
#define E_NOT_OK ((Std_ReturnType)1U)

typedef enum {
    MCAL_OK = 0,
    MCAL_TIMEOUT,
    MCAL_NACK,
    MCAL_ARB_LOST,
    MCAL_BUSY,
    MCAL_INVALID_ARG,
    MCAL_HW_FAIL
} Mcal_ResultType;

#endif
