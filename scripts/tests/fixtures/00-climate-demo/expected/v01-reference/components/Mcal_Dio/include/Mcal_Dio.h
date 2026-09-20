#ifndef MCAL_DIO_H
#define MCAL_DIO_H

#include <stdint.h>

#include "Std_Types.h"

Mcal_ResultType Mcal_Dio_Read(int32_t pin, uint8_t *level);
Mcal_ResultType Mcal_Dio_Write(int32_t pin, uint8_t level);

#endif
