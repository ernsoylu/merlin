#include "Mcal_I2c.h"

#ifdef ESP_PLATFORM

#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_rom_sys.h"

static Mcal_ResultType map_error(esp_err_t error)
{
    if (error == ESP_OK) {
        return MCAL_OK;
    }
    if (error == ESP_ERR_TIMEOUT) {
        return MCAL_TIMEOUT;
    }
    if (error == ESP_ERR_INVALID_ARG || error == ESP_ERR_INVALID_STATE) {
        return MCAL_INVALID_ARG;
    }
    if (error == ESP_FAIL) {
        return MCAL_NACK;
    }
    return MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_I2c_Init(Mcal_I2cHandleType *handle,
                              const Mcal_I2cConfigType *config)
{
    if (handle == 0 || config == 0 || config->frequencyHz == 0U ||
        config->timeoutMs == 0U || config->port < 0) {
        return MCAL_INVALID_ARG;
    }

    const i2c_config_t native = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sdaPin,
        .scl_io_num = config->sclPin,
        .sda_pullup_en = config->pullups ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .scl_pullup_en = config->pullups ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .master.clk_speed = config->frequencyHz
    };
    esp_err_t error = i2c_param_config((i2c_port_t)config->port, &native);
    if (error != ESP_OK) {
        return map_error(error);
    }
    error = i2c_driver_install((i2c_port_t)config->port, I2C_MODE_MASTER,
                               0, 0, 0);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        return map_error(error);
    }
    handle->port = config->port;
    handle->timeoutMs = config->timeoutMs;
    handle->sdaPin = config->sdaPin;
    handle->sclPin = config->sclPin;
    handle->frequencyHz = config->frequencyHz;
    handle->pullups = config->pullups;
    handle->initialized = 1U;
    return MCAL_OK;
}

static Mcal_ResultType write_register(void *context, uint8_t address,
                                      uint8_t reg, const uint8_t *data,
                                      uint16_t length)
{
    Mcal_I2cHandleType *handle = context;
    if (handle == 0 || !handle->initialized || data == 0 || length == 0U) {
        return MCAL_INVALID_ARG;
    }
    uint8_t buffer[256];
    if ((uint32_t)length + 1U > sizeof(buffer)) {
        return MCAL_INVALID_ARG;
    }
    buffer[0] = reg;
    for (uint16_t i = 0; i < length; ++i) {
        buffer[i + 1U] = data[i];
    }
    return map_error(i2c_master_write_to_device((i2c_port_t)handle->port,
                                                 address, buffer,
                                                 (size_t)length + 1U,
                                                 (handle->timeoutMs + portTICK_PERIOD_MS - 1U) /
                                                     portTICK_PERIOD_MS));
}

static Mcal_ResultType read_register(void *context, uint8_t address,
                                     uint8_t reg, uint8_t *data,
                                     uint16_t length)
{
    Mcal_I2cHandleType *handle = context;
    if (handle == 0 || !handle->initialized || data == 0 || length == 0U) {
        return MCAL_INVALID_ARG;
    }
    return map_error(i2c_master_write_read_device(
        (i2c_port_t)handle->port, address, &reg, 1U, data, length,
        (handle->timeoutMs + portTICK_PERIOD_MS - 1U) / portTICK_PERIOD_MS));
}

Mcal_ResultType Mcal_I2c_Recover(Mcal_I2cHandleType *handle)
{
    if (handle == 0 || !handle->initialized) {
        return MCAL_INVALID_ARG;
    }
    (void)i2c_driver_delete((i2c_port_t)handle->port);
    if (gpio_set_direction(handle->sclPin, GPIO_MODE_OUTPUT_OD) != ESP_OK ||
        gpio_set_direction(handle->sdaPin, GPIO_MODE_INPUT) != ESP_OK ||
        gpio_set_pull_mode(handle->sclPin, handle->pullups ? GPIO_PULLUP_ONLY : GPIO_FLOATING) != ESP_OK ||
        gpio_set_pull_mode(handle->sdaPin, handle->pullups ? GPIO_PULLUP_ONLY : GPIO_FLOATING) != ESP_OK) {
        return MCAL_HW_FAIL;
    }
    for (uint8_t pulse = 0; pulse < 9U && gpio_get_level(handle->sdaPin) == 0; ++pulse) {
        if (gpio_set_level(handle->sclPin, 0) != ESP_OK) {
            return MCAL_HW_FAIL;
        }
        esp_rom_delay_us(5U);
        if (gpio_set_level(handle->sclPin, 1) != ESP_OK) {
            return MCAL_HW_FAIL;
        }
        esp_rom_delay_us(5U);
    }
    if (gpio_set_level(handle->sdaPin, 0) != ESP_OK ||
        gpio_set_level(handle->sclPin, 1) != ESP_OK) {
        return MCAL_HW_FAIL;
    }
    esp_rom_delay_us(5U);
    if (gpio_set_direction(handle->sdaPin, GPIO_MODE_OUTPUT_OD) != ESP_OK ||
        gpio_set_level(handle->sdaPin, 1) != ESP_OK) {
        return MCAL_HW_FAIL;
    }
    const Mcal_I2cConfigType config = {
        .port = handle->port, .sdaPin = handle->sdaPin,
        .sclPin = handle->sclPin, .frequencyHz = handle->frequencyHz,
        .timeoutMs = handle->timeoutMs, .pullups = handle->pullups
    };
    return Mcal_I2c_Init(handle, &config);
}

Mcal_I2cInterfaceType Mcal_I2c_GetInterface(Mcal_I2cHandleType *handle)
{
    return (Mcal_I2cInterfaceType){
        .context = handle,
        .writeRegister = write_register,
        .readRegister = read_register,
        .recover = (Mcal_I2cRecoverFn)Mcal_I2c_Recover
    };
}

#else

Mcal_ResultType Mcal_I2c_Init(Mcal_I2cHandleType *handle,
                              const Mcal_I2cConfigType *config)
{
    if (handle == 0 || config == 0 || config->port < 0 ||
        config->sdaPin < 0 || config->sclPin < 0 ||
        config->frequencyHz == 0U || config->timeoutMs == 0U ||
        config->pullups > 1U) {
        return MCAL_INVALID_ARG;
    }
    return MCAL_HW_FAIL;
}

Mcal_I2cInterfaceType Mcal_I2c_GetInterface(Mcal_I2cHandleType *handle)
{
    return (Mcal_I2cInterfaceType){.context = handle};
}

Mcal_ResultType Mcal_I2c_Recover(Mcal_I2cHandleType *handle)
{
    return handle == 0 || !handle->initialized
        ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

#endif
