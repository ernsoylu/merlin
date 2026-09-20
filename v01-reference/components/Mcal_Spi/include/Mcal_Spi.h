#ifndef MCAL_SPI_H
#define MCAL_SPI_H

#include <stdint.h>

#include "Std_Types.h"

#define MCAL_SPI_HSPI (1U << 0)

Mcal_ResultType Mcal_Spi_GetCapabilities(uint8_t *capabilities);
Mcal_ResultType Mcal_Spi_Select(uint8_t requested, uint8_t *selected);

#endif
