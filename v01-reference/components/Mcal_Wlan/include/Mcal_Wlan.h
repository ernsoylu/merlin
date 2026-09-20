#ifndef MCAL_WLAN_H
#define MCAL_WLAN_H

#include <stdint.h>

#include "Std_Types.h"

typedef struct {
    uint8_t initialized;
    uint8_t started;
} Mcal_WlanHandleType;

Mcal_ResultType Mcal_Wlan_Init(Mcal_WlanHandleType *handle);
Mcal_ResultType Mcal_Wlan_Start(Mcal_WlanHandleType *handle);
Mcal_ResultType Mcal_Wlan_Stop(Mcal_WlanHandleType *handle);

#endif
