/* Portable EcuM state machine: no SDK, no application. The ESP32 climate
 * reference's own startup lives in ecum_startup.c so a second ECU image can
 * reuse this half without dragging that application in. */
#include "EcuM.h"

#include "Os_Wrapper.h"


void EcuM_ContextInit(EcuM_ContextType *context, uint32_t bootLoopCount)
{
    *context = (EcuM_ContextType){
        .state = ECUM_STARTUP, .bootLoopCount = bootLoopCount
    };
}

int EcuM_Release(EcuM_ContextType *context, int64_t epochUs)
{
    if (context == 0 || context->gateReleased) {
        return 0;
    }
    context->startupEpochUs = epochUs;
    context->gateReleased = 1U;
    context->watchdogSubscribed = 0U;
    context->state = context->initFailures == 0U ? ECUM_RUN : ECUM_DEGRADED;
    return 1;
}

void EcuM_RecordInitFailure(EcuM_ContextType *context)
{
    context->initFailures++;
    if (context->gateReleased) {
        context->state = ECUM_DEGRADED;
    }
}

void EcuM_RecordDeadlineFault(EcuM_ContextType *context)
{
    context->deadlineFaults++;
    if (context->deadlineFaults >= 5U) {
        context->state = ECUM_SHUTDOWN;
        context->resetRequested = 1U;
        context->controlledResets++;
    }
}

void EcuM_RecordRunActivation(EcuM_ContextType *context)
{
    if (context == 0 || context->state != ECUM_RUN) {
        return;
    }
    context->runActivations++;
    if (context->runActivations >= 240U) {
        if (context->bootLoopCounter != 0) {
            *context->bootLoopCounter = 0U;
        }
        context->runActivations = 0U;
    }
}

int EcuM_EvaluateBootLoop(EcuM_ContextType *context,
                          EcuM_BootLoopType *retained,
                          Mcal_McuResetReasonType reason, int64_t nowUs)
{
    if (context == 0 || retained == 0) {
        return 0;
    }
    /* Retained memory is undefined after a power-on or brownout, so the window
     * is re-armed rather than trusted -- including when it holds a start time
     * in the future, which would otherwise never expire. */
    const int expired = (nowUs - retained->windowStartUs) > ECUM_BOOT_LOOP_WINDOW_US ||
                        nowUs < retained->windowStartUs;
    if (reason == MCAL_MCU_RESET_POWERON || reason == MCAL_MCU_RESET_BROWNOUT ||
        expired) {
        retained->resets = 0U;
        retained->windowStartUs = nowUs;
    }
    if (retained->resets >= ECUM_BOOT_LOOP_MAX_RESETS) {
        context->bootLoopCount = retained->resets;
        EcuM_EnterSafeHalt(context);
        return 1;
    }
    retained->resets++;
    context->bootLoopCount = retained->resets;
    /* Sustained running clears the counter through EcuM_RecordRunActivation. */
    context->bootLoopCounter = &retained->resets;
    return 0;
}

void EcuM_EnterSafeHalt(EcuM_ContextType *context)
{
    context->state = ECUM_SAFE_HALT;
    context->watchdogSubscribed = 0U;
    context->resetRequested = 0U;
/* Matches Os_Wrapper.h: the ESP8266 SDK has no per-task unsubscribe, so that
 * target reaches SAFE_HALT by parking the task instead. */
#if defined(ESP_PLATFORM) && !defined(MERLIN_HW364A)
    Os_UnsubscribeWatchdog();
#endif
}
