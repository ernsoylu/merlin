#include <assert.h>

#include "Mcal_Pwm.h"

int main(void)
{
    Mcal_PwmHandleType handle = {0};
    const Mcal_PwmConfigType config = {
        .timer = 0,
        .channel = 0,
        .pin = 4,
        .frequencyHz = 1000U,
        .resolutionBits = 10U,
        .initialDutyPermille = 500U
    };

    assert(Mcal_Pwm_Init(0, &config) == MCAL_INVALID_ARG);
    assert(Mcal_Pwm_Init(&handle, 0) == MCAL_INVALID_ARG);
    assert(Mcal_Pwm_Init(&handle, &config) == MCAL_HW_FAIL);
    assert(Mcal_Pwm_SetDuty(0, 500U) == MCAL_INVALID_ARG);
    assert(Mcal_Pwm_SetDuty(&handle, 500U) == MCAL_INVALID_ARG);
    return 0;
}
