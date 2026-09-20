#ifndef MCAL_PWM_H
#define MCAL_PWM_H

#include <stdint.h>

#include "Std_Types.h"

typedef struct {
    int32_t timer;
    int32_t channel;
    int32_t pin;
    uint32_t frequencyHz;
    uint8_t resolutionBits;
    uint16_t initialDutyPermille;
} Mcal_PwmConfigType;

typedef struct {
    int32_t channel;
    uint32_t maxDuty;
    uint8_t initialized;
} Mcal_PwmHandleType;

Mcal_ResultType Mcal_Pwm_Init(Mcal_PwmHandleType *handle,
                              const Mcal_PwmConfigType *config);
Mcal_ResultType Mcal_Pwm_SetDuty(const Mcal_PwmHandleType *handle,
                                 uint16_t dutyPermille);

#endif
