#include <assert.h>

#include "Mcal_Adc.h"

int main(void)
{
    Mcal_AdcHandleType handle = {0};
    const Mcal_AdcConfigType config = {
        .mode = MCAL_ADC_TOUT,
        .clockDiv = 8U
    };
    const Mcal_AdcConfigType badClock = {
        .mode = MCAL_ADC_TOUT,
        .clockDiv = 7U
    };
    uint16_t value = 0U;

    assert(Mcal_Adc_Init(0, &config) == MCAL_INVALID_ARG);
    assert(Mcal_Adc_Init(&handle, 0) == MCAL_INVALID_ARG);
    assert(Mcal_Adc_Init(&handle, &badClock) == MCAL_INVALID_ARG);
    assert(Mcal_Adc_Init(&handle, &config) == MCAL_UNSUPPORTED);
    assert(Mcal_Adc_Read(0, &value) == MCAL_INVALID_ARG);
    assert(Mcal_Adc_Read(&handle, 0) == MCAL_INVALID_ARG);
    return 0;
}
