#ifndef IOHWAB_FAN_H
#define IOHWAB_FAN_H

#include <stdint.h>

typedef struct {
    uint32_t holdCycles;
    uint32_t staleCycles;
    float failsafeDuty;
    float appliedDuty;
} IoHwAb_FanType;

void IoHwAb_FanInit(IoHwAb_FanType *fan, uint32_t holdCycles,
                    float failsafeDuty);
float IoHwAb_FanApply(IoHwAb_FanType *fan, float requestedDuty,
                     int commandValid);

#endif
