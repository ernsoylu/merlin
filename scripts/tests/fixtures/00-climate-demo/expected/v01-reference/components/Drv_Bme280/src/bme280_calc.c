#include "bme280_calc.h"

static int32_t compensate_temperature(const Bme280_CalibrationType *c, int32_t adc, int32_t *fine)
{
    int32_t var1 = (((adc >> 3) - ((int32_t)c->dig_T1 << 1)) * c->dig_T2) >> 11;
    int32_t delta = (adc >> 4) - (int32_t)c->dig_T1;
    int32_t var2 = (((delta * delta) >> 12) * c->dig_T3) >> 14;
    *fine = var1 + var2;
    return (*fine * 5 + 128) >> 8;
}

static uint32_t compensate_pressure(const Bme280_CalibrationType *c, int32_t adc, int32_t fine)
{
    int64_t var1 = (int64_t)fine - 128000;
    int64_t var2 = var1 * var1 * c->dig_P6;
    var2 += (var1 * c->dig_P5) * ((int64_t)1 << 17);
    var2 += (int64_t)c->dig_P4 * ((int64_t)1 << 35);
    var1 = ((var1 * var1 * c->dig_P3) >> 8) +
           (var1 * c->dig_P2) * ((int64_t)1 << 12);
    var1 = ((((int64_t)1 << 47) + var1) * c->dig_P1) >> 33;
    if (var1 == 0) {
        return 0;
    }

    int64_t pressure = 1048576 - adc;
    pressure = (((pressure << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)c->dig_P9 * (pressure >> 13) * (pressure >> 13)) >> 25;
    var2 = ((int64_t)c->dig_P8 * pressure) >> 19;
    pressure = ((pressure + var1 + var2) >> 8) + (int64_t)c->dig_P7 * 16;
    return (uint32_t)((pressure + 128) >> 8);
}

static uint32_t compensate_humidity(const Bme280_CalibrationType *c, int32_t adc, int32_t fine)
{
    int32_t value = fine - 76800;
    value = (((((adc * 16384) - ((int32_t)c->dig_H4 * 1048576) -
                 ((int32_t)c->dig_H5 * value)) + 16384) >> 15) *
             (((((((value * c->dig_H6) >> 10) *
                   (((value * c->dig_H3) >> 11) + 32768)) >> 10) + 2097152) *
                 c->dig_H2 + 8192) >> 14));
    value -= (((((value >> 15) * (value >> 15)) >> 7) * c->dig_H1) >> 4);
    if (value < 0) {
        value = 0;
    }
    if (value > 419430400) {
        value = 419430400;
    }
    return (uint32_t)(((int64_t)value * 1000) >> 22);
}

void Bme280_Compensate(const Bme280_CalibrationType *calibration,
                       int32_t adcTemperature,
                       int32_t adcPressure,
                       int32_t adcHumidity,
                       Bme280_CompensatedType *output)
{
    int32_t fine;
    output->temperatureCentiDegC = compensate_temperature(calibration, adcTemperature, &fine);
    output->pressurePa = compensate_pressure(calibration, adcPressure, fine);
    output->humidityMilliPercent = compensate_humidity(calibration, adcHumidity, fine);
}
