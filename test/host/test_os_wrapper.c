#include <assert.h>
#include "Os_Wrapper.h"

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
    return 0;
}
