#ifndef MCAL_ICU_H
#define MCAL_ICU_H

#include <stdint.h>

#include "Std_Types.h"

typedef struct {
    int32_t edgePin;
    int32_t levelPin;
    int32_t lowLimit;
    int32_t highLimit;
    uint32_t glitchFilterNs;
} Mcal_IcuConfigType;

typedef enum {
    MCAL_ICU_STOPPED = 0,
    MCAL_ICU_RUNNING
} Mcal_IcuStateType;

typedef struct {
    void *unit;
    void *channel;
    Mcal_IcuConfigType config;
    Mcal_IcuStateType state;
    uint32_t overflowCount;
} Mcal_IcuHandleType;

Mcal_ResultType Mcal_Icu_ValidateConfig(const Mcal_IcuConfigType *config);
Mcal_ResultType Mcal_Icu_Init(Mcal_IcuHandleType *handle,
                              const Mcal_IcuConfigType *config);
Mcal_ResultType Mcal_Icu_Start(Mcal_IcuHandleType *handle);
Mcal_ResultType Mcal_Icu_Stop(Mcal_IcuHandleType *handle);
Mcal_ResultType Mcal_Icu_Clear(Mcal_IcuHandleType *handle);
Mcal_ResultType Mcal_Icu_Read(const Mcal_IcuHandleType *handle,
                              int64_t *count);
uint32_t Mcal_Icu_GetOverflowCount(const Mcal_IcuHandleType *handle);

#endif
