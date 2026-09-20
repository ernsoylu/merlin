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

static Mcal_ResultType async_validate(const Mcal_I2cAsyncQueueType *queue,
                                      const Mcal_I2cAsyncRequestType *request)
{
    if (queue == 0 || request == 0 || request->length == 0U) {
        return MCAL_INVALID_ARG;
    }
    if (request->operation == MCAL_I2C_ASYNC_WRITE_REGISTER) {
        return queue->interface.writeRegister != 0 && request->writeData != 0
            ? MCAL_OK : MCAL_INVALID_ARG;
    }
    if (request->operation == MCAL_I2C_ASYNC_READ_REGISTER) {
        return queue->interface.readRegister != 0 && request->readData != 0
            ? MCAL_OK : MCAL_INVALID_ARG;
    }
    return MCAL_INVALID_ARG;
}

void Mcal_I2cAsync_Init(Mcal_I2cAsyncQueueType *queue,
                        Mcal_I2cInterfaceType interface)
{
    if (queue == 0) {
        return;
    }
    *queue = (Mcal_I2cAsyncQueueType){.interface = interface};
}

Mcal_ResultType Mcal_I2cAsync_Submit(Mcal_I2cAsyncQueueType *queue,
                                     Mcal_I2cAsyncRequestType *request)
{
    Mcal_ResultType valid = async_validate(queue, request);
    if (valid != MCAL_OK || request->queued) {
        return valid != MCAL_OK ? valid : MCAL_BUSY;
    }
    if (queue->count == MCAL_I2C_ASYNC_CAPACITY) {
        return MCAL_BUSY;
    }
    request->completed = 0U;
    request->result = MCAL_BUSY;
    request->queued = 1U;
    queue->requests[queue->tail] = request;
    queue->tail = (uint8_t)((queue->tail + 1U) % MCAL_I2C_ASYNC_CAPACITY);
    queue->count++;
    return MCAL_OK;
}

Mcal_ResultType Mcal_I2cAsync_Service(Mcal_I2cAsyncQueueType *queue)
{
    if (queue == 0) {
        return MCAL_INVALID_ARG;
    }
    if (queue->count == 0U) {
        return MCAL_BUSY;
    }
    Mcal_I2cAsyncRequestType *request = queue->requests[queue->head];
    queue->head = (uint8_t)((queue->head + 1U) % MCAL_I2C_ASYNC_CAPACITY);
    queue->count--;
    request->queued = 0U;
    if (request->operation == MCAL_I2C_ASYNC_WRITE_REGISTER) {
        request->result = queue->interface.writeRegister(
            queue->interface.context, request->address, request->reg,
            request->writeData, request->length);
    } else {
        request->result = queue->interface.readRegister(
            queue->interface.context, request->address, request->reg,
            request->readData, request->length);
    }
    request->completed = 1U;
    if (request->completion != 0) {
        request->completion(request->completionContext, request->result);
    }
    return request->result;
}

Mcal_ResultType Mcal_I2cAsync_Cancel(Mcal_I2cAsyncQueueType *queue,
                                     Mcal_I2cAsyncRequestType *request)
{
    if (queue == 0 || request == 0) {
        return MCAL_INVALID_ARG;
    }
    uint8_t found = 0U;
    const uint8_t originalCount = queue->count;
    for (uint8_t i = 0U; i < originalCount; ++i) {
        Mcal_I2cAsyncRequestType *candidate = queue->requests[
            (uint8_t)((queue->head + i) % MCAL_I2C_ASYNC_CAPACITY)];
        if (candidate == request) {
            found = 1U;
        } else if (found) {
            queue->requests[(uint8_t)((queue->head + i - 1U) %
                                       MCAL_I2C_ASYNC_CAPACITY)] = candidate;
        }
    }
    if (!found) {
        return MCAL_BUSY;
    }
    request->queued = 0U;
    queue->count = (uint8_t)(originalCount - 1U);
    queue->tail = (uint8_t)((queue->head + queue->count) %
                            MCAL_I2C_ASYNC_CAPACITY);
    return MCAL_OK;
}

Mcal_ResultType Mcal_I2c_TransferBounded(Mcal_I2cAsyncQueueType *queue,
                                         Mcal_I2cAsyncRequestType *request,
                                         uint8_t serviceBudget)
{
    if (serviceBudget == 0U) {
        return MCAL_TIMEOUT;
    }
    Mcal_ResultType result = Mcal_I2cAsync_Submit(queue, request);
    if (result != MCAL_OK) {
        return result;
    }
    for (uint8_t step = 0U; step < serviceBudget; ++step) {
        (void)Mcal_I2cAsync_Service(queue);
        if (request->completed) {
            return request->result;
        }
    }
    (void)Mcal_I2cAsync_Cancel(queue, request);
    return MCAL_TIMEOUT;
}

uint8_t Mcal_I2cAsync_Pending(const Mcal_I2cAsyncQueueType *queue)
{
    return queue == 0 ? 0U : queue->count;
}
