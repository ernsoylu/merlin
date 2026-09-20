#include "ClimateController.h"

void ClimateController_Init(ClimateController_CtxType *context,
                            float setpointDegC)
{
    *context = (ClimateController_CtxType){0};
    context->setpointDegC = setpointDegC;
    context->pid.kp = 0.1f;
    context->pid.ki = 0.02f;
    context->pid.outMin = 0.0f;
    context->pid.outMax = 1.0f;
    Pid_Init(&context->pid, 0.0f, 1.0f);
}

float ClimateController_Run(ClimateController_CtxType *context,
                            const Rte_EnvironmentalDataType *environment,
                            int64_t activationUs)
{
    const int valid = environment != 0 &&
                      environment->quality[0] == RTE_QUALITY_VALID;
    if (!valid) {
        context->outputValid = 0U;
        context->previousActivationUs = activationUs;
        return context->requestedDuty;
    }

    const float dtSeconds = context->previousActivationUs == 0 ||
                            activationUs <= context->previousActivationUs
        ? 0.0f
        : (float)(activationUs - context->previousActivationUs) / 1000000.0f;
    context->requestedDuty = Pid_Update(&context->pid, context->setpointDegC,
                                       environment->temperatureDegC, dtSeconds);
    context->previousActivationUs = activationUs;
    context->outputValid = 1U;
    return context->requestedDuty;
}
