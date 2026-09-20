#ifndef MCAL_MCU_H
#define MCAL_MCU_H

#include "Std_Types.h"

/* Target-independent reset causes. The SDK's own reason type never crosses
 * MCAL, so BSW can compare reasons without knowing which backend it runs on.
 * Values are stable: they appear in recorded startup evidence. */
typedef enum {
    MCAL_MCU_RESET_UNKNOWN = 0,
    MCAL_MCU_RESET_POWERON,
    MCAL_MCU_RESET_EXTERNAL,
    MCAL_MCU_RESET_SOFTWARE,
    MCAL_MCU_RESET_PANIC,
    MCAL_MCU_RESET_WATCHDOG,
    MCAL_MCU_RESET_BROWNOUT,
    MCAL_MCU_RESET_DEEPSLEEP
} Mcal_McuResetReasonType;

Mcal_ResultType Mcal_Mcu_GetResetReason(Mcal_McuResetReasonType *reason);

/* Requests a controlled reset. On a target backend this does not return; a
 * return value therefore only ever reports that the reset did not happen. */
Mcal_ResultType Mcal_Mcu_Reset(void);

#endif
