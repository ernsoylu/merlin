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

int EcuM_CheckBootLoop(EcuM_ContextType *context, uint32_t resets,
                       uint32_t windowMs)
{
    context->bootLoopCount = resets;
    if (resets >= 5U && windowMs <= 300000U) {
        EcuM_EnterSafeHalt(context);
        return 1;
    }
    return 0;
}

void EcuM_EnterSafeHalt(EcuM_ContextType *context)
{
    context->state = ECUM_SAFE_HALT;
    context->watchdogSubscribed = 0U;
    context->resetRequested = 0U;
#ifdef ESP_PLATFORM
    Os_UnsubscribeWatchdog();
#endif
}
