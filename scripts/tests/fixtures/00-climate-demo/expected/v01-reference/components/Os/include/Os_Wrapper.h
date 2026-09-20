#ifndef OS_WRAPPER_H
#define OS_WRAPPER_H

#include <stdint.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#define OS_ISR_ATTR IRAM_ATTR
#else
#define OS_ISR_ATTR
#endif

typedef struct {
    int64_t expectedTick;
    int64_t periodTicks;
    int64_t jitterToleranceTicks;
    uint32_t skippedActivations;
    uint32_t lateActivations;
    uint32_t deadlineMisses;
    uint32_t lastFault;
    int64_t jitterMinTicks;
    int64_t jitterMaxTicks;
    uint32_t jitterSamples;
    uint32_t wakeCount;
} Os_ReleaseStateType;

void Os_ReleaseInit(Os_ReleaseStateType *state, int64_t firstBoundary);
void Os_ReleaseConfigure(Os_ReleaseStateType *state, int64_t periodTicks,
                         int64_t jitterToleranceTicks);
int Os_ReleaseSkip(Os_ReleaseStateType *state, int64_t actualTick, int64_t periodTicks);
int Os_ReleaseRecordWake(Os_ReleaseStateType *state, int64_t actualTick);
int Os_DeadlineCheck(Os_ReleaseStateType *state, int64_t completionTick,
                     int64_t deadlineTick);

#define OS_RTF_NONE 0U
#define OS_RTF_DEADLINE 2U
#define OS_RTF_LATE_ACTIVATION 3U

typedef void (*Os_RunnableFn)(void *context);
typedef void (*Os_FaultFn)(void *context, uint32_t fault, int64_t value);

#ifdef ESP_PLATFORM
typedef StackType_t Os_StackType;
typedef StaticTask_t Os_TaskStorageType;
#else
typedef uint32_t Os_StackType;
typedef uint32_t Os_TaskStorageType;
#endif

typedef struct {
    const char *name;
    uint32_t periodMs;
    uint32_t deadlineMs;
    uint32_t jitterToleranceMs;
    uint32_t priority;
    Os_RunnableFn runnable;
    Os_FaultFn fault;
    void *context;
    Os_StackType *stack;
    uint32_t stackWords;
    Os_TaskStorageType storage;
    Os_ReleaseStateType release;
#ifdef ESP_PLATFORM
    TaskHandle_t handle;
#endif
} Os_TaskConfigType;

typedef struct {
    const char *name;
    uint32_t priority;
    Os_RunnableFn runnable;
    void *context;
    Os_StackType *stack;
    uint32_t stackWords;
    Os_TaskStorageType storage;
#ifdef ESP_PLATFORM
    TaskHandle_t handle;
#endif
} Os_EventTaskConfigType;

int Os_EventTaskDispatch(Os_EventTaskConfigType *config);

#if defined(ESP_PLATFORM) && !defined(MERLIN_HW364A)
int Os_CreateStaticTask(Os_TaskConfigType *config);
int Os_ReleaseTask(Os_TaskConfigType *config);
int Os_CreateStaticEventTask(Os_EventTaskConfigType *config);
int Os_NotifyEventFromIsr(Os_EventTaskConfigType *config) OS_ISR_ATTR;
void Os_UnsubscribeWatchdog(void);
#endif

#endif
