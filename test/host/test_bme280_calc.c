#include <assert.h>
#include "bme280_calc.h"

int main(void)
{
    const Bme280_CalibrationType calibration = {
        .dig_T1 = 27504, .dig_T2 = 26435, .dig_T3 = -1000,
        .dig_P1 = 36477, .dig_P2 = -10685, .dig_P3 = 3024,
        .dig_P4 = 2855, .dig_P5 = 140, .dig_P6 = -7,
        .dig_P7 = 15500, .dig_P8 = -14600, .dig_P9 = 6000,
        .dig_H1 = 75, .dig_H2 = 362, .dig_H3 = 0,
        .dig_H4 = 325, .dig_H5 = 50, .dig_H6 = 30
    };
    Bme280_CompensatedType output;
    Bme280_Compensate(&calibration, 519888, 415148, 32257, &output);
    assert(output.temperatureCentiDegC == 2508);
    assert(output.pressurePa >= 100653U && output.pressurePa <= 100654U);
    assert(output.humidityMilliPercent >= 60000U && output.humidityMilliPercent <= 65000U);
    return 0;
}
