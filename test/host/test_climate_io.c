#include <assert.h>

#include "ClimateController.h"
#include "IoHwAb_Fan.h"
#include "Rte_Type.h"

int main(void)
{
    ClimateController_CtxType controller;
    ClimateController_Init(&controller, 22.0f);
    Rte_EnvironmentalDataType sample = {
        .temperatureDegC = 20.0f,
        .quality = {RTE_QUALITY_VALID, RTE_QUALITY_VALID, RTE_QUALITY_VALID}
    };
    float first = ClimateController_Run(&controller, &sample, 1000000);
    float second = ClimateController_Run(&controller, &sample, 1010000);
    assert(first > 0.0f && second >= first);
    assert(controller.previousActivationUs == 1010000);

    IoHwAb_FanType fan;
    IoHwAb_FanInit(&fan, 2, 1.0f);
    assert(IoHwAb_FanApply(&fan, second, 1) == second);
    assert(IoHwAb_FanApply(&fan, 0.0f, 0) == second);
    assert(IoHwAb_FanApply(&fan, 0.0f, 0) == second);
    assert(IoHwAb_FanApply(&fan, 0.0f, 0) == 1.0f);
    return 0;
}
