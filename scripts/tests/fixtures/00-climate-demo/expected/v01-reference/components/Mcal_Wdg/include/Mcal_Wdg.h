#ifndef MCAL_WDG_H
#define MCAL_WDG_H

#include <stdint.h>

#include "Std_Types.h"

typedef struct {
    uint32_t timeoutMs;
} Mcal_WdgConfigType;

typedef struct {
    uint32_t timeoutMs;
    uint8_t initialized;
} Mcal_WdgHandleType;

Mcal_ResultType Mcal_Wdg_Init(Mcal_WdgHandleType *handle,
                              const Mcal_WdgConfigType *config);
Mcal_ResultType Mcal_Wdg_Feed(const Mcal_WdgHandleType *handle);
Mcal_ResultType Mcal_Wdg_Stop(Mcal_WdgHandleType *handle);

#endif
