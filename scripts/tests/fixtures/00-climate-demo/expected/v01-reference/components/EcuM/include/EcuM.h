#ifndef ECUM_H
#define ECUM_H

#include <stdint.h>

#include "Mcal_Mcu.h"

/* Five qualified resets inside five minutes is a boot loop, not bad luck.
 * Both references use these; a board that needs its own gets a manifest
 * value, not a second copy of the policy. */
#define ECUM_BOOT_LOOP_MAX_RESETS 5U
#define ECUM_BOOT_LOOP_WINDOW_US 300000000LL

typedef struct {
    uint32_t resets;
    int64_t windowStartUs;
} EcuM_BootLoopType;

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
int EcuM_EvaluateBootLoop(EcuM_ContextType *context,
                          EcuM_BootLoopType *retained,
                          Mcal_McuResetReasonType reason, int64_t nowUs);
void EcuM_EnterSafeHalt(EcuM_ContextType *context);

void EcuM_Startup(void);

#endif
