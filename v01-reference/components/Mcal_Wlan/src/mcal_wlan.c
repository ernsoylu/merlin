#include "Mcal_Wlan.h"

#ifdef ESP_PLATFORM

#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "tcpip_adapter.h"

static Mcal_ResultType map_error(esp_err_t error)
{
    return error == ESP_OK ? MCAL_OK :
           error == ESP_ERR_INVALID_ARG ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

static esp_err_t event_handler(void *context, system_event_t *event)
{
    (void)context;
    (void)event;
    return ESP_OK;
}

Mcal_ResultType Mcal_Wlan_Init(Mcal_WlanHandleType *handle)
{
    if (handle == 0) {
        return MCAL_INVALID_ARG;
    }
    tcpip_adapter_init();
    esp_err_t error = esp_event_loop_init(event_handler, 0);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        return map_error(error);
    }
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    error = esp_wifi_init(&config);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        return map_error(error);
    }
    error = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (error != ESP_OK) {
        return map_error(error);
    }
    error = esp_wifi_set_mode(WIFI_MODE_NULL);
    if (error != ESP_OK) {
        return map_error(error);
    }
    handle->initialized = 1U;
    handle->started = 0U;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Wlan_Start(Mcal_WlanHandleType *handle)
{
    if (handle == 0 || !handle->initialized) {
        return MCAL_INVALID_ARG;
    }
    const Mcal_ResultType result = map_error(esp_wifi_start());
    if (result == MCAL_OK || result == MCAL_INVALID_ARG) {
        handle->started = 1U;
        return MCAL_OK;
    }
    return result;
}

Mcal_ResultType Mcal_Wlan_Stop(Mcal_WlanHandleType *handle)
{
    if (handle == 0 || !handle->initialized) {
        return MCAL_INVALID_ARG;
    }
    const Mcal_ResultType result = map_error(esp_wifi_stop());
    if (result == MCAL_OK || result == MCAL_INVALID_ARG) {
        handle->started = 0U;
        return MCAL_OK;
    }
    return result;
}

#else

Mcal_ResultType Mcal_Wlan_Init(Mcal_WlanHandleType *handle)
{
    if (handle == 0) {
        return MCAL_INVALID_ARG;
    }
    handle->initialized = 0U;
    handle->started = 0U;
    return MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Wlan_Start(Mcal_WlanHandleType *handle)
{
    return handle == 0 || !handle->initialized ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Wlan_Stop(Mcal_WlanHandleType *handle)
{
    return handle == 0 || !handle->initialized ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

#endif
