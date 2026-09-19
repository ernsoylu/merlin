#ifndef ECUM_H
#define ECUM_H

#include <stdint.h>

typedef enum {
    ECUM_STARTUP = 0,
    ECUM_RUN,
    ECUM_DEGRADED,
    ECUM_SHUTDOWN,
    ECUM_SAFE_HALT
} EcuM_StateType;

typedef struct {
    EcuM_StateType state;
    int64_t startupEpochUs;
    uint32_t initFailures;
    uint32_t deadlineFaults;
    uint32_t controlledResets;
    uint32_t bootLoopCount;
    uint32_t runActivations;
    uint32_t *bootLoopCounter;
    uint8_t gateReleased;
    uint8_t watchdogSubscribed;
    uint8_t resetRequested;
} EcuM_ContextType;

void EcuM_ContextInit(EcuM_ContextType *context, uint32_t bootLoopCount);
int EcuM_Release(EcuM_ContextType *context, int64_t epochUs);
void EcuM_RecordInitFailure(EcuM_ContextType *context);
void EcuM_RecordDeadlineFault(EcuM_ContextType *context);
void EcuM_RecordRunActivation(EcuM_ContextType *context);
int EcuM_CheckBootLoop(EcuM_ContextType *context, uint32_t resets,
                       uint32_t windowMs);
void EcuM_EnterSafeHalt(EcuM_ContextType *context);

void EcuM_Startup(void);

#endif
