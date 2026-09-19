#ifndef BME280_H
#define BME280_H

#include <stdint.h>

#include "bme280_calc.h"
#include "Mcal_I2c.h"
#include "Rte_Type.h"

typedef enum {
    BME280_STATE_START = 0,
    BME280_STATE_CHECK,
    BME280_STATE_READ
} Bme280_StateType;

typedef enum {
    BME280_HEALTH_INITIAL = 0,
    BME280_HEALTH_READY,
    BME280_HEALTH_DEGRADED
} Bme280_HealthType;

typedef enum {
    BME280_INIT_OK = 0,
    BME280_INIT_CHIP_ID_MISMATCH
} Bme280_InitStatusType;

typedef struct {
    Bme280_CalibrationType calibration;
    Bme280_CompensatedType sample;
    Mcal_I2cInterfaceType i2c;
    uint8_t address;
    uint32_t sequence;
    int64_t sampleTimeUs;
    Rte_QualityType quality[3];
    Bme280_StateType state;
    Bme280_HealthType health;
    Mcal_ResultType lastResult;
} Bme280_InstanceType;

void Bme280_InstanceInit(Bme280_InstanceType *instance,
                         uint8_t address,
                         const Bme280_CalibrationType *calibration,
                         Mcal_I2cInterfaceType i2c);
Mcal_ResultType Bme280_Init(Bme280_InstanceType *instance);
void Bme280_CopyEnvironmental(const Bme280_InstanceType *instance,
                              Rte_EnvironmentalDataType *out);
Mcal_ResultType Bme280_MainFunction_High(Bme280_InstanceType *instance,
                                          int64_t nowUs);

#endif
