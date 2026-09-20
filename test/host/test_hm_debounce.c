#include <assert.h>
#include "Hm_Debounce.h"

int main(void)
{
    Hm_DebounceType state;
    Hm_DebounceInit(&state, 3, 2);
    Hm_DebounceStep(&state, 1);
    Hm_DebounceStep(&state, 1);
    assert(!state.active);
    Hm_DebounceStep(&state, 1);
    assert(state.active);
    Hm_DebounceStep(&state, 0);
    assert(state.active);
    Hm_DebounceStep(&state, 0);
    assert(!state.active);

    Hm_RuntimeType runtime;
    Hm_RuntimeInit(&runtime, 2, 1);
    assert(!Hm_RuntimeObserveSequence(&runtime, 4));
    assert(!Hm_RuntimeObserveSequence(&runtime, 4));
    assert(Hm_RuntimeObserveSequence(&runtime, 4));
    assert(!Hm_RuntimeObserveSequence(&runtime, 5));
    assert(!Hm_RuntimeObserveSequence(&runtime, 4));
    assert(Hm_RuntimeObserveSequence(&runtime, 4));
    assert(Hm_RuntimeObserveSequence(&runtime, 5));
    assert(!Hm_RuntimeObserveSequence(&runtime, 6));

    Hm_RuntimeInit(&runtime, 2, 1);
    assert(!Hm_RuntimeObserveSequence(&runtime, UINT32_MAX));
    assert(!Hm_RuntimeObserveSequence(&runtime, 0U));

    Hm_RuntimeRecordRtf(&runtime, 2);
    assert(Hm_RuntimeRtfCount(&runtime, 2) == 1U);
    return 0;
}
