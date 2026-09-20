#include <assert.h>

#include "Mcal_Port.h"

int main(void)
{
    const Mcal_PortPinConfigType pin = {
        .pin = 4,
        .output = 1U,
        .initialLevel = 0U
    };

    assert(Mcal_Port_Init(0, 1U) == MCAL_INVALID_ARG);
    assert(Mcal_Port_Init(&pin, 0U) == MCAL_INVALID_ARG);
    assert(Mcal_Port_Init(&(Mcal_PortPinConfigType){.pin = -1}, 1U) ==
           MCAL_INVALID_ARG);
    assert(Mcal_Port_Init(&pin, 1U) == MCAL_HW_FAIL);
    assert(Mcal_Port_Write(-1, 0U) == MCAL_INVALID_ARG);
    assert(Mcal_Port_Write(4, 2U) == MCAL_INVALID_ARG);
    assert(Mcal_Port_Write(4, 0U) == MCAL_HW_FAIL);
    return 0;
}
