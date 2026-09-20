#include "Os_Wrapper.h"

#if defined(ESP_PLATFORM) && !defined(MERLIN_HW364A)
#include "esp_task_wdt.h"
#include "esp_timer.h"

static void task_entry(void *argument)
{
    Os_TaskConfigType *config = argument;
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    /* The startup gate opens before this subscription by design. */
    (void)esp_task_wdt_add(NULL);
    const TickType_t periodTicks = pdMS_TO_TICKS(config->periodMs);
    const TickType_t jitterTicks = pdMS_TO_TICKS(config->jitterToleranceMs);
    TickType_t lastWake = xTaskGetTickCount();
    Os_ReleaseInit(&config->release, (int64_t)lastWake + periodTicks);
    Os_ReleaseConfigure(&config->release, periodTicks, jitterTicks);

    for (;;) {
        vTaskDelayUntil(&lastWake, periodTicks);
        config->release.wakeCount++;
        const TickType_t actualWake = xTaskGetTickCount();
        if (Os_ReleaseSkip(&config->release, actualWake, periodTicks)) {
            esp_task_wdt_reset();
            continue;
        }
        if (Os_ReleaseRecordWake(&config->release, actualWake) && config->fault != 0) {
            config->fault(config->context, OS_RTF_LATE_ACTIVATION, actualWake);
        }
        const int64_t startUs = esp_timer_get_time();
        config->runnable(config->context);
        const int64_t elapsedUs = esp_timer_get_time() - startUs;
        if (Os_DeadlineCheck(&config->release, elapsedUs,
                             (int64_t)config->deadlineMs * 1000) &&
            config->fault != 0) {
            config->fault(config->context, OS_RTF_DEADLINE, elapsedUs);
        }
        /* Exactly one feed for each completed activation. */
        esp_task_wdt_reset();
    }
}

int Os_CreateStaticTask(Os_TaskConfigType *config)
{
    if (config == 0 || config->runnable == 0 || config->stack == 0 ||
        config->stackWords == 0U || config->periodMs == 0U) {
        return 0;
    }
    config->handle = xTaskCreateStatic(task_entry, config->name,
                                       config->stackWords, config,
                                       config->priority, config->stack,
                                       &config->storage);
    return config->handle != NULL;
}

int Os_ReleaseTask(Os_TaskConfigType *config)
{
    if (config == 0 || config->handle == NULL) {
        return 0;
    }
    return xTaskNotifyGive(config->handle) == pdPASS;
}

void Os_UnsubscribeWatchdog(void)
{
    (void)esp_task_wdt_delete(NULL);
}
#endif

void Os_ReleaseInit(Os_ReleaseStateType *state, int64_t firstBoundary)
{
    *state = (Os_ReleaseStateType){.expectedTick = firstBoundary};
}

void Os_ReleaseConfigure(Os_ReleaseStateType *state, int64_t periodTicks,
                         int64_t jitterToleranceTicks)
{
    state->periodTicks = periodTicks;
    state->jitterToleranceTicks = jitterToleranceTicks;
}

int Os_ReleaseSkip(Os_ReleaseStateType *state, int64_t actualTick, int64_t periodTicks)
{
    if (actualTick <= state->expectedTick || periodTicks <= 0) {
        return 0;
    }

    state->expectedTick = ((actualTick / periodTicks) + 1) * periodTicks;
    state->skippedActivations++;
    state->lastFault = OS_RTF_LATE_ACTIVATION;
    return 1;
}

int Os_ReleaseRecordWake(Os_ReleaseStateType *state, int64_t actualTick)
{
    if (state->periodTicks <= 0) {
        return 0;
    }
    const int64_t jitterTicks = actualTick - state->expectedTick;
    if (state->jitterSamples == 0U || jitterTicks < state->jitterMinTicks) {
        state->jitterMinTicks = jitterTicks;
    }
    if (jitterTicks > state->jitterMaxTicks) {
        state->jitterMaxTicks = jitterTicks;
    }
    state->jitterSamples++;
    if (actualTick > state->expectedTick + state->jitterToleranceTicks) {
        state->lateActivations++;
        state->lastFault = OS_RTF_LATE_ACTIVATION;
        return 1;
    }
    state->expectedTick += state->periodTicks;
    return 0;
}

int Os_DeadlineCheck(Os_ReleaseStateType *state, int64_t completionTick,
                     int64_t deadlineTick)
{
    if (completionTick <= deadlineTick) {
        return 0;
    }
    state->deadlineMisses++;
    state->lastFault = OS_RTF_DEADLINE;
    return 1;
}
