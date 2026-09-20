#include "Mcal_Dio.h"

#ifdef ESP_PLATFORM

#include "driver/gpio.h"
#include "esp_err.h"

#endif

static Mcal_ResultType validate_pin(int32_t pin)
{
#ifdef ESP_PLATFORM
    return pin < 0 || pin >= GPIO_NUM_MAX ? MCAL_INVALID_ARG : MCAL_OK;
#else
    return pin < 0 ? MCAL_INVALID_ARG : MCAL_OK;
#endif
}

#ifdef ESP_PLATFORM

static Mcal_ResultType map_error(esp_err_t error)
{
    return error == ESP_OK ? MCAL_OK :
           error == ESP_ERR_INVALID_ARG ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Dio_Read(int32_t pin, uint8_t *level)
{
    if (level == 0) {
        return MCAL_INVALID_ARG;
    }
    const Mcal_ResultType valid = validate_pin(pin);
    if (valid != MCAL_OK) {
        return valid;
    }
    const int value = gpio_get_level((gpio_num_t)pin);
    if (value < 0) {
        return MCAL_HW_FAIL;
    }
    *level = value != 0 ? 1U : 0U;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Dio_Write(int32_t pin, uint8_t level)
{
    if (level > 1U) {
        return MCAL_INVALID_ARG;
    }
    const Mcal_ResultType valid = validate_pin(pin);
    return valid != MCAL_OK ? valid :
           map_error(gpio_set_level((gpio_num_t)pin, level));
}

#else

Mcal_ResultType Mcal_Dio_Read(int32_t pin, uint8_t *level)
{
    if (level == 0) {
        return MCAL_INVALID_ARG;
    }
    return validate_pin(pin) == MCAL_OK ? MCAL_HW_FAIL : MCAL_INVALID_ARG;
}

Mcal_ResultType Mcal_Dio_Write(int32_t pin, uint8_t level)
{
    if (level > 1U) {
        return MCAL_INVALID_ARG;
    }
    return validate_pin(pin) == MCAL_OK ? MCAL_HW_FAIL : MCAL_INVALID_ARG;
}

#endif
