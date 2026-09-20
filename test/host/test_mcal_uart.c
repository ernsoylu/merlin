#include <assert.h>

#include "Mcal_Uart.h"

int main(void)
{
    const Mcal_UartConfigType config = {
        .port = 0,
        .txPin = 1,
        .rxPin = 3,
        .baudRate = 115200U,
        .rxBufferSize = 256U,
        .txBufferSize = 256U
    };
    Mcal_UartHandleType handle = {0};
    const uint8_t byte = 'x';

    assert(Mcal_Uart_Init(0, &config) == MCAL_INVALID_ARG);
    assert(Mcal_Uart_Init(&handle, &(Mcal_UartConfigType){0}) == MCAL_INVALID_ARG);
    assert(Mcal_Uart_Write(&handle, &byte, 1U) == MCAL_INVALID_ARG);
    assert(Mcal_Uart_Init(&handle, &config) == MCAL_HW_FAIL);
    return 0;
}
