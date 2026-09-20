#include "Mcal_Port.h"

#ifdef ESP_PLATFORM

#include "driver/gpio.h"
#include "esp_err.h"

static Mcal_ResultType map_error(esp_err_t error)
{
    return error == ESP_OK ? MCAL_OK :
           error == ESP_ERR_INVALID_ARG ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Port_Init(const Mcal_PortPinConfigType *pins,
                               uint16_t count)
{
    if (pins == 0 || count == 0U) {
        return MCAL_INVALID_ARG;
    }
    for (uint16_t i = 0; i < count; ++i) {
        if (pins[i].pin < 0 || pins[i].pin >= GPIO_NUM_MAX) {
            return MCAL_INVALID_ARG;
        }
        gpio_config_t config = {
            .pin_bit_mask = 1ULL << pins[i].pin,
            .mode = pins[i].output ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        Mcal_ResultType result = map_error(gpio_config(&config));
        if (result != MCAL_OK) {
            return result;
        }
        if (pins[i].output) {
            result = map_error(gpio_set_level(pins[i].pin,
                                              pins[i].initialLevel != 0U));
            if (result != MCAL_OK) {
                return result;
            }
        }
    }
    return MCAL_OK;
}

Mcal_ResultType Mcal_Port_Write(int32_t pin, uint8_t level)
{
    return pin < 0 || pin >= GPIO_NUM_MAX
        ? MCAL_INVALID_ARG : map_error(gpio_set_level(pin, level != 0U));
}

#else

Mcal_ResultType Mcal_Port_Init(const Mcal_PortPinConfigType *pins,
                               uint16_t count)
{
    if (pins == 0 || count == 0U) {
        return MCAL_INVALID_ARG;
    }
    for (uint16_t i = 0; i < count; ++i) {
        if (pins[i].pin < 0 || pins[i].output > 1U ||
            pins[i].initialLevel > 1U) {
            return MCAL_INVALID_ARG;
        }
    }
    return MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Port_Write(int32_t pin, uint8_t level)
{
    return pin < 0 || level > 1U ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

#endif
