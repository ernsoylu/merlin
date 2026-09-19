#ifndef OS_WRAPPER_H
#define OS_WRAPPER_H

#include <stdint.h>

typedef struct {
    int64_t expectedTick;
    uint32_t skippedActivations;
} Os_ReleaseStateType;

void Os_ReleaseInit(Os_ReleaseStateType *state, int64_t firstBoundary);
int Os_ReleaseSkip(Os_ReleaseStateType *state, int64_t actualTick, int64_t periodTicks);

#endif
