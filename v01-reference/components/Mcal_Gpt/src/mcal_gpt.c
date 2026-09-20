#include "Mcal_Gpt.h"

#ifdef ESP_PLATFORM

#include "esp_timer.h"

Mcal_ResultType Mcal_Gpt_GetTimeUs(int64_t *timeUs)
{
    if (timeUs == 0) {
        return MCAL_INVALID_ARG;
    }
    *timeUs = esp_timer_get_time();
    return MCAL_OK;
}

#else

Mcal_ResultType Mcal_Gpt_GetTimeUs(int64_t *timeUs)
{
    return timeUs == 0 ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

#endif
