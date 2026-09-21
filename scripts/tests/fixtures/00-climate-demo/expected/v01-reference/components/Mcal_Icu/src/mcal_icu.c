#include "Mcal_Icu.h"

Mcal_ResultType Mcal_Icu_ValidateConfig(const Mcal_IcuConfigType *config)
{
    if (config == 0 || config->edgePin < 0 || config->lowLimit >= 0 ||
        config->highLimit <= 0 || config->lowLimit >= config->highLimit) {
        return MCAL_INVALID_ARG;
    }
    return MCAL_OK;
}

#ifdef ESP_PLATFORM

#include "driver/pulse_cnt.h"
#include "esp_err.h"

static Mcal_ResultType map_error(esp_err_t error)
{
    if (error == ESP_OK) {
        return MCAL_OK;
    }
    if (error == ESP_ERR_INVALID_ARG) {
        return MCAL_INVALID_ARG;
    }
    if (error == ESP_ERR_INVALID_STATE) {
        return MCAL_BUSY;
    }
    return MCAL_HW_FAIL;
}

static bool on_watch_point(pcnt_unit_handle_t unit,
                           const pcnt_watch_event_data_t *event,
                           void *context)
{
    (void)unit;
    (void)event;
    Mcal_IcuHandleType *handle = context;
    if (handle != 0) {
        handle->overflowCount++;
    }
    return false;
}

Mcal_ResultType Mcal_Icu_Init(Mcal_IcuHandleType *handle,
                              const Mcal_IcuConfigType *config)
{
    if (handle == 0 || Mcal_Icu_ValidateConfig(config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
    *handle = (Mcal_IcuHandleType){.config = *config};
    const pcnt_unit_config_t unitConfig = {
        .low_limit = config->lowLimit,
        .high_limit = config->highLimit,
        .flags.accum_count = 1U
    };
    pcnt_unit_handle_t unit = 0;
    esp_err_t error = pcnt_new_unit(&unitConfig, &unit);
    if (error != ESP_OK) {
        return map_error(error);
    }
    const pcnt_chan_config_t channelConfig = {
        .edge_gpio_num = config->edgePin,
        .level_gpio_num = config->levelPin
    };
    pcnt_channel_handle_t channel = 0;
    error = pcnt_new_channel(unit, &channelConfig, &channel);
    if (error == ESP_OK) {
        error = pcnt_channel_set_edge_action(
            channel, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
            PCNT_CHANNEL_EDGE_ACTION_HOLD);
    }
    if (error == ESP_OK && config->glitchFilterNs != 0U) {
        const pcnt_glitch_filter_config_t filter = {
            .max_glitch_ns = config->glitchFilterNs
        };
        error = pcnt_unit_set_glitch_filter(unit, &filter);
    }
    if (error == ESP_OK) {
        const pcnt_event_callbacks_t callbacks = {.on_reach = on_watch_point};
        error = pcnt_unit_register_event_callbacks(unit, &callbacks, handle);
    }
    if (error == ESP_OK) {
        error = pcnt_unit_add_watch_point(unit, config->lowLimit);
    }
    if (error == ESP_OK) {
        error = pcnt_unit_add_watch_point(unit, config->highLimit);
    }
    if (error != ESP_OK) {
        if (channel != 0) {
            (void)pcnt_del_channel(channel);
        }
        (void)pcnt_del_unit(unit);
        return map_error(error);
    }
    handle->unit = unit;
    handle->channel = channel;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Icu_Start(Mcal_IcuHandleType *handle)
{
    if (handle == 0 || handle->unit == 0) {
        return MCAL_INVALID_ARG;
    }
    if (handle->state == MCAL_ICU_RUNNING) {
        return MCAL_BUSY;
    }
    Mcal_ResultType result = map_error(pcnt_unit_enable(handle->unit));
    if (result == MCAL_OK) {
        result = map_error(pcnt_unit_start(handle->unit));
    }
    if (result != MCAL_OK) {
        (void)pcnt_unit_disable(handle->unit);
        return result;
    }
    handle->state = MCAL_ICU_RUNNING;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Icu_Stop(Mcal_IcuHandleType *handle)
{
    if (handle == 0 || handle->unit == 0) {
        return MCAL_INVALID_ARG;
    }
    if (handle->state == MCAL_ICU_STOPPED) {
        return MCAL_BUSY;
    }
    const Mcal_ResultType result = map_error(pcnt_unit_stop(handle->unit));
    if (result == MCAL_OK) {
        handle->state = MCAL_ICU_STOPPED;
    }
    return result;
}

Mcal_ResultType Mcal_Icu_Clear(Mcal_IcuHandleType *handle)
{
    if (handle == 0 || handle->unit == 0) {
        return MCAL_INVALID_ARG;
    }
    return map_error(pcnt_unit_clear_count(handle->unit));
}

Mcal_ResultType Mcal_Icu_Read(const Mcal_IcuHandleType *handle,
                              int64_t *count)
{
    if (handle == 0 || handle->unit == 0 || count == 0) {
        return MCAL_INVALID_ARG;
    }
    int value = 0;
    const Mcal_ResultType result = map_error(
        pcnt_unit_get_count(handle->unit, &value));
    if (result == MCAL_OK) {
        *count = value;
    }
    return result;
}

#else

Mcal_ResultType Mcal_Icu_Init(Mcal_IcuHandleType *handle,
                              const Mcal_IcuConfigType *config)
{
    if (handle == 0 || Mcal_Icu_ValidateConfig(config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
    *handle = (Mcal_IcuHandleType){.config = *config};
    return MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Icu_Start(Mcal_IcuHandleType *handle)
{
    return handle == 0 ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Icu_Stop(Mcal_IcuHandleType *handle)
{
    return handle == 0 ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Icu_Clear(Mcal_IcuHandleType *handle)
{
    return handle == 0 ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Icu_Read(const Mcal_IcuHandleType *handle,
                              int64_t *count)
{
    return handle == 0 || count == 0 ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

#endif

uint32_t Mcal_Icu_GetOverflowCount(const Mcal_IcuHandleType *handle)
{
    return handle == 0 ? 0U : handle->overflowCount;
}
