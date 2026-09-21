#include "Mcal_Gpt.h"

Mcal_ResultType Mcal_Gptimer_ValidateConfig(const Mcal_GptimerConfigType *config)
{
    if (config == 0 || config->resolutionHz == 0U || config->periodUs == 0U ||
        config->alarm == 0) {
        return MCAL_INVALID_ARG;
    }
    const uint64_t ticks = ((uint64_t)config->resolutionHz * config->periodUs) /
                           1000000ULL;
    return ticks == 0U || ticks > UINT64_MAX ? MCAL_INVALID_ARG : MCAL_OK;
}

#if defined(ESP_PLATFORM) && !defined(MERLIN_HW364A)

#include "esp_timer.h"

Mcal_ResultType Mcal_Gpt_GetTimeUs(int64_t *timeUs)
{
    if (timeUs == 0) {
        return MCAL_INVALID_ARG;
    }
    *timeUs = esp_timer_get_time();
    return MCAL_OK;
}

#else

Mcal_ResultType Mcal_Gpt_GetTimeUs(int64_t *timeUs)
{
    return timeUs == 0 ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

#endif

#if defined(ESP_PLATFORM) && !defined(MERLIN_HW364A)

#include "driver/gptimer.h"
#include "esp_err.h"

static Mcal_ResultType gptimer_map_error(esp_err_t error)
{
    return error == ESP_OK ? MCAL_OK :
           error == ESP_ERR_INVALID_ARG ? MCAL_INVALID_ARG :
           error == ESP_ERR_INVALID_STATE ? MCAL_BUSY : MCAL_HW_FAIL;
}

static bool gptimer_alarm(gptimer_handle_t timer,
                          const gptimer_alarm_event_data_t *event,
                          void *context)
{
    (void)timer;
    (void)event;
    Mcal_GptimerHandleType *handle = context;
    if (handle != 0) {
        handle->alarmCount++;
        handle->config.alarm(handle->config.context);
    }
    return false;
}

Mcal_ResultType Mcal_Gptimer_Init(Mcal_GptimerHandleType *handle,
                                  const Mcal_GptimerConfigType *config)
{
    if (handle == 0 || Mcal_Gptimer_ValidateConfig(config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
    *handle = (Mcal_GptimerHandleType){.config = *config};
    const gptimer_config_t timerConfig = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = config->resolutionHz,
        .intr_priority = config->interruptPriority
    };
    gptimer_handle_t timer = 0;
    esp_err_t error = gptimer_new_timer(&timerConfig, &timer);
    if (error == ESP_OK) {
        const gptimer_event_callbacks_t callbacks = {.on_alarm = gptimer_alarm};
        error = gptimer_register_event_callbacks(timer, &callbacks, handle);
    }
    if (error == ESP_OK) {
        const uint64_t alarmTicks = ((uint64_t)config->resolutionHz *
                                     config->periodUs) / 1000000ULL;
        const gptimer_alarm_config_t alarm = {
            .alarm_count = alarmTicks,
            .reload_count = 0U,
            .flags.auto_reload_on_alarm = 1U
        };
        error = gptimer_set_alarm_action(timer, &alarm);
    }
    if (error == ESP_OK) {
        error = gptimer_enable(timer);
    }
    if (error != ESP_OK) {
        if (timer != 0) {
            (void)gptimer_del_timer(timer);
        }
        return gptimer_map_error(error);
    }
    handle->timer = timer;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Gptimer_Start(Mcal_GptimerHandleType *handle)
{
    if (handle == 0 || handle->timer == 0 ||
        handle->state == MCAL_GPTIMER_RELEASED) {
        return MCAL_INVALID_ARG;
    }
    if (handle->state == MCAL_GPTIMER_RUNNING) {
        return MCAL_BUSY;
    }
    const Mcal_ResultType result = gptimer_map_error(
        gptimer_start(handle->timer));
    if (result == MCAL_OK) {
        handle->state = MCAL_GPTIMER_RUNNING;
    }
    return result;
}

Mcal_ResultType Mcal_Gptimer_Stop(Mcal_GptimerHandleType *handle)
{
    if (handle == 0 || handle->timer == 0 ||
        handle->state == MCAL_GPTIMER_RELEASED) {
        return MCAL_INVALID_ARG;
    }
    if (handle->state == MCAL_GPTIMER_STOPPED) {
        return MCAL_BUSY;
    }
    const Mcal_ResultType result = gptimer_map_error(
        gptimer_stop(handle->timer));
    if (result == MCAL_OK) {
        handle->state = MCAL_GPTIMER_STOPPED;
    }
    return result;
}

Mcal_ResultType Mcal_Gptimer_Release(Mcal_GptimerHandleType *handle)
{
    if (handle == 0 || handle->timer == 0 ||
        handle->state == MCAL_GPTIMER_RUNNING) {
        return MCAL_INVALID_ARG;
    }
    Mcal_ResultType result = gptimer_map_error(gptimer_disable(handle->timer));
    if (result == MCAL_OK) {
        result = gptimer_map_error(gptimer_del_timer(handle->timer));
    }
    if (result == MCAL_OK) {
        handle->timer = 0;
        handle->state = MCAL_GPTIMER_RELEASED;
    }
    return result;
}

#else

Mcal_ResultType Mcal_Gptimer_Init(Mcal_GptimerHandleType *handle,
                                  const Mcal_GptimerConfigType *config)
{
    if (handle == 0 || Mcal_Gptimer_ValidateConfig(config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
    *handle = (Mcal_GptimerHandleType){.config = *config};
    return MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Gptimer_Start(Mcal_GptimerHandleType *handle)
{
    return handle == 0 ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Gptimer_Stop(Mcal_GptimerHandleType *handle)
{
    return handle == 0 ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Gptimer_Release(Mcal_GptimerHandleType *handle)
{
    return handle == 0 ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

#endif

uint32_t Mcal_Gptimer_AlarmCount(const Mcal_GptimerHandleType *handle)
{
    return handle == 0 ? 0U : handle->alarmCount;
}
