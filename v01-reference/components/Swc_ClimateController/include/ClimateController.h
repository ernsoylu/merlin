#ifndef CLIMATE_CONTROLLER_H
#define CLIMATE_CONTROLLER_H

#include <stdint.h>

#include "Rte_Type.h"
#include "pid.h"

typedef struct {
    Pid_StateType pid;
    float setpointDegC;
    int64_t previousActivationUs;
    float requestedDuty;
    uint8_t outputValid;
} ClimateController_CtxType;

void ClimateController_Init(ClimateController_CtxType *context,
                            float setpointDegC);
float ClimateController_Run(ClimateController_CtxType *context,
                            const Rte_EnvironmentalDataType *environment,
                            int64_t activationUs);

#endif
