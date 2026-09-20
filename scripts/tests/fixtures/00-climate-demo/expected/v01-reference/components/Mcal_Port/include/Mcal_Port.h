#ifndef MCAL_PORT_H
#define MCAL_PORT_H

#include <stdint.h>

#include "Std_Types.h"

typedef struct {
    int32_t pin;
    uint8_t output;
    uint8_t initialLevel;
} Mcal_PortPinConfigType;

Mcal_ResultType Mcal_Port_Init(const Mcal_PortPinConfigType *pins,
                               uint16_t count);
Mcal_ResultType Mcal_Port_Write(int32_t pin, uint8_t level);

#endif
