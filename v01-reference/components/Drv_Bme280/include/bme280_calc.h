#ifndef BME280_CALC_H
#define BME280_CALC_H

#include <stdint.h>

typedef struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
    uint8_t dig_H1;
    int16_t dig_H2;
    uint8_t dig_H3;
    int16_t dig_H4;
    int16_t dig_H5;
    int8_t dig_H6;
} Bme280_CalibrationType;

typedef struct {
    int32_t temperatureCentiDegC;
    uint32_t pressurePa;
    uint32_t humidityMilliPercent;
} Bme280_CompensatedType;

void Bme280_Compensate(const Bme280_CalibrationType *calibration,
                       int32_t adcTemperature,
                       int32_t adcPressure,
                       int32_t adcHumidity,
                       Bme280_CompensatedType *output);

#endif
