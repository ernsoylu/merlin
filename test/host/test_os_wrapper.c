#include <assert.h>
#include "Os_Wrapper.h"

static void event_runnable(void *context)
{
    unsigned int *calls = context;
    (*calls)++;
}

int main(void)
{
    Os_ReleaseStateType state;
    Os_ReleaseInit(&state, 10);
    assert(!Os_ReleaseSkip(&state, 10, 10));
    assert(Os_ReleaseSkip(&state, 25, 10));
    assert(state.expectedTick == 30 && state.skippedActivations == 1);
    assert(!Os_ReleaseSkip(&state, 30, 10));
    Os_ReleaseConfigure(&state, 10, 1);
    assert(!Os_ReleaseRecordWake(&state, 30));
    assert(Os_ReleaseRecordWake(&state, 42));
    assert(state.lateActivations == 1 && state.lastFault == OS_RTF_LATE_ACTIVATION);
    assert(Os_DeadlineCheck(&state, 41, 40));
    assert(state.deadlineMisses == 1 && state.lastFault == OS_RTF_DEADLINE);

    unsigned int calls = 0U;
    Os_EventTaskConfigType event = {
        .runnable = event_runnable,
        .context = &calls
    };
    assert(Os_EventTaskDispatch(&event));
    assert(calls == 1U);
    assert(!Os_EventTaskDispatch(0));
    event.runnable = 0;
    assert(!Os_EventTaskDispatch(&event));
    return 0;
}
