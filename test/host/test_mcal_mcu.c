#include <assert.h>

#include "Mcal_Mcu.h"

int main(void)
{
    Mcal_McuResetReasonType reason = MCAL_MCU_RESET_POWERON;

    assert(Mcal_Mcu_GetResetReason(0) == MCAL_INVALID_ARG);
    assert(Mcal_Mcu_GetResetReason(&reason) == MCAL_HW_FAIL);
    /* A failed query must not leave a caller believing in a fresh power-on. */
    assert(reason == MCAL_MCU_RESET_POWERON);
    assert(Mcal_Mcu_Reset() == MCAL_HW_FAIL);
    return 0;
}
