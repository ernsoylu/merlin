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
    return 0;
}
