#include "Mcal_Pwm.h"

#ifdef ESP_PLATFORM

#include "driver/ledc.h"
#include "esp_err.h"

static Mcal_ResultType map_error(esp_err_t error)
{
    return error == ESP_OK ? MCAL_OK :
           error == ESP_ERR_INVALID_ARG ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Pwm_Init(Mcal_PwmHandleType *handle,
                              const Mcal_PwmConfigType *config)
{
    if (handle == 0 || config == 0 || config->frequencyHz == 0U ||
        config->resolutionBits == 0U || config->resolutionBits > 15U ||
        config->initialDutyPermille > 1000U) {
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

#else

Mcal_ResultType Mcal_Pwm_Init(Mcal_PwmHandleType *handle,
                              const Mcal_PwmConfigType *config)
{
    (void)handle;
    (void)config;
    return MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Pwm_SetDuty(const Mcal_PwmHandleType *handle,
                                 uint16_t dutyPermille)
{
    (void)handle;
    (void)dutyPermille;
    return MCAL_HW_FAIL;
}

#endif
