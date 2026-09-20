#include "Mcal_Wdg.h"

static Mcal_ResultType validate_config(const Mcal_WdgConfigType *config)
{
    return config == 0 || config->timeoutMs == 0U
        ? MCAL_INVALID_ARG : MCAL_OK;
}

#ifdef ESP_PLATFORM

#include "esp_err.h"
#include "esp_task_wdt.h"

#if !defined(MERLIN_HW364A)
static Mcal_ResultType map_error(esp_err_t error)
{
    return error == ESP_OK ? MCAL_OK :
           error == ESP_ERR_INVALID_ARG ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}
#endif

Mcal_ResultType Mcal_Wdg_Init(Mcal_WdgHandleType *handle,
                              const Mcal_WdgConfigType *config)
{
    if (handle == 0 || validate_config(config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
#ifdef MERLIN_HW364A
    /* ESP8266 RTOS SDK exposes a global watchdog subscription only. */
    (void)esp_task_wdt_init();
#else
    const Mcal_ResultType result = map_error(esp_task_wdt_add(NULL));
    if (result != MCAL_OK && result != MCAL_INVALID_ARG) {
        handle->initialized = 0U;
        return result;
    }
#endif
    handle->timeoutMs = config->timeoutMs;
    handle->initialized = 1U;
    return MCAL_OK;
}

Mcal_ResultType Mcal_Wdg_Feed(const Mcal_WdgHandleType *handle)
{
    if (handle == 0 || !handle->initialized) {
        return MCAL_INVALID_ARG;
    }
    (void)esp_task_wdt_reset();
    return MCAL_OK;
}

Mcal_ResultType Mcal_Wdg_Stop(Mcal_WdgHandleType *handle)
{
    if (handle == 0 || !handle->initialized) {
        return MCAL_INVALID_ARG;
    }
#ifdef MERLIN_HW364A
    /* This SDK has no per-task unsubscribe API; parking the task is the
     * supported SAFE_HALT behavior for this reference. */
    return MCAL_HW_FAIL;
#else
    const Mcal_ResultType result = map_error(esp_task_wdt_delete(NULL));
    if (result == MCAL_OK) {
        handle->initialized = 0U;
    }
    return result;
#endif
}

#else

Mcal_ResultType Mcal_Wdg_Init(Mcal_WdgHandleType *handle,
                              const Mcal_WdgConfigType *config)
{
    if (handle == 0 || validate_config(config) != MCAL_OK) {
        return MCAL_INVALID_ARG;
    }
    handle->initialized = 0U;
    return MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Wdg_Feed(const Mcal_WdgHandleType *handle)
{
    return handle == 0 || !handle->initialized ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Wdg_Stop(Mcal_WdgHandleType *handle)
{
    if (handle == 0 || !handle->initialized) {
        return MCAL_INVALID_ARG;
    }
    return MCAL_HW_FAIL;
}

#endif
