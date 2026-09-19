#include <assert.h>
#include <stdint.h>

#include "bme280.h"

typedef struct {
    uint8_t chipId;
    uint8_t calibrationTp[24];
    uint8_t calibrationH1;
    uint8_t calibrationHumidity[7];
    uint8_t status;
    uint8_t data[8];
    uint8_t lastAddress;
    uint8_t lastRegister;
    unsigned int writes;
    unsigned int reads;
    Mcal_ResultType nextResult;
} MockI2cType;

static Mcal_ResultType write_register(void *context, uint8_t address,
                                      uint8_t reg, const uint8_t *data,
                                      uint16_t length)
{
    MockI2cType *mock = context;
    assert(length == 1U && data[0] == 0x25U);
    mock->lastAddress = address;
    mock->lastRegister = reg;
    mock->writes++;
    return mock->nextResult;
}

static Mcal_ResultType read_register(void *context, uint8_t address,
                                     uint8_t reg, uint8_t *data,
                                     uint16_t length)
{
    MockI2cType *mock = context;
    mock->lastAddress = address;
    mock->lastRegister = reg;
    mock->reads++;
    if (mock->nextResult != MCAL_OK) {
        return mock->nextResult;
    }
    if (reg == 0xF3U) {
        assert(length == 1U);
        data[0] = mock->status;
    } else if (reg == 0xD0U) {
        assert(length == 1U);
        data[0] = mock->chipId;
    } else if (reg == 0x88U) {
        assert(length == sizeof(mock->calibrationTp));
        for (uint16_t i = 0; i < length; ++i) {
            data[i] = mock->calibrationTp[i];
        }
    } else if (reg == 0xA1U) {
        assert(length == 1U);
        data[0] = mock->calibrationH1;
    } else if (reg == 0xE1U) {
        assert(length == sizeof(mock->calibrationHumidity));
        for (uint16_t i = 0; i < length; ++i) {
            data[i] = mock->calibrationHumidity[i];
        }
    } else {
        assert(reg == 0xF7U && length == 8U);
        for (uint16_t i = 0; i < length; ++i) {
            data[i] = mock->data[i];
        }
    }
    return MCAL_OK;
}

static void put_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void encode_calibration(MockI2cType *mock,
                               const Bme280_CalibrationType *calibration)
{
    const int16_t tp[] = {
        (int16_t)calibration->dig_T1, calibration->dig_T2,
        calibration->dig_T3, (int16_t)calibration->dig_P1,
        calibration->dig_P2, calibration->dig_P3, calibration->dig_P4,
        calibration->dig_P5, calibration->dig_P6, calibration->dig_P7,
        calibration->dig_P8, calibration->dig_P9
    };
    for (uint16_t i = 0; i < 12U; ++i) {
        put_u16(&mock->calibrationTp[i * 2U], (uint16_t)tp[i]);
    }
    mock->calibrationH1 = calibration->dig_H1;
    put_u16(&mock->calibrationHumidity[0], (uint16_t)calibration->dig_H2);
    mock->calibrationHumidity[2] = calibration->dig_H3;
    mock->calibrationHumidity[3] = (uint8_t)(calibration->dig_H4 >> 4);
    mock->calibrationHumidity[4] = (uint8_t)((calibration->dig_H4 & 0x0F) |
                                             ((calibration->dig_H5 & 0x0F) << 4));
    mock->calibrationHumidity[5] = (uint8_t)(calibration->dig_H5 >> 4);
    mock->calibrationHumidity[6] = (uint8_t)calibration->dig_H6;
}

static void encode_sample(MockI2cType *mock, int32_t pressure,
                          int32_t temperature, int32_t humidity)
{
    mock->data[0] = (uint8_t)(pressure >> 12);
    mock->data[1] = (uint8_t)(pressure >> 4);
    mock->data[2] = (uint8_t)(pressure << 4);
    mock->data[3] = (uint8_t)(temperature >> 12);
    mock->data[4] = (uint8_t)(temperature >> 4);
    mock->data[5] = (uint8_t)(temperature << 4);
    mock->data[6] = (uint8_t)(humidity >> 8);
    mock->data[7] = (uint8_t)humidity;
}

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
    MockI2cType mock = {.chipId = 0x60U, .nextResult = MCAL_OK};
    encode_calibration(&mock, &calibration);
    encode_sample(&mock, 415148, 519888, 32257);
    const Mcal_I2cInterfaceType i2c = {
        .context = &mock, .writeRegister = write_register,
        .readRegister = read_register
    };
    Bme280_InstanceType first;
    Bme280_InstanceType second;
    Bme280_InstanceInit(&first, 0x76U, &calibration, i2c);
    Bme280_InstanceInit(&second, 0x77U, &calibration, i2c);
    assert(Bme280_Init(&first) == MCAL_OK);
    assert(first.calibration.dig_T1 == calibration.dig_T1);
    assert(first.health == BME280_HEALTH_INITIAL);

    assert(Bme280_MainFunction_High(&first, 1000) == MCAL_OK);
    assert(first.state == BME280_STATE_CHECK && mock.writes == 1U);
    assert(Bme280_MainFunction_High(&first, 2000) == MCAL_OK);
    assert(first.state == BME280_STATE_READ);
    assert(Bme280_MainFunction_High(&first, 3000) == MCAL_OK);
    assert(first.sequence == 1U && first.sampleTimeUs == 3000);
    assert(first.quality[0] == RTE_QUALITY_VALID);
    assert(first.sample.temperatureCentiDegC == 2508);
    assert(second.sequence == 0U);
    assert(mock.lastAddress == 0x76U && mock.lastRegister == 0xF7U);

    first.state = BME280_STATE_CHECK;
    mock.nextResult = MCAL_NACK;
    assert(Bme280_MainFunction_High(&first, 4000) == MCAL_NACK);
    assert(first.sequence == 1U && first.health == BME280_HEALTH_DEGRADED);
    return 0;
}
