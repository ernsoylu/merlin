#include "Mcal_Mcu.h"

#ifdef ESP_PLATFORM

#include "esp_system.h"

/* Only the reason codes both ESP-IDF 5.2.3 and ESP8266 RTOS SDK v3.4 define
 * are mapped; anything else (USB/JTAG/efuse on IDF, FAST_SW on ESP8266)
 * reports UNKNOWN rather than silently becoming a neighbouring cause. */
static Mcal_McuResetReasonType map_reason(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:   return MCAL_MCU_RESET_POWERON;
    case ESP_RST_EXT:       return MCAL_MCU_RESET_EXTERNAL;
    case ESP_RST_SW:        return MCAL_MCU_RESET_SOFTWARE;
    case ESP_RST_PANIC:     return MCAL_MCU_RESET_PANIC;
    case ESP_RST_INT_WDT:   return MCAL_MCU_RESET_WATCHDOG;
    case ESP_RST_TASK_WDT:  return MCAL_MCU_RESET_WATCHDOG;
    case ESP_RST_WDT:       return MCAL_MCU_RESET_WATCHDOG;
    case ESP_RST_BROWNOUT:  return MCAL_MCU_RESET_BROWNOUT;
    case ESP_RST_DEEPSLEEP: return MCAL_MCU_RESET_DEEPSLEEP;
    default:                return MCAL_MCU_RESET_UNKNOWN;
    }
}

Mcal_ResultType Mcal_Mcu_GetResetReason(Mcal_McuResetReasonType *reason)
{
    if (reason == 0) {
        return MCAL_INVALID_ARG;
    }
    *reason = map_reason(esp_reset_reason());
    return MCAL_OK;
}

Mcal_ResultType Mcal_Mcu_Reset(void)
{
    esp_restart();
    return MCAL_HW_FAIL;
}

#else

Mcal_ResultType Mcal_Mcu_GetResetReason(Mcal_McuResetReasonType *reason)
{
    return reason == 0 ? MCAL_INVALID_ARG : MCAL_HW_FAIL;
}

Mcal_ResultType Mcal_Mcu_Reset(void)
{
    return MCAL_HW_FAIL;
}

#endif
