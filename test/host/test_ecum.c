#include <assert.h>

#include "EcuM.h"

int main(void)
{
    EcuM_ContextType context;
    EcuM_ContextInit(&context, 4U);
    EcuM_RecordInitFailure(&context);
    assert(EcuM_Release(&context, 100) && context.state == ECUM_DEGRADED);
    assert(!EcuM_Release(&context, 200));
    for (unsigned int i = 0; i < 5U; ++i) {
        EcuM_RecordDeadlineFault(&context);
    }
    assert(context.state == ECUM_SHUTDOWN && context.resetRequested);
    assert(EcuM_CheckBootLoop(&context, 5U, 300000U));
    assert(context.state == ECUM_SAFE_HALT && !context.resetRequested);
    assert(!context.watchdogSubscribed);
    context.state = ECUM_RUN;
    uint32_t retainedBoots = 3U;
    context.bootLoopCounter = &retainedBoots;
    for (unsigned int i = 0; i < 240U; ++i) {
        EcuM_RecordRunActivation(&context);
    }
    assert(retainedBoots == 0U);
    return 0;
}
