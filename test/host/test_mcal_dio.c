#include <assert.h>

#include "Mcal_Dio.h"

int main(void)
{
    uint8_t level = 0U;

    assert(Mcal_Dio_Read(0, 0) == MCAL_INVALID_ARG);
    assert(Mcal_Dio_Read(-1, &level) == MCAL_INVALID_ARG);
    assert(Mcal_Dio_Write(-1, 0U) == MCAL_INVALID_ARG);
    assert(Mcal_Dio_Write(0, 2U) == MCAL_INVALID_ARG);
    assert(Mcal_Dio_Read(0, &level) == MCAL_HW_FAIL);
    assert(Mcal_Dio_Write(0, 0U) == MCAL_HW_FAIL);
    return 0;
}
