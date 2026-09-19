#include <stdio.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

#include "DisplayDemo.h"
#include "Mcal_I2c.h"
#include "ssd1306_frame.h"

#define HW364A_SDA_GPIO 14
#define HW364A_SCL_GPIO 12
#define HW364A_I2C_PORT I2C_NUM_0
#define HW364A_OLED_ADDRESS 0x3CU

static esp_err_t last_i2c_error;

static Mcal_ResultType map_error(esp_err_t error)
{
    if (error == ESP_OK) {
        return MCAL_OK;
    }
    if (error == ESP_ERR_TIMEOUT) {
        return MCAL_TIMEOUT;
    }
    if (error == ESP_FAIL) {
        return MCAL_NACK;
    }
    return MCAL_HW_FAIL;
}

static Mcal_ResultType transfer(Mcal_I2cHandleType *handle, uint8_t address,
                                uint8_t control, const uint8_t *data,
                                uint16_t length)
{
    i2c_cmd_handle_t command = i2c_cmd_link_create();
    if (command == 0) {
        return MCAL_HW_FAIL;
    }
    i2c_master_start(command);
    i2c_master_write_byte(command, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(command, control, true);
    i2c_master_write(command, (uint8_t *)data, length, true);
    i2c_master_stop(command);
    const esp_err_t error = i2c_master_cmd_begin(
        (i2c_port_t)handle->port, command,
        (handle->timeoutMs + portTICK_PERIOD_MS - 1U) / portTICK_PERIOD_MS);
    last_i2c_error = error;
    i2c_cmd_link_delete(command);
    return map_error(error);
}

static Mcal_ResultType write_register(void *context, uint8_t address,
                                      uint8_t reg, const uint8_t *data,
                                      uint16_t length)
{
    Mcal_I2cHandleType *handle = context;
    if (handle == 0 || !handle->initialized || data == 0 || length == 0U) {
        return MCAL_INVALID_ARG;
    }
    return transfer(handle, address, reg, data, length);
}

static Mcal_ResultType recover(void *context)
{
    Mcal_I2cHandleType *handle = context;
    if (handle == 0 || !handle->initialized) {
        return MCAL_INVALID_ARG;
    }
    (void)i2c_driver_delete((i2c_port_t)handle->port);
    (void)gpio_set_direction((gpio_num_t)handle->sclPin, GPIO_MODE_OUTPUT_OD);
    (void)gpio_set_direction((gpio_num_t)handle->sdaPin, GPIO_MODE_INPUT);
    for (uint8_t pulse = 0U; pulse < 9U &&
         gpio_get_level((gpio_num_t)handle->sdaPin) == 0; ++pulse) {
        (void)gpio_set_level((gpio_num_t)handle->sclPin, 0U);
        ets_delay_us(5U);
        (void)gpio_set_level((gpio_num_t)handle->sclPin, 1U);
        ets_delay_us(5U);
    }
    return Mcal_I2c_Init(handle, &(Mcal_I2cConfigType){
        .port = handle->port, .sdaPin = handle->sdaPin,
        .sclPin = handle->sclPin, .frequencyHz = handle->frequencyHz,
        .timeoutMs = handle->timeoutMs, .pullups = handle->pullups
    });
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
        .sda_pullup_en = config->pullups ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .scl_io_num = config->sclPin,
        .scl_pullup_en = config->pullups ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .clk_stretch_tick = 300
    };
    /* ESP8266 RTOS SDK v3.4 requires install before parameter configuration. */
    esp_err_t error = i2c_driver_install((i2c_port_t)config->port,
                                         I2C_MODE_MASTER);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        return map_error(error);
    }
    error = i2c_param_config((i2c_port_t)config->port, &native);
    if (error != ESP_OK) {
        return map_error(error);
    }
    *handle = (Mcal_I2cHandleType){
        .port = config->port, .timeoutMs = config->timeoutMs,
        .sdaPin = config->sdaPin, .sclPin = config->sclPin,
        .frequencyHz = config->frequencyHz, .pullups = config->pullups,
        .initialized = 1U
    };
    return MCAL_OK;
}

Mcal_I2cInterfaceType Mcal_I2c_GetInterface(Mcal_I2cHandleType *handle)
{
    return (Mcal_I2cInterfaceType){
        .context = handle, .writeRegister = write_register,
        .recover = recover
    };
}

static void print_status(const Ssd1306_InstanceType *display)
{
    printf("{\"display\":\"onboardOled\",\"health\":%u,\"completed\":%u,\"failures\":%u}\n",
           (unsigned)display->health, (unsigned)display->lastCompletedSequence,
           (unsigned)display->transferFailures);
}

void app_main(void)
{
    static Mcal_I2cHandleType i2c;
    static Ssd1306_InstanceType display;
    static DisplayDemo_CtxType demo;
    const Mcal_I2cConfigType config = {
        .port = HW364A_I2C_PORT, .sdaPin = HW364A_SDA_GPIO,
        .sclPin = HW364A_SCL_GPIO, .frequencyHz = 100000U,
        .timeoutMs = 1000U, .pullups = 1U
    };
    const Mcal_ResultType i2cResult = Mcal_I2c_Init(&i2c, &config);
    printf("hw364a_i2c_init result=%u\n", (unsigned)i2cResult);
    if (i2cResult != MCAL_OK) {
        puts("{\"system\":\"SAFE_HALT\",\"reason\":\"I2C_INIT\"}");
        return;
    }
    vTaskDelay(100U / portTICK_PERIOD_MS);
    Ssd1306_InstanceInit(&display, HW364A_OLED_ADDRESS,
                         Mcal_I2c_GetInterface(&i2c));
    const Mcal_ResultType displayResult = Ssd1306_Initialize(&display);
    printf("hw364a_oled_init result=%u native=%d\n", (unsigned)displayResult,
           (int)last_i2c_error);
    if (displayResult != MCAL_OK) {
        puts("{\"display\":\"onboardOled\",\"health\":\"DEGRADED\"}");
        return;
    }
    DisplayDemo_Init(&demo);
    for (;;) {
        DisplayDemo_Run(&demo);
        const Rte_MonochromeFrameType *frame = DisplayDemo_GetFrame(&demo);
        const Ssd1306_FrameViewType view = {
            .width = frame->width, .height = frame->height,
            .sequence = frame->sequence, .pixels = frame->pixels,
            .pixelBytes = RTE_MONOCHROME_FRAME_BYTES
        };
        if (Ssd1306_SubmitFrame(&display, &view) != MCAL_OK) {
            puts("{\"display\":\"onboardOled\",\"health\":\"DEGRADED\"}");
            return;
        }
        while (display.activeValid) {
            if (Ssd1306_TransferChunk(&display) != MCAL_OK) {
                print_status(&display);
                return;
            }
            esp_task_wdt_reset();
            vTaskDelay(1U / portTICK_PERIOD_MS);
        }
        print_status(&display);
        vTaskDelay(500U / portTICK_PERIOD_MS);
    }
}
