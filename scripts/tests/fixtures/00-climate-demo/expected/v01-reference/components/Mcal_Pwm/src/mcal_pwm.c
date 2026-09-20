#include "Mcal_Pwm.h"

static Mcal_ResultType validate_config(const Mcal_PwmHandleType *handle,
                                       const Mcal_PwmConfigType *config)
{
    return handle == 0 || config == 0 || config->frequencyHz == 0U ||
           config->resolutionBits == 0U || config->initialDutyPermille > 1000U
        ? MCAL_INVALID_ARG : MCAL_OK;
}

#ifdef ESP_PLATFORM

#include "sdkconfig.h"
#include "esp_err.h"

static Mcal_ResultType map_error(esp_err_t error)
{
    return error == ESP_OK ? MCAL_OK :
           error == ESP_ERR_INVALID_ARG ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

#if defined(CONFIG_IDF_TARGET_ESP8266)

#include "driver/pwm.h"

Mcal_ResultType Mcal_Pwm_Init(Mcal_PwmHandleType *handle,
                              const Mcal_PwmConfigType *config)
{
    if (validate_config(handle, config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }

    /* ESP8266 RTOS SDK PWM is one global, time-based channel group. */
    if (config->timer != 0 || config->channel != 0 || config->pin < 0 ||
        config->pin > 16 || config->frequencyHz > 100000U) {
        return MCAL_UNSUPPORTED;
    }
    const uint32_t periodUs = 1000000U / config->frequencyHz;
    if (periodUs < 10U) {
        return MCAL_INVALID_ARG;
    }
    uint32_t duty = periodUs * config->initialDutyPermille / 1000U;
    const uint32_t pin = (uint32_t)config->pin;
    Mcal_ResultType result = map_error(pwm_init(periodUs, &duty, 1U, &pin));
    if (result != MCAL_OK) {
        handle->initialized = 0U;
        return result;
    }
    handle->channel = 0;
    handle->maxDuty = periodUs;
    handle->initialized = 1U;
    result = map_error(pwm_start());
    if (result != MCAL_OK) {
        handle->initialized = 0U;
    }
    return result;
}

Mcal_ResultType Mcal_Pwm_SetDuty(const Mcal_PwmHandleType *handle,
                                 uint16_t dutyPermille)
{
    if (handle == 0 || !handle->initialized || dutyPermille > 1000U) {
        return MCAL_INVALID_ARG;
    }
    const uint32_t duty = handle->maxDuty * dutyPermille / 1000U;
    Mcal_ResultType result = map_error(pwm_set_duty((uint8_t)handle->channel,
                                                     duty));
    if (result == MCAL_OK) {
        result = map_error(pwm_start());
    }
    return result;
}

#else

#include "driver/ledc.h"

Mcal_ResultType Mcal_Pwm_Init(Mcal_PwmHandleType *handle,
                              const Mcal_PwmConfigType *config)
{
    if (validate_config(handle, config) != MCAL_OK ||
        config->resolutionBits > 15U) {
        return MCAL_INVALID_ARG;
    }
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = config->timer,
        .duty_resolution = config->resolutionBits,
        .freq_hz = config->frequencyHz,
        .clk_cfg = LEDC_AUTO_CLK
    };
    Mcal_ResultType result = map_error(ledc_timer_config(&timer));
    if (result != MCAL_OK) {
        return result;
    }
    const ledc_channel_config_t channel = {
        .gpio_num = config->pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = config->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = config->timer,
        .duty = 0,
        .hpoint = 0
    };
    result = map_error(ledc_channel_config(&channel));
    if (result != MCAL_OK) {
        return result;
    }
    handle->channel = config->channel;
    handle->maxDuty = (1UL << config->resolutionBits) - 1UL;
    handle->initialized = 1U;
    return Mcal_Pwm_SetDuty(handle, config->initialDutyPermille);
}

Mcal_ResultType Mcal_Pwm_SetDuty(const Mcal_PwmHandleType *handle,
                                 uint16_t dutyPermille)
{
    if (handle == 0 || !handle->initialized || dutyPermille > 1000U) {
        return MCAL_INVALID_ARG;
    }
    const uint32_t duty = handle->maxDuty * dutyPermille / 1000U;
    esp_err_t error = ledc_set_duty(LEDC_LOW_SPEED_MODE, handle->channel, duty);
    if (error == ESP_OK) {
        error = ledc_update_duty(LEDC_LOW_SPEED_MODE, handle->channel);
    }
    return map_error(error);
}

#endif

#else

Mcal_ResultType Mcal_Pwm_Init(Mcal_PwmHandleType *handle,
                              const Mcal_PwmConfigType *config)
{
    if (validate_config(handle, config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
    handle->initialized = 0U;
    return MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Pwm_SetDuty(const Mcal_PwmHandleType *handle,
                                 uint16_t dutyPermille)
{
    if (handle == 0 || !handle->initialized || dutyPermille > 1000U) {
        return MCAL_INVALID_ARG;
    }
    return MCAL_HW_FAIL;
}

#endif
