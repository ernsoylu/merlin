#include "Mcal_Spi.h"

#ifdef ESP_PLATFORM

#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP8266)
#define MCAL_TARGET_SPI_CAPABILITIES MCAL_SPI_HSPI
#else
#define MCAL_TARGET_SPI_CAPABILITIES 0U
#endif

#else

#define MCAL_TARGET_SPI_CAPABILITIES 0U

#endif

#ifndef MERLIN_SPI_CAPABILITIES
#define MERLIN_SPI_CAPABILITIES MCAL_TARGET_SPI_CAPABILITIES
#endif

Mcal_ResultType Mcal_Spi_GetCapabilities(uint8_t *capabilities)
{
    if (capabilities == 0) {
        return MCAL_INVALID_ARG;
    }
    *capabilities = (uint8_t)(MERLIN_SPI_CAPABILITIES & MCAL_SPI_HSPI);
    return MCAL_OK;
}

Mcal_ResultType Mcal_Spi_Select(uint8_t requested, uint8_t *selected)
{
    if (selected == 0 || requested == 0U ||
        (requested & (uint8_t)~MCAL_SPI_HSPI) != 0U) {
        return MCAL_INVALID_ARG;
    }
    uint8_t capabilities = 0U;
    (void)Mcal_Spi_GetCapabilities(&capabilities);
    if ((requested & capabilities) != requested) {
        *selected = 0U;
        return MCAL_UNSUPPORTED;
    }
    *selected = requested;
    return MCAL_OK;
}
