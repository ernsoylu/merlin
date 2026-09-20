#include "IoHwAb_Fan.h"

void IoHwAb_FanInit(IoHwAb_FanType *fan, uint32_t holdCycles,
                    float failsafeDuty)
{
    *fan = (IoHwAb_FanType){
        .holdCycles = holdCycles, .failsafeDuty = failsafeDuty
    };
}

float IoHwAb_FanApply(IoHwAb_FanType *fan, float requestedDuty,
                     int commandValid)
{
    if (commandValid) {
        fan->staleCycles = 0U;
        fan->appliedDuty = requestedDuty;
    } else if (fan->staleCycles < fan->holdCycles) {
        fan->staleCycles++;
    } else {
        fan->appliedDuty = fan->failsafeDuty;
    }
    return fan->appliedDuty;
}
