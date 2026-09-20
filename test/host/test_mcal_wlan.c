#include <assert.h>

#include "Mcal_Wlan.h"

int main(void)
{
    Mcal_WlanHandleType handle = {0};

    assert(Mcal_Wlan_Init(0) == MCAL_INVALID_ARG);
    assert(Mcal_Wlan_Start(&handle) == MCAL_INVALID_ARG);
    assert(Mcal_Wlan_Stop(&handle) == MCAL_INVALID_ARG);
    assert(Mcal_Wlan_Init(&handle) == MCAL_HW_FAIL);
    return 0;
}
