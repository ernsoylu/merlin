#include <assert.h>

#include "Mcal_Wdg.h"

int main(void)
{
    const Mcal_WdgConfigType config = {.timeoutMs = 15000U};
    Mcal_WdgHandleType handle = {0};

    assert(Mcal_Wdg_Init(0, &config) == MCAL_INVALID_ARG);
    assert(Mcal_Wdg_Init(&handle, &(Mcal_WdgConfigType){0}) == MCAL_INVALID_ARG);
    assert(Mcal_Wdg_Feed(&handle) == MCAL_INVALID_ARG);
    assert(Mcal_Wdg_Init(&handle, &config) == MCAL_HW_FAIL);
    assert(Mcal_Wdg_Stop(&handle) == MCAL_INVALID_ARG);
    return 0;
}
