#include "bme280.h"

#define BME280_REG_STATUS 0xF3U
#define BME280_REG_CTRL_MEAS 0xF4U
#define BME280_REG_DATA 0xF7U
#define BME280_REG_CHIP_ID 0xD0U
#define BME280_REG_CALIB_TP 0x88U
#define BME280_REG_CALIB_H1 0xA1U
#define BME280_REG_CALIB_H2 0xE1U
#define BME280_STATUS_MEASURING 0x08U
#define BME280_FORCED_MEASUREMENT 0x25U
#define BME280_MAX_FAILURES_BEFORE_RECOVERY 3U
#define BME280_RECOVERY_COOLDOWN_ACTIVATIONS 10U

static Mcal_ResultType fail(Bme280_InstanceType *instance,
                            Mcal_ResultType result)
{
    instance->lastResult = result;
    instance->health = BME280_HEALTH_DEGRADED;
    return result;
}

static int recoverable(Mcal_ResultType result)
{
    return result == MCAL_NACK || result == MCAL_TIMEOUT ||
           result == MCAL_ARB_LOST || result == MCAL_HW_FAIL;
}

static Mcal_ResultType runtime_fail(Bme280_InstanceType *instance,
                                    Mcal_ResultType result)
{
    fail(instance, result);
    if (!recoverable(result)) {
        return result;
    }
    instance->consecutiveFailures++;
    if (instance->consecutiveFailures >= BME280_MAX_FAILURES_BEFORE_RECOVERY &&
        instance->recoveryCooldown == 0U && instance->i2c.recover != 0) {
        (void)instance->i2c.recover(instance->i2c.context);
        instance->recoveryCount++;
        instance->consecutiveFailures = 0U;
        instance->recoveryCooldown = BME280_RECOVERY_COOLDOWN_ACTIVATIONS;
    }
    return result;
}

static void runtime_success(Bme280_InstanceType *instance)
{
    instance->consecutiveFailures = 0U;
    if (instance->recoveryCooldown > 0U) {
        instance->recoveryCooldown--;
    }
}

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int16_t read_s16(const uint8_t *data)
{
    return (int16_t)read_u16(data);
}

static void decode_calibration(Bme280_CalibrationType *calibration,
                               const uint8_t tp[24], uint8_t h1,
                               const uint8_t humidity[7])
{
    calibration->dig_T1 = read_u16(&tp[0]);
    calibration->dig_T2 = read_s16(&tp[2]);
    calibration->dig_T3 = read_s16(&tp[4]);
    calibration->dig_P1 = read_u16(&tp[6]);
    calibration->dig_P2 = read_s16(&tp[8]);
    calibration->dig_P3 = read_s16(&tp[10]);
    calibration->dig_P4 = read_s16(&tp[12]);
    calibration->dig_P5 = read_s16(&tp[14]);
    calibration->dig_P6 = read_s16(&tp[16]);
    calibration->dig_P7 = read_s16(&tp[18]);
    calibration->dig_P8 = read_s16(&tp[20]);
    calibration->dig_P9 = read_s16(&tp[22]);
    calibration->dig_H1 = h1;
    calibration->dig_H2 = read_s16(&humidity[0]);
    calibration->dig_H3 = humidity[2];
    calibration->dig_H4 = (int16_t)(((uint16_t)humidity[3] << 4) |
                                    (humidity[4] & 0x0FU));
    calibration->dig_H5 = (int16_t)(((uint16_t)humidity[5] << 4) |
                                    (humidity[4] >> 4));
    calibration->dig_H6 = (int8_t)humidity[6];
}

void Bme280_InstanceInit(Bme280_InstanceType *instance,
                         uint8_t address,
                         const Bme280_CalibrationType *calibration,
                         Mcal_I2cInterfaceType i2c)
{
    *instance = (Bme280_InstanceType){0};
    instance->address = address;
    instance->i2c = i2c;
    if (calibration != 0) {
        instance->calibration = *calibration;
    }
    instance->quality[0] = RTE_QUALITY_INITIAL;
    instance->quality[1] = RTE_QUALITY_INITIAL;
    instance->quality[2] = RTE_QUALITY_INITIAL;
    instance->state = BME280_STATE_START;
    instance->health = BME280_HEALTH_INITIAL;
    instance->lastResult = MCAL_OK;
}

Mcal_ResultType Bme280_Init(Bme280_InstanceType *instance)
{
    if (instance == 0 || instance->i2c.readRegister == 0) {
        return instance == 0 ? MCAL_INVALID_ARG : fail(instance, MCAL_INVALID_ARG);
    }

    uint8_t chipId = 0U;
    Mcal_ResultType result = instance->i2c.readRegister(
        instance->i2c.context, instance->address, BME280_REG_CHIP_ID,
        &chipId, 1U);
    if (result != MCAL_OK) {
        return fail(instance, result);
    }
    if (chipId != 0x60U) {
        return fail(instance, MCAL_HW_FAIL);
    }

    uint8_t tp[24];
    uint8_t h1 = 0U;
    uint8_t humidity[7];
    result = instance->i2c.readRegister(instance->i2c.context, instance->address,
                                        BME280_REG_CALIB_TP, tp, sizeof(tp));
    if (result != MCAL_OK) {
        return fail(instance, result);
    }
    result = instance->i2c.readRegister(instance->i2c.context, instance->address,
                                        BME280_REG_CALIB_H1, &h1, 1U);
    if (result != MCAL_OK) {
        return fail(instance, result);
    }
    result = instance->i2c.readRegister(instance->i2c.context, instance->address,
                                        BME280_REG_CALIB_H2, humidity,
                                        sizeof(humidity));
    if (result != MCAL_OK) {
        return fail(instance, result);
    }
    decode_calibration(&instance->calibration, tp, h1, humidity);
    instance->health = BME280_HEALTH_INITIAL;
    instance->lastResult = MCAL_OK;
    return MCAL_OK;
}

void Bme280_CopyEnvironmental(const Bme280_InstanceType *instance,
                              Rte_EnvironmentalDataType *out)
{
    out->temperatureDegC = (float)instance->sample.temperatureCentiDegC / 100.0f;
    out->humidityPercent = (float)instance->sample.humidityMilliPercent / 1000.0f;
    out->pressurePa = (float)instance->sample.pressurePa;
    out->quality[0] = instance->quality[0] == RTE_QUALITY_VALID
        ? RTE_QUALITY_VALID : RTE_QUALITY_INITIAL;
    out->quality[1] = instance->quality[1] == RTE_QUALITY_VALID
        ? RTE_QUALITY_VALID : RTE_QUALITY_INITIAL;
    out->quality[2] = instance->quality[2] == RTE_QUALITY_VALID
        ? RTE_QUALITY_VALID : RTE_QUALITY_INITIAL;
    out->sampleTimeUs = instance->sampleTimeUs;
    out->sequence = instance->sequence;
}

Mcal_ResultType Bme280_MainFunction_High(Bme280_InstanceType *instance,
                                         int64_t nowUs)
{
    if (instance == 0 || instance->i2c.writeRegister == 0 ||
        instance->i2c.readRegister == 0) {
        return instance == 0 ? MCAL_INVALID_ARG : fail(instance, MCAL_INVALID_ARG);
    }

    if (instance->state == BME280_STATE_START) {
        const uint8_t value = BME280_FORCED_MEASUREMENT;
        const Mcal_ResultType result = instance->i2c.writeRegister(
            instance->i2c.context, instance->address, BME280_REG_CTRL_MEAS,
            &value, 1U);
        if (result == MCAL_OK) {
            runtime_success(instance);
            instance->state = BME280_STATE_CHECK;
            instance->lastResult = MCAL_OK;
        } else {
            return runtime_fail(instance, result);
        }
        return MCAL_OK;
    }

    if (instance->state == BME280_STATE_CHECK) {
        uint8_t status = 0U;
        const Mcal_ResultType result = instance->i2c.readRegister(
            instance->i2c.context, instance->address, BME280_REG_STATUS,
            &status, 1U);
        if (result != MCAL_OK) {
            return runtime_fail(instance, result);
        }
        if ((status & BME280_STATUS_MEASURING) != 0U) {
            instance->lastResult = MCAL_BUSY;
            return MCAL_BUSY;
        }
        instance->state = BME280_STATE_READ;
        runtime_success(instance);
        instance->lastResult = MCAL_OK;
        return MCAL_OK;
    }

    uint8_t data[8];
    const Mcal_ResultType result = instance->i2c.readRegister(
        instance->i2c.context, instance->address, BME280_REG_DATA, data, 8U);
    if (result != MCAL_OK) {
        return runtime_fail(instance, result);
    }

    const int32_t pressure = ((int32_t)data[0] << 12) |
                             ((int32_t)data[1] << 4) | (data[2] >> 4);
    const int32_t temperature = ((int32_t)data[3] << 12) |
                                ((int32_t)data[4] << 4) | (data[5] >> 4);
    const int32_t humidity = ((int32_t)data[6] << 8) | data[7];
    Bme280_Compensate(&instance->calibration, temperature, pressure, humidity,
                      &instance->sample);
    instance->sampleTimeUs = nowUs;
    instance->sequence++;
    instance->quality[0] = RTE_QUALITY_VALID;
    instance->quality[1] = RTE_QUALITY_VALID;
    instance->quality[2] = RTE_QUALITY_VALID;
    instance->state = BME280_STATE_START;
    instance->health = BME280_HEALTH_READY;
    runtime_success(instance);
    instance->lastResult = MCAL_OK;
    return MCAL_OK;
}
