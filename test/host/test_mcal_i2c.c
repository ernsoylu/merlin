#include <assert.h>

#include "Mcal_I2c.h"

int main(void)
{
    const Mcal_I2cConfigType config = {
        .port = 0,
        .sdaPin = 14,
        .sclPin = 12,
        .frequencyHz = 100000U,
        .timeoutMs = 10U,
        .pullups = 1U
    };
    Mcal_I2cHandleType handle = {0};

    assert(Mcal_I2c_Init(0, &config) == MCAL_INVALID_ARG);
    assert(Mcal_I2c_Init(&handle, 0) == MCAL_INVALID_ARG);
    assert(Mcal_I2c_Init(&handle, &(Mcal_I2cConfigType){.port = -1}) ==
           MCAL_INVALID_ARG);
    assert(Mcal_I2c_Init(&handle, &config) == MCAL_HW_FAIL);
    assert(Mcal_I2c_Recover(0) == MCAL_INVALID_ARG);
    assert(Mcal_I2c_Recover(&handle) == MCAL_INVALID_ARG);
    assert(Mcal_I2c_GetInterface(&handle).context == &handle);
    return 0;
}
