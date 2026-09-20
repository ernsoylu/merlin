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

void Hm_RuntimeInit(Hm_RuntimeType *state, unsigned int failedCyclesToSet,
                    unsigned int passedCyclesToHeal)
{
    *state = (Hm_RuntimeType){0};
    Hm_DebounceInit(&state->sequence, failedCyclesToSet, passedCyclesToHeal);
}

int Hm_RuntimeObserveSequence(Hm_RuntimeType *state, uint32_t sequence)
{
    if (!state->hasSequence) {
        state->hasSequence = 1U;
        state->lastSequence = sequence;
        Hm_DebounceStep(&state->sequence, 0);
        return 0;
    }

    /* Unsigned subtraction gives the intended modulo-2^32 ordering: a delta
     * in the upper half is stale/out-of-order, including the ambiguous half
     * range, rather than progress. */
    const uint32_t delta = sequence - state->lastSequence;
    const int progressed = delta != 0U && delta < 0x80000000U;
    Hm_DebounceStep(&state->sequence, !progressed);
    if (progressed) {
        state->lastSequence = sequence;
    }
    return state->sequence.active;
}

void Hm_RuntimeRecordRtf(Hm_RuntimeType *state, uint32_t rtf)
{
    if (rtf < 8U) {
        state->rtfCounts[rtf]++;
    }
}

uint32_t Hm_RuntimeRtfCount(const Hm_RuntimeType *state, uint32_t rtf)
{
    return rtf < 8U ? state->rtfCounts[rtf] : 0U;
}
