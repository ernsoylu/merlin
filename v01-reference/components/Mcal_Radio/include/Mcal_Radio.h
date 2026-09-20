#ifndef MCAL_RADIO_H
#define MCAL_RADIO_H

#include <stdint.h>

#include "Std_Types.h"

#define MCAL_RADIO_WLAN (1U << 0)
#define MCAL_RADIO_BT   (1U << 1)

Mcal_ResultType Mcal_Radio_GetCapabilities(uint8_t *capabilities);
Mcal_ResultType Mcal_Radio_Select(uint8_t requested, uint8_t *selected);

#endif
