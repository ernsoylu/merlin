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
    return 0;
}
