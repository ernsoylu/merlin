#ifndef MCAL_UART_H
#define MCAL_UART_H

#include <stdint.h>

#include "Std_Types.h"

typedef struct {
    int32_t port;
    int32_t txPin;
    int32_t rxPin;
    uint32_t baudRate;
    uint16_t rxBufferSize;
    uint16_t txBufferSize;
} Mcal_UartConfigType;

typedef struct {
    int32_t port;
    uint8_t initialized;
} Mcal_UartHandleType;

Mcal_ResultType Mcal_Uart_Init(Mcal_UartHandleType *handle,
                               const Mcal_UartConfigType *config);
Mcal_ResultType Mcal_Uart_Write(const Mcal_UartHandleType *handle,
                                const uint8_t *data, uint16_t length);

#endif
