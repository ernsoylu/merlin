#include <assert.h>

#include "Mcal_Gpt.h"

static void alarm(void *context)
{
    unsigned int *calls = context;
    (*calls)++;
}

int main(void)
{
    int64_t timeUs = 0;

    assert(Mcal_Gpt_GetTimeUs(0) == MCAL_INVALID_ARG);
    assert(Mcal_Gpt_GetTimeUs(&timeUs) == MCAL_HW_FAIL);

    unsigned int calls = 0U;
    const Mcal_GptimerConfigType valid = {
        .resolutionHz = 1000000U, .periodUs = 1000U,
        .alarm = alarm, .context = &calls
    };
    Mcal_GptimerConfigType invalid = valid;
    invalid.periodUs = 0U;
    assert(Mcal_Gptimer_ValidateConfig(&valid) == MCAL_OK);
    assert(Mcal_Gptimer_ValidateConfig(&invalid) == MCAL_INVALID_ARG);
    invalid = valid;
    invalid.alarm = 0;
    assert(Mcal_Gptimer_ValidateConfig(&invalid) == MCAL_INVALID_ARG);

    Mcal_GptimerHandleType handle;
    assert(Mcal_Gptimer_Init(&handle, &valid) == MCAL_HW_FAIL);
    assert(Mcal_Gptimer_AlarmCount(&handle) == 0U);
    assert(Mcal_Gptimer_Start(&handle) == MCAL_HW_FAIL);
    assert(Mcal_Gptimer_Release(&handle) == MCAL_HW_FAIL);
    return 0;
}
