#include <assert.h>

#include "Mcal_Icu.h"

int main(void)
{
    const Mcal_IcuConfigType valid = {
        .edgePin = 4, .levelPin = -1, .lowLimit = -100,
        .highLimit = 100, .glitchFilterNs = 1000U
    };
    Mcal_IcuConfigType invalid = valid;
    invalid.edgePin = -1;
    assert(Mcal_Icu_ValidateConfig(&valid) == MCAL_OK);
    assert(Mcal_Icu_ValidateConfig(&invalid) == MCAL_INVALID_ARG);
    invalid = valid;
    invalid.lowLimit = 0;
    assert(Mcal_Icu_ValidateConfig(&invalid) == MCAL_INVALID_ARG);
    invalid = valid;
    invalid.highLimit = 0;
    assert(Mcal_Icu_ValidateConfig(&invalid) == MCAL_INVALID_ARG);
    invalid = valid;
    invalid.lowLimit = 100;
    assert(Mcal_Icu_ValidateConfig(&invalid) == MCAL_INVALID_ARG);

    Mcal_IcuHandleType handle;
    assert(Mcal_Icu_Init(&handle, &valid) == MCAL_HW_FAIL);
    assert(Mcal_Icu_GetOverflowCount(&handle) == 0U);
    assert(Mcal_Icu_Start(&handle) == MCAL_HW_FAIL);
    assert(Mcal_Icu_Read(&handle, 0) == MCAL_INVALID_ARG);
    assert(Mcal_Icu_Read(&handle, &(int64_t){0}) == MCAL_HW_FAIL);
    assert(Mcal_Icu_Init(0, &valid) == MCAL_INVALID_ARG);
    return 0;
}
