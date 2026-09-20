#include <assert.h>

#include "Mcal_Radio.h"
#include "Mcal_Wlan.h"

int main(void)
{
    Mcal_WlanHandleType handle = {0};
    uint8_t capabilities = 0U;

    assert(Mcal_Wlan_Init(0) == MCAL_INVALID_ARG);
    assert(Mcal_Wlan_Start(&handle) == MCAL_INVALID_ARG);
    assert(Mcal_Wlan_Stop(&handle) == MCAL_INVALID_ARG);

    /* Same source, two builds: the gate must follow the build's declared
     * capability, not the caller's intent. Without WLAN the SDK is never
     * reached; with it, init fails only because there is no host radio. */
    assert(Mcal_Radio_GetCapabilities(&capabilities) == MCAL_OK);
    if ((capabilities & MCAL_RADIO_WLAN) != 0U) {
        assert(Mcal_Wlan_Init(&handle) == MCAL_HW_FAIL);
    } else {
        assert(Mcal_Wlan_Init(&handle) == MCAL_UNSUPPORTED);
    }
    assert(handle.initialized == 0U);
    assert(handle.started == 0U);
    return 0;
}
