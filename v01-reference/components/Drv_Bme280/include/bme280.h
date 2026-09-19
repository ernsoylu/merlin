#ifndef BME280_H
#define BME280_H

#include "bme280_calc.h"

typedef struct {
    Bme280_CalibrationType calibration;
    Bme280_CompensatedType sample;
    unsigned int sequence;
} Bme280_InstanceType;

#endif
