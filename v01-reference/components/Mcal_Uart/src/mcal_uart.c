#include "Mcal_Uart.h"

static Mcal_ResultType validate_config(const Mcal_UartConfigType *config)
{
    return config == 0 || config->port < 0 || config->baudRate == 0U ||
           config->rxBufferSize == 0U || config->txBufferSize == 0U
        ? MCAL_INVALID_ARG : MCAL_OK;
}

#ifdef ESP_PLATFORM

#include "driver/uart.h"
#include "esp_err.h"

static Mcal_ResultType map_error(esp_err_t error)
{
    return error == ESP_OK ? MCAL_OK :
           error == ESP_ERR_INVALID_ARG ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Uart_Init(Mcal_UartHandleType *handle,
                               const Mcal_UartConfigType *config)
{
    if (handle == 0 || validate_config(config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
    const uart_config_t native = {
        .baud_rate = (int)config->baudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    Mcal_ResultType result = map_error(uart_param_config((uart_port_t)config->port,
                                                          &native));
    if (result != MCAL_OK) {
        handle->initialized = 0U;
        return result;
    }
    const esp_err_t install = uart_driver_install((uart_port_t)config->port,
                                                   config->rxBufferSize,
                                                   config->txBufferSize, 0, 0, 0);
    if (install != ESP_OK && install != ESP_ERR_INVALID_STATE) {
        handle->initialized = 0U;
        return map_error(install);
    }
    result = map_error(uart_set_pin((uart_port_t)config->port, config->txPin,
                                    config->rxPin, UART_PIN_NO_CHANGE,
                                    UART_PIN_NO_CHANGE));
    if (result != MCAL_OK) {
        handle->initialized = 0U;
        return result;
    }
    handle->port = config->port;
    handle->initialized = 1U;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Uart_Write(const Mcal_UartHandleType *handle,
                                const uint8_t *data, uint16_t length)
{
    if (handle == 0 || !handle->initialized || data == 0 || length == 0U) {
        return MCAL_INVALID_ARG;
    }
    return uart_write_bytes((uart_port_t)handle->port, (const char *)data,
                            (size_t)length) == (int)length
        ? MCAL_OK : MCAL_HW_FAIL;
}

#else

Mcal_ResultType Mcal_Uart_Init(Mcal_UartHandleType *handle,
                               const Mcal_UartConfigType *config)
{
    if (handle == 0 || validate_config(config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
    handle->initialized = 0U;
    return MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Uart_Write(const Mcal_UartHandleType *handle,
                                const uint8_t *data, uint16_t length)
{
    if (handle == 0 || !handle->initialized || data == 0 || length == 0U) {
        return MCAL_INVALID_ARG;
    }
    return MCAL_HW_FAIL;
}

#endif
