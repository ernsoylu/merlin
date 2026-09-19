#include "Os_Wrapper.h"

void Os_ReleaseInit(Os_ReleaseStateType *state, int64_t firstBoundary)
{
    state->expectedTick = firstBoundary;
    state->skippedActivations = 0;
}

int Os_ReleaseSkip(Os_ReleaseStateType *state, int64_t actualTick, int64_t periodTicks)
{
    if (actualTick <= state->expectedTick) {
        return 0;
    }

    state->expectedTick = ((actualTick / periodTicks) + 1) * periodTicks;
    state->skippedActivations++;
    return 1;
}
