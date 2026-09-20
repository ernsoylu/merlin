#ifndef MCAL_ADC_H
#define MCAL_ADC_H

#include <stdint.h>

#include "Std_Types.h"

typedef enum {
    MCAL_ADC_TOUT = 0,
    MCAL_ADC_VDD = 1
} Mcal_AdcModeType;

typedef struct {
    Mcal_AdcModeType mode;
    uint8_t clockDiv;
} Mcal_AdcConfigType;

typedef struct {
    Mcal_AdcModeType mode;
    uint8_t initialized;
} Mcal_AdcHandleType;

Mcal_ResultType Mcal_Adc_Init(Mcal_AdcHandleType *handle,
                              const Mcal_AdcConfigType *config);
Mcal_ResultType Mcal_Adc_Read(const Mcal_AdcHandleType *handle,
                              uint16_t *value);

#endif
