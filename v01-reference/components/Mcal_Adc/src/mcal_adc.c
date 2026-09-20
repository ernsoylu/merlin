#include "Mcal_Adc.h"

static Mcal_ResultType validate_config(const Mcal_AdcHandleType *handle,
                                       const Mcal_AdcConfigType *config)
{
    return handle == 0 || config == 0 ||
           (config->mode != MCAL_ADC_TOUT && config->mode != MCAL_ADC_VDD) ||
           config->clockDiv < 8U || config->clockDiv > 32U
        ? MCAL_INVALID_ARG : MCAL_OK;
}

#ifdef ESP_PLATFORM

#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP8266)

#include "driver/adc.h"
#include "esp_err.h"

static Mcal_ResultType map_error(esp_err_t error)
{
    return error == ESP_OK ? MCAL_OK :
           error == ESP_ERR_INVALID_ARG ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Adc_Init(Mcal_AdcHandleType *handle,
                              const Mcal_AdcConfigType *config)
{
    if (validate_config(handle, config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
    adc_config_t native = {
        .mode = (adc_mode_t)config->mode,
        .clk_div = config->clockDiv
    };
    const Mcal_ResultType result = map_error(adc_init(&native));
    handle->initialized = result == MCAL_OK ? 1U : 0U;
    if (result == MCAL_OK) {
        handle->mode = config->mode;
    }
    return result;
}

Mcal_ResultType Mcal_Adc_Read(const Mcal_AdcHandleType *handle,
                              uint16_t *value)
{
    if (handle == 0 || !handle->initialized || value == 0) {
        return MCAL_INVALID_ARG;
    }
    return map_error(adc_read(value));
}

#else

Mcal_ResultType Mcal_Adc_Init(Mcal_AdcHandleType *handle,
                              const Mcal_AdcConfigType *config)
{
    (void)handle;
    (void)config;
    return MCAL_UNSUPPORTED;
}

Mcal_ResultType Mcal_Adc_Read(const Mcal_AdcHandleType *handle,
                              uint16_t *value)
{
    (void)handle;
    (void)value;
    return MCAL_UNSUPPORTED;
}

#endif

#else

Mcal_ResultType Mcal_Adc_Init(Mcal_AdcHandleType *handle,
                              const Mcal_AdcConfigType *config)
{
    if (validate_config(handle, config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
    handle->initialized = 0U;
    return MCAL_UNSUPPORTED;
}

Mcal_ResultType Mcal_Adc_Read(const Mcal_AdcHandleType *handle,
                              uint16_t *value)
{
    if (handle == 0 || !handle->initialized || value == 0) {
        return MCAL_INVALID_ARG;
    }
    return MCAL_UNSUPPORTED;
}

#endif
