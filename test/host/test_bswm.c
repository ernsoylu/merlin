#include <assert.h>

#include "BswM.h"

int main(void)
{
    BswM_ContextType context;
    BswM_ModeType mode;

    BswM_Init(&context);
    assert(BswM_GetMode(&context, &mode) == BSWM_RESULT_OK);
    assert(mode == BSWM_MODE_STARTUP);
    assert(BswM_RequestMode(&context, BSWM_MODE_RUN) == BSWM_RESULT_OK);
    assert(BswM_RequestMode(&context, BSWM_MODE_DEGRADED) == BSWM_RESULT_OK);
    assert(BswM_RequestMode(&context, BSWM_MODE_RUN) == BSWM_RESULT_OK);
    assert(BswM_RequestMode(&context, BSWM_MODE_SAFE_HALT) == BSWM_RESULT_OK);
    assert(BswM_RequestMode(&context, BSWM_MODE_RUN) == BSWM_RESULT_REJECTED);
    assert(BswM_RequestMode(&context, BSWM_MODE_SHUTDOWN) == BSWM_RESULT_OK);
    assert(BswM_RequestMode(&context, BSWM_MODE_SAFE_HALT) == BSWM_RESULT_REJECTED);
    assert(BswM_RequestMode(0, BSWM_MODE_RUN) == BSWM_RESULT_INVALID);
    assert(BswM_GetMode(0, &mode) == BSWM_RESULT_INVALID);
    return 0;
}
