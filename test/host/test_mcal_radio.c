#include <assert.h>

#include "Mcal_Radio.h"

int main(void)
{
    uint8_t capabilities = 0xFFU;
    uint8_t selected = 0xFFU;

    assert(Mcal_Radio_GetCapabilities(0) == MCAL_INVALID_ARG);
    assert(Mcal_Radio_GetCapabilities(&capabilities) == MCAL_OK);
    assert(capabilities == 0U);
    assert(Mcal_Radio_Select(0U, &selected) == MCAL_INVALID_ARG);
    assert(Mcal_Radio_Select(MCAL_RADIO_WLAN, &selected) == MCAL_UNSUPPORTED);
    assert(selected == 0U);
    return 0;
}
