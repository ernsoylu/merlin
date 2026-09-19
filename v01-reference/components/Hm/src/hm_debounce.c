#include "Hm_Debounce.h"

void Hm_DebounceInit(Hm_DebounceType *state, unsigned int failedCyclesToSet,
                     unsigned int passedCyclesToHeal)
{
    *state = (Hm_DebounceType){0, 0, failedCyclesToSet, passedCyclesToHeal, 0};
}

void Hm_DebounceStep(Hm_DebounceType *state, int failed)
{
    if (failed) {
        state->passed = 0;
        if (state->failed < state->failedCyclesToSet) {
            state->failed++;
        }
        if (state->failed >= state->failedCyclesToSet) {
            state->active = 1;
        }
    } else {
        state->failed = 0;
        if (state->passed < state->passedCyclesToHeal) {
            state->passed++;
        }
        if (state->passed >= state->passedCyclesToHeal) {
            state->active = 0;
        }
    }
}
