#ifndef BSWM_H
#define BSWM_H

typedef enum {
    BSWM_MODE_STARTUP = 0,
    BSWM_MODE_RUN,
    BSWM_MODE_DEGRADED,
    BSWM_MODE_SAFE_HALT,
    BSWM_MODE_SHUTDOWN
} BswM_ModeType;

typedef enum {
    BSWM_RESULT_OK = 0,
    BSWM_RESULT_INVALID,
    BSWM_RESULT_REJECTED
} BswM_ResultType;

typedef struct {
    BswM_ModeType mode;
} BswM_ContextType;

void BswM_Init(BswM_ContextType *context);
BswM_ResultType BswM_RequestMode(BswM_ContextType *context,
                                 BswM_ModeType requested);
BswM_ResultType BswM_GetMode(const BswM_ContextType *context,
                             BswM_ModeType *mode);

#endif
