#include "Mcal_Radio.h"

#ifndef MERLIN_RADIO_CAPABILITIES
#define MERLIN_RADIO_CAPABILITIES 0U
#endif

Mcal_ResultType Mcal_Radio_GetCapabilities(uint8_t *capabilities)
{
    if (capabilities == 0) {
        return MCAL_INVALID_ARG;
    }
    *capabilities = (uint8_t)(MERLIN_RADIO_CAPABILITIES &
                              (MCAL_RADIO_WLAN | MCAL_RADIO_BT));
    return MCAL_OK;
}

Mcal_ResultType Mcal_Radio_Select(uint8_t requested, uint8_t *selected)
{
    if (selected == 0 || requested == 0U ||
        (requested & (uint8_t)~(MCAL_RADIO_WLAN | MCAL_RADIO_BT)) != 0U) {
        return MCAL_INVALID_ARG;
    }
    uint8_t capabilities = 0U;
    (void)Mcal_Radio_GetCapabilities(&capabilities);
    if ((requested & capabilities) != requested) {
        *selected = 0U;
        return MCAL_UNSUPPORTED;
    }
    *selected = requested;
    return MCAL_OK;
}
