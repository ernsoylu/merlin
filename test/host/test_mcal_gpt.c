#include <assert.h>

#include "Mcal_Gpt.h"

int main(void)
{
    int64_t timeUs = 0;

    assert(Mcal_Gpt_GetTimeUs(0) == MCAL_INVALID_ARG);
    assert(Mcal_Gpt_GetTimeUs(&timeUs) == MCAL_HW_FAIL);
    return 0;
}
