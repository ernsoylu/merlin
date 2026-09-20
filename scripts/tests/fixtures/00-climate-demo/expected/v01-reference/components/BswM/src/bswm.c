#include "BswM.h"

static int transition_allowed(BswM_ModeType current, BswM_ModeType requested)
{
    if (requested == BSWM_MODE_SHUTDOWN) {
        return current != BSWM_MODE_SHUTDOWN;
    }
    if (current == BSWM_MODE_SAFE_HALT || current == BSWM_MODE_SHUTDOWN) {
        return 0;
    }
    return requested == BSWM_MODE_RUN ||
           requested == BSWM_MODE_DEGRADED ||
           requested == BSWM_MODE_SAFE_HALT;
}

void BswM_Init(BswM_ContextType *context)
{
    if (context != 0) {
        context->mode = BSWM_MODE_STARTUP;
    }
}

BswM_ResultType BswM_RequestMode(BswM_ContextType *context,
                                 BswM_ModeType requested)
{
    if (context == 0 || requested > BSWM_MODE_SHUTDOWN) {
        return BSWM_RESULT_INVALID;
    }
    if (!transition_allowed(context->mode, requested)) {
        return BSWM_RESULT_REJECTED;
    }
    context->mode = requested;
    return BSWM_RESULT_OK;
}

BswM_ResultType BswM_GetMode(const BswM_ContextType *context,
                             BswM_ModeType *mode)
{
    if (context == 0 || mode == 0) {
        return BSWM_RESULT_INVALID;
    }
    *mode = context->mode;
    return BSWM_RESULT_OK;
}
