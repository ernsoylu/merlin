#ifndef MCAL_GPT_H
#define MCAL_GPT_H

#include <stdint.h>

#include "Std_Types.h"

/* Free-running target timebase used for bounded execution measurements. */
Mcal_ResultType Mcal_Gpt_GetTimeUs(int64_t *timeUs);

typedef void (*Mcal_GptimerAlarmFn)(void *context);

typedef struct {
    uint32_t resolutionHz;
    uint32_t periodUs;
    int32_t interruptPriority;
    Mcal_GptimerAlarmFn alarm;
    void *context;
} Mcal_GptimerConfigType;

typedef enum {
    MCAL_GPTIMER_STOPPED = 0,
    MCAL_GPTIMER_RUNNING,
    MCAL_GPTIMER_RELEASED
} Mcal_GptimerStateType;

typedef struct {
    void *timer;
    Mcal_GptimerConfigType config;
    Mcal_GptimerStateType state;
    uint32_t alarmCount;
} Mcal_GptimerHandleType;

Mcal_ResultType Mcal_Gptimer_ValidateConfig(const Mcal_GptimerConfigType *config);
Mcal_ResultType Mcal_Gptimer_Init(Mcal_GptimerHandleType *handle,
                                  const Mcal_GptimerConfigType *config);
Mcal_ResultType Mcal_Gptimer_Start(Mcal_GptimerHandleType *handle);
Mcal_ResultType Mcal_Gptimer_Stop(Mcal_GptimerHandleType *handle);
Mcal_ResultType Mcal_Gptimer_Release(Mcal_GptimerHandleType *handle);
uint32_t Mcal_Gptimer_AlarmCount(const Mcal_GptimerHandleType *handle);

#endif
