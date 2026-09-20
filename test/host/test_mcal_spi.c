#include <assert.h>

#include "Mcal_Spi.h"

int main(void)
{
    uint8_t capabilities = 0xFFU;
    uint8_t selected = 0xFFU;

    assert(Mcal_Spi_GetCapabilities(0) == MCAL_INVALID_ARG);
    assert(Mcal_Spi_GetCapabilities(&capabilities) == MCAL_OK);
    assert(capabilities == 0U);
    assert(Mcal_Spi_Select(0U, &selected) == MCAL_INVALID_ARG);
    assert(Mcal_Spi_Select(MCAL_SPI_HSPI, 0) == MCAL_INVALID_ARG);
    assert(Mcal_Spi_Select(MCAL_SPI_HSPI, &selected) == MCAL_UNSUPPORTED);
    assert(selected == 0U);
    return 0;
}
