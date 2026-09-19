#include <stdio.h>

#include "EcuM.h"
#include "Os_Wrapper.h"

#ifdef ESP_PLATFORM
#include "bme280.h"
#include "ClimateController.h"
#include "IoHwAb_Fan.h"
#include "Mcal_Port.h"
#include "Mcal_Pwm.h"
#include "Rte_Type.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_timer.h"
#endif

void EcuM_ContextInit(EcuM_ContextType *context, uint32_t bootLoopCount)
{
    *context = (EcuM_ContextType){
        .state = ECUM_STARTUP, .bootLoopCount = bootLoopCount
    };
}

int EcuM_Release(EcuM_ContextType *context, int64_t epochUs)
{
    if (context == 0 || context->gateReleased) {
        return 0;
    }
    context->startupEpochUs = epochUs;
    context->gateReleased = 1U;
    context->watchdogSubscribed = 0U;
    context->state = context->initFailures == 0U ? ECUM_RUN : ECUM_DEGRADED;
    return 1;
}

void EcuM_RecordInitFailure(EcuM_ContextType *context)
{
    context->initFailures++;
    if (context->gateReleased) {
        context->state = ECUM_DEGRADED;
    }
}

void EcuM_RecordDeadlineFault(EcuM_ContextType *context)
{
    context->deadlineFaults++;
    if (context->deadlineFaults >= 5U) {
        context->state = ECUM_SHUTDOWN;
        context->resetRequested = 1U;
        context->controlledResets++;
    }
}

void EcuM_RecordRunActivation(EcuM_ContextType *context)
{
    if (context == 0 || context->state != ECUM_RUN) {
        return;
    }
    context->runActivations++;
    if (context->runActivations >= 240U) {
        if (context->bootLoopCounter != 0) {
            *context->bootLoopCounter = 0U;
        }
        context->runActivations = 0U;
    }
}

int EcuM_CheckBootLoop(EcuM_ContextType *context, uint32_t resets,
                       uint32_t windowMs)
{
    context->bootLoopCount = resets;
    if (resets >= 5U && windowMs <= 300000U) {
        EcuM_EnterSafeHalt(context);
        return 1;
    }
    return 0;
}

void EcuM_EnterSafeHalt(EcuM_ContextType *context)
{
    context->state = ECUM_SAFE_HALT;
    context->watchdogSubscribed = 0U;
    context->resetRequested = 0U;
#ifdef ESP_PLATFORM
    Os_UnsubscribeWatchdog();
#endif
}

#ifdef ESP_PLATFORM
typedef struct {
    uint8_t chipId;
    uint8_t calibrationTp[24];
    uint8_t calibrationH1;
    uint8_t calibrationHumidity[7];
    uint8_t sample[8];
} EcuM_FakeI2cType;

typedef struct {
    EcuM_ContextType context;
    Mcal_I2cInterfaceType i2c;
    Bme280_InstanceType sensors[2];
    Rte_EnvironmentalSlotType samples[2];
    ClimateController_CtxType controller;
    IoHwAb_FanType fan;
    Mcal_PwmHandleType fanPwm;
    EcuM_FakeI2cType fake;
} EcuM_RuntimeType;

static void put_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void fake_prepare(EcuM_FakeI2cType *fake)
{
    const Bme280_CalibrationType calibration = {
        .dig_T1 = 27504, .dig_T2 = 26435, .dig_T3 = -1000,
        .dig_P1 = 36477, .dig_P2 = -10685, .dig_P3 = 3024,
        .dig_P4 = 2855, .dig_P5 = 140, .dig_P6 = -7,
        .dig_P7 = 15500, .dig_P8 = -14600, .dig_P9 = 6000,
        .dig_H1 = 75, .dig_H2 = 362, .dig_H3 = 0,
        .dig_H4 = 325, .dig_H5 = 50, .dig_H6 = 30
    };
    const int16_t tp[] = {
        (int16_t)calibration.dig_T1, calibration.dig_T2, calibration.dig_T3,
        (int16_t)calibration.dig_P1, calibration.dig_P2, calibration.dig_P3,
        calibration.dig_P4, calibration.dig_P5, calibration.dig_P6,
        calibration.dig_P7, calibration.dig_P8, calibration.dig_P9
    };
    for (uint16_t i = 0; i < 12U; ++i) {
        put_u16(&fake->calibrationTp[i * 2U], (uint16_t)tp[i]);
    }
    fake->chipId = 0x60U;
    fake->calibrationH1 = calibration.dig_H1;
    put_u16(&fake->calibrationHumidity[0], (uint16_t)calibration.dig_H2);
    fake->calibrationHumidity[2] = calibration.dig_H3;
    fake->calibrationHumidity[3] = (uint8_t)(calibration.dig_H4 >> 4);
    fake->calibrationHumidity[4] = (uint8_t)((calibration.dig_H4 & 0x0F) |
                                             ((calibration.dig_H5 & 0x0F) << 4));
    fake->calibrationHumidity[5] = (uint8_t)(calibration.dig_H5 >> 4);
    fake->calibrationHumidity[6] = (uint8_t)calibration.dig_H6;
    const int32_t pressure = 415148;
    const int32_t temperature = 519888;
    const int32_t humidity = 32257;
    fake->sample[0] = (uint8_t)(pressure >> 12);
    fake->sample[1] = (uint8_t)(pressure >> 4);
    fake->sample[2] = (uint8_t)(pressure << 4);
    fake->sample[3] = (uint8_t)(temperature >> 12);
    fake->sample[4] = (uint8_t)(temperature >> 4);
    fake->sample[5] = (uint8_t)(temperature << 4);
    fake->sample[6] = (uint8_t)(humidity >> 8);
    fake->sample[7] = (uint8_t)humidity;
}

static Mcal_ResultType fake_write(void *context, uint8_t address, uint8_t reg,
                                  const uint8_t *data, uint16_t length)
{
    (void)context;
    (void)address;
    (void)reg;
    (void)data;
    (void)length;
    return MCAL_OK;
}

static Mcal_ResultType fake_read(void *context, uint8_t address, uint8_t reg,
                                 uint8_t *data, uint16_t length)
{
    const EcuM_FakeI2cType *fake = context;
    (void)address;
    if (reg == 0xD0U && length == 1U) {
        data[0] = fake->chipId;
    } else if (reg == 0x88U && length == sizeof(fake->calibrationTp)) {
        for (uint16_t i = 0; i < length; ++i) data[i] = fake->calibrationTp[i];
    } else if (reg == 0xA1U && length == 1U) {
        data[0] = fake->calibrationH1;
    } else if (reg == 0xE1U && length == sizeof(fake->calibrationHumidity)) {
        for (uint16_t i = 0; i < length; ++i) data[i] = fake->calibrationHumidity[i];
    } else if (reg == 0xF3U && length == 1U) {
        data[0] = 0U;
    } else if (reg == 0xF7U && length == sizeof(fake->sample)) {
        for (uint16_t i = 0; i < length; ++i) data[i] = fake->sample[i];
    } else {
        return MCAL_INVALID_ARG;
    }
    return MCAL_OK;
}

static void report_fault(void *argument, uint32_t fault, int64_t value)
{
    EcuM_RuntimeType *runtime = argument;
    (void)value;
    if (fault == OS_RTF_DEADLINE) {
        EcuM_RecordDeadlineFault(&runtime->context);
    }
}

static const char *sensor_health(Bme280_HealthType health)
{
    if (health == BME280_HEALTH_READY) {
        return "READY";
    }
    if (health == BME280_HEALTH_DEGRADED) {
        return "DEGRADED";
    }
    return "INITIAL";
}

static void report_sensor(const Bme280_InstanceType *sensor, const char *name)
{
    printf("{\"instance\":\"%s\",\"health\":\"%s\",\"sequence\":%u,\"temperatureCentiDegC\":%ld}\n",
           name, sensor_health(sensor->health), (unsigned)sensor->sequence,
           (long)sensor->sample.temperatureCentiDegC);
}

static void run_t10(void *argument)
{
    EcuM_RuntimeType *runtime = argument;
    const int64_t nowUs = esp_timer_get_time();
    for (uint8_t i = 0; i < 2U; ++i) {
        const uint32_t sequence = runtime->sensors[i].sequence;
        const Mcal_ResultType result = Bme280_MainFunction_High(
            &runtime->sensors[i], nowUs);
        if (result != MCAL_OK && result != MCAL_BUSY) {
            EcuM_RecordInitFailure(&runtime->context);
        }
        if (runtime->sensors[i].sequence != sequence) {
            Rte_EnvironmentalDataType sample;
            Bme280_CopyEnvironmental(&runtime->sensors[i], &sample);
            Rte_EnvironmentalPublish(&runtime->samples[i], &sample);
            report_sensor(&runtime->sensors[i],
                          i == 0U ? "ambientSensor" : "enclosureSensor");
        }
    }
}

static void run_t100(void *argument)
{
    EcuM_RuntimeType *runtime = argument;
    Rte_EnvironmentalDataType sample;
    Rte_EnvironmentalRead(&runtime->samples[0], &sample);
    const float requested = ClimateController_Run(
        &runtime->controller, &sample, esp_timer_get_time());
    const int fresh = Rte_EnvironmentalIsFresh(&sample, esp_timer_get_time(), 50U);
    const float applied = IoHwAb_FanApply(
        &runtime->fan, requested,
        fresh && runtime->controller.outputValid != 0U);
    if (Mcal_Pwm_SetDuty(&runtime->fanPwm, (uint16_t)(applied * 1000.0f)) != MCAL_OK) {
        EcuM_RecordInitFailure(&runtime->context);
    }
}

static void run_t500(void *argument)
{
    EcuM_RuntimeType *runtime = argument;
    EcuM_RecordRunActivation(&runtime->context);
    if (runtime->context.initFailures != 0U) {
        runtime->context.state = ECUM_DEGRADED;
    }
}

void EcuM_Startup(void)
{
    static RTC_DATA_ATTR uint32_t bootLoopCounter;
    static EcuM_RuntimeType runtime;
    static Os_StackType t10Stack[2048];
    static Os_StackType t100Stack[2048];
    static Os_StackType t500Stack[2048];
    static Os_TaskConfigType t10 = {
        .name = "T10", .periodMs = 10U, .deadlineMs = 9U,
        .jitterToleranceMs = 1U, .priority = 5U,
        .runnable = run_t10, .fault = report_fault, .stack = t10Stack,
        .stackWords = 2048U
    };
    static Os_TaskConfigType t100 = {
        .name = "T100", .periodMs = 100U, .deadlineMs = 90U,
        .jitterToleranceMs = 1U, .priority = 4U,
        .runnable = run_t100, .fault = report_fault, .stack = t100Stack,
        .stackWords = 2048U
    };
    static Os_TaskConfigType t500 = {
        .name = "T500", .periodMs = 500U, .deadlineMs = 450U,
        .jitterToleranceMs = 1U, .priority = 3U,
        .runnable = run_t500, .fault = report_fault, .stack = t500Stack,
        .stackWords = 2048U
    };

    EcuM_ContextInit(&runtime.context, 0U);
    runtime.context.bootLoopCounter = &bootLoopCounter;
    if (bootLoopCounter >= 5U) {
        EcuM_EnterSafeHalt(&runtime.context);
        printf("{\"system\":\"SAFE_HALT\",\"reason\":\"BOOT_LOOP\",\"resetReason\":%d}\n",
               esp_reset_reason());
        return;
    }
    bootLoopCounter++;
    const Mcal_PortPinConfigType portConfig = {
        .pin = CONFIG_MERLIN_FAN_GPIO, .output = 1U, .initialLevel = 0U
    };
    if (Mcal_Port_Init(&portConfig, 1U) != MCAL_OK) {
        EcuM_RecordInitFailure(&runtime.context);
    }
    const Mcal_PwmConfigType pwmConfig = {
        .timer = 0, .channel = 0, .pin = CONFIG_MERLIN_FAN_GPIO,
        .frequencyHz = 1000U, .resolutionBits = 10U,
        .initialDutyPermille = 1000U
    };
    if (Mcal_Pwm_Init(&runtime.fanPwm, &pwmConfig) != MCAL_OK) {
        EcuM_RecordInitFailure(&runtime.context);
    }
    fake_prepare(&runtime.fake);
#if CONFIG_MERLIN_FAKE_SENSORS
    runtime.i2c = (Mcal_I2cInterfaceType){
        .context = &runtime.fake, .writeRegister = fake_write,
        .readRegister = fake_read
    };
#else
    static Mcal_I2cHandleType handle;
    const Mcal_I2cConfigType config = {
        .port = 0, .sdaPin = CONFIG_MERLIN_I2C_SDA_GPIO,
        .sclPin = CONFIG_MERLIN_I2C_SCL_GPIO, .frequencyHz = 100000U,
        .timeoutMs = 2U, .pullups = 1U
    };
    if (Mcal_I2c_Init(&handle, &config) != MCAL_OK) {
        EcuM_RecordInitFailure(&runtime.context);
    }
    runtime.i2c = Mcal_I2c_GetInterface(&handle);
#endif
    for (uint8_t i = 0; i < 2U; ++i) {
        Bme280_InstanceInit(&runtime.sensors[i], (uint8_t)(0x76U + i), 0,
                            runtime.i2c);
        if (Bme280_Init(&runtime.sensors[i]) != MCAL_OK) {
            EcuM_RecordInitFailure(&runtime.context);
        }
    }
    ClimateController_Init(&runtime.controller, 22.0f);
    IoHwAb_FanInit(&runtime.fan, 2U, 1.0f);
    t10.context = &runtime;
    t100.context = &runtime;
    t500.context = &runtime;
    const int tasksReady = Os_CreateStaticTask(&t10) &&
                           Os_CreateStaticTask(&t100) &&
                           Os_CreateStaticTask(&t500);
    if (!tasksReady) {
        EcuM_RecordInitFailure(&runtime.context);
        EcuM_EnterSafeHalt(&runtime.context);
        puts("{\"system\":\"SAFE_HALT\",\"reason\":\"TASK_INIT\"}");
        return;
    }
    (void)EcuM_Release(&runtime.context, esp_timer_get_time());
    (void)Os_ReleaseTask(&t10);
    (void)Os_ReleaseTask(&t100);
    (void)Os_ReleaseTask(&t500);
}

#else

void EcuM_Startup(void)
{
    puts("{\"instance\":\"ambientSensor\",\"health\":\"INITIAL\",\"sequence\":0}");
    puts("{\"instance\":\"enclosureSensor\",\"health\":\"INITIAL\",\"sequence\":0}");
}

#endif
