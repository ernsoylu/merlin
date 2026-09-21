/* EcuM_Startup for the ESP32 climate reference: its tasks, its simulated
 * sensor transport and its logging. Project-specific by design -- the
 * portable state machine it drives is in ecum.c. */
#include <stdio.h>

#include "EcuM.h"
#include "Log_Ring.h"
#include "Os_Wrapper.h"

#ifdef ESP_PLATFORM
#include "bme280.h"
#include "ClimateController.h"
#include "IoHwAb_Fan.h"
#include "Mcal_Mcu.h"
#include "Mcal_Port.h"
#include "Mcal_Pwm.h"
#include "Rte_Type.h"
#include "esp_attr.h"
#include "esp_timer.h"
#endif

#ifdef ESP_PLATFORM
typedef struct {
    uint8_t chipId;
    uint8_t calibrationTp[24];
    uint8_t calibrationH1;
    uint8_t calibrationHumidity[7];
    uint8_t sample[8];
    uint32_t sampleReads[2];
    uint32_t disconnectAttempts[2];
} EcuM_FakeI2cType;

enum {
    ECUM_LOG_SENSOR = 1U,
    ECUM_LOG_HEARTBEAT,
    ECUM_LOG_CONTROLLED_RESET
};

typedef struct {
    EcuM_ContextType context;
    Mcal_I2cInterfaceType i2c;
    Bme280_InstanceType sensors[2];
    Rte_EnvironmentalSlotType samples[2];
    ClimateController_CtxType controller;
    IoHwAb_FanType fan;
    Mcal_PwmHandleType fanPwm;
    EcuM_FakeI2cType fake;
    int64_t publishMinUs;
    int64_t publishMaxUs;
    uint32_t publishSamples;
    const Os_ReleaseStateType *t10Release;
    Log_RingType log;
    uint8_t fanHealthy;
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

static int fake_sensor_index(uint8_t address)
{
    if (address == 0x76U) {
        return 0;
    }
    return address == 0x77U ? 1 : -1;
}

static int fake_sensor_disconnected(const EcuM_FakeI2cType *fake,
                                    uint8_t address)
{
#if CONFIG_MERLIN_FAKE_SENSORS
#if CONFIG_MERLIN_FAKE_DISCONNECT_AFTER_SAMPLES > 0
    const int index = fake_sensor_index(address);
    if (index < 0 || CONFIG_MERLIN_FAKE_DISCONNECT_SENSOR != index ||
        fake->sampleReads[index] < CONFIG_MERLIN_FAKE_DISCONNECT_AFTER_SAMPLES) {
        return 0;
    }
#if CONFIG_MERLIN_FAKE_DISCONNECT_FOR_ACTIVATIONS > 0
    return fake->disconnectAttempts[index] <=
           CONFIG_MERLIN_FAKE_DISCONNECT_FOR_ACTIVATIONS;
#else
    return 1;
#endif
#else
    (void)fake;
    (void)address;
    return 0;
#endif
#else
    (void)fake;
    (void)address;
    return 0;
#endif
}

static Mcal_ResultType fake_write(void *context, uint8_t address, uint8_t reg,
                                  const uint8_t *data, uint16_t length)
{
    const EcuM_FakeI2cType *fake = context;
#if CONFIG_MERLIN_FAKE_DISCONNECT_AFTER_SAMPLES > 0
    EcuM_FakeI2cType *mutableFake = context;
    const int index = fake_sensor_index(address);
    if (reg == 0xF4U && index >= 0 &&
        fake->sampleReads[index] >= CONFIG_MERLIN_FAKE_DISCONNECT_AFTER_SAMPLES) {
        mutableFake->disconnectAttempts[index]++;
    }
#endif
    if (fake_sensor_disconnected(fake, address)) {
        return MCAL_NACK;
    }
    (void)reg;
    (void)data;
    (void)length;
    return MCAL_OK;
}

static Mcal_ResultType fake_recover(void *context)
{
    (void)context;
    return MCAL_OK;
}

static Mcal_ResultType fake_read(void *context, uint8_t address, uint8_t reg,
                                 uint8_t *data, uint16_t length)
{
    EcuM_FakeI2cType *fake = context;
    if (fake_sensor_disconnected(fake, address)) {
        return MCAL_NACK;
    }
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
        const int index = fake_sensor_index(address);
        if (index >= 0) {
            fake->sampleReads[index]++;
        }
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
        printf("{\"fault\":\"RTF-002\",\"deadlineFaults\":%u}\n",
               (unsigned)runtime->context.deadlineFaults);
        if (runtime->context.resetRequested) {
            /* Failsafe first: drive the actuator off before the reset takes effect. */
            const float failsafeDuty = IoHwAb_FanApply(&runtime->fan, 0.0f, 0);
            (void)Mcal_Pwm_SetDuty(&runtime->fanPwm, (uint16_t)(failsafeDuty * 1000.0f));
            (void)Log_TryPush(&runtime->log, (Log_RecordType){
                .code = ECUM_LOG_CONTROLLED_RESET,
                .argument = runtime->context.controlledResets
            });
            /* Let the low-priority drain emit the forensic record before reboot. */
            vTaskDelay(pdMS_TO_TICKS(20U));
            (void)Mcal_Mcu_Reset();
        }
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

static const char *ecum_state_name(EcuM_StateType state)
{
    switch (state) {
    case ECUM_RUN: return "RUN";
    case ECUM_DEGRADED: return "DEGRADED";
    case ECUM_SHUTDOWN: return "SHUTDOWN";
    case ECUM_SAFE_HALT: return "SAFE_HALT";
    default: return "STARTUP";
    }
}

static void report_sensor(EcuM_RuntimeType *runtime,
                          const Bme280_InstanceType *sensor, const char *name)
{
    (void)Log_TryPush(&runtime->log, (Log_RecordType){
        .code = ECUM_LOG_SENSOR,
        .argument = sensor->sequence,
        .timestampUs = sensor->sampleTimeUs,
        .label = name,
        .payload = sensor
    });
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
            const int64_t lockStartUs = esp_timer_get_time();
            Rte_EnvironmentalPublish(&runtime->samples[i], &sample);
            const int64_t lockUs = esp_timer_get_time() - lockStartUs;
            if (runtime->publishSamples == 0U || lockUs < runtime->publishMinUs) {
                runtime->publishMinUs = lockUs;
            }
            if (lockUs > runtime->publishMaxUs) {
                runtime->publishMaxUs = lockUs;
            }
            runtime->publishSamples++;
            report_sensor(runtime, &runtime->sensors[i],
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
        runtime->fanHealthy = 0U;
        EcuM_RecordInitFailure(&runtime->context);
    }
}

static void update_runtime_state(EcuM_RuntimeType *runtime)
{
    if (runtime->context.state == ECUM_SHUTDOWN ||
        runtime->context.state == ECUM_SAFE_HALT) {
        return;
    }
    runtime->context.state = runtime->fanHealthy != 0U &&
                             runtime->sensors[0].health == BME280_HEALTH_READY &&
                             runtime->sensors[1].health == BME280_HEALTH_READY
        ? ECUM_RUN : ECUM_DEGRADED;
}

static void run_t500(void *argument)
{
    EcuM_RuntimeType *runtime = argument;
#if CONFIG_MERLIN_INJECT_SLOW_T500 > 0
    static uint32_t s_injectRemaining = CONFIG_MERLIN_INJECT_SLOW_T500;
    if (s_injectRemaining > 0U) {
        s_injectRemaining--;
        vTaskDelay(pdMS_TO_TICKS(1200U));
    }
#endif
    EcuM_RecordRunActivation(&runtime->context);
    if (runtime->context.initFailures != 0U) {
        update_runtime_state(runtime);
    }
    /* Sensor records are event-driven, so retain one periodic state snapshot. */
    static uint16_t s_heartbeatDivider;
    if (++s_heartbeatDivider < 200U) {
        return;
    }
    s_heartbeatDivider = 0U;
    (void)Log_TryPush(&runtime->log, (Log_RecordType){
        .code = ECUM_LOG_HEARTBEAT,
        .payload = runtime
    });
}

static void log_drain_task(void *argument)
{
    EcuM_RuntimeType *runtime = argument;
    Log_RecordType record;
    for (;;) {
        while (Log_TryPop(&runtime->log, &record)) {
            if (record.code == ECUM_LOG_SENSOR) {
                const Bme280_InstanceType *sensor = record.payload;
                printf("{\"instance\":\"%s\",\"health\":\"%s\",\"sequence\":%u,"
                       "\"temperatureCentiDegC\":%ld}\n",
                       record.label, sensor_health(sensor->health),
                       (unsigned)record.argument,
                       (long)sensor->sample.temperatureCentiDegC);
            } else if (record.code == ECUM_LOG_HEARTBEAT) {
                const EcuM_RuntimeType *state = record.payload;
                Rte_EnvironmentalDataType sample;
                Rte_EnvironmentalRead(&state->samples[0], &sample);
                printf("{\"heartbeat\":\"%s\",\"ambientSensor\":\"%s\","
                       "\"enclosureSensor\":\"%s\",\"initFailures\":%u,"
                       "\"ambientRecoveryCount\":%u,\"enclosureRecoveryCount\":%u,"
                       "\"ambientSampleFresh\":%u,\"fanDutyPermille\":%u,"
                       "\"deadlineFaults\":%u,\"publishLockMinUs\":%lld,"
                       "\"publishLockMaxUs\":%lld,\"publishSamples\":%u,"
                       "\"t10JitterMinTicks\":%lld,\"t10JitterMaxTicks\":%lld,"
                       "\"t10JitterSamples\":%u,\"t10WakeCount\":%u,"
                       "\"t10SkippedActivations\":%u}\n",
                       ecum_state_name(state->context.state),
                       sensor_health(state->sensors[0].health),
                       sensor_health(state->sensors[1].health),
                       (unsigned)state->context.initFailures,
                       (unsigned)state->sensors[0].recoveryCount,
                       (unsigned)state->sensors[1].recoveryCount,
                       (unsigned)Rte_EnvironmentalIsFresh(
                           &sample, esp_timer_get_time(), 50U),
                       (unsigned)(state->fan.appliedDuty * 1000.0f),
                       (unsigned)state->context.deadlineFaults,
                       (long long)state->publishMinUs,
                       (long long)state->publishMaxUs,
                       (unsigned)state->publishSamples,
                       (long long)state->t10Release->jitterMinTicks,
                       (long long)state->t10Release->jitterMaxTicks,
                       (unsigned)state->t10Release->jitterSamples,
                       (unsigned)state->t10Release->wakeCount,
                       (unsigned)state->t10Release->skippedActivations);
            } else if (record.code == ECUM_LOG_CONTROLLED_RESET) {
                printf("{\"system\":\"CONTROLLED_RESET\",\"reason\":"
                       "\"DEADLINE_FAULTS\",\"count\":%u}\n",
                       (unsigned)record.argument);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}

void EcuM_Startup(void)
{
    /* RTC_DATA_ATTR only survives deep sleep; RTC_NOINIT_ATTR is the one that
     * survives esp_restart()/watchdog/brownout resets, which is what a
     * boot-loop counter needs. Its content is undefined on a true power-on,
     * so an ESP_RST_POWERON/BROWNOUT reset (or window expiry) re-arms it. */
    static RTC_NOINIT_ATTR EcuM_BootLoopType bootLoop;
    static EcuM_RuntimeType runtime;
    static Os_StackType t10Stack[2048];
    static Os_StackType t100Stack[2048];
    static Os_StackType t500Stack[2048];
    static StackType_t logStack[2048];
    static StaticTask_t logTaskStorage;
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
    Log_RingInit(&runtime.log);
    Mcal_McuResetReasonType resetReason = MCAL_MCU_RESET_UNKNOWN;
    (void)Mcal_Mcu_GetResetReason(&resetReason);
    if (EcuM_EvaluateBootLoop(&runtime.context, &bootLoop, resetReason,
                              esp_timer_get_time())) {
        printf("{\"system\":\"SAFE_HALT\",\"reason\":\"BOOT_LOOP\",\"resetReason\":%d}\n",
               (int)resetReason);
        return;
    }
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
    runtime.fanHealthy = Mcal_Pwm_Init(&runtime.fanPwm, &pwmConfig) == MCAL_OK;
    if (runtime.fanHealthy == 0U) {
        EcuM_RecordInitFailure(&runtime.context);
    }
    fake_prepare(&runtime.fake);
#if CONFIG_MERLIN_FAKE_SENSORS
    runtime.i2c = (Mcal_I2cInterfaceType){
        .context = &runtime.fake, .writeRegister = fake_write,
        .readRegister = fake_read, .recover = fake_recover
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
    runtime.t10Release = &t10.release;
    t10.context = &runtime;
    t100.context = &runtime;
    t500.context = &runtime;
    const int logReady = xTaskCreateStatic(log_drain_task, "LogDrain",
                                           2048U, &runtime, 1U, logStack,
                                           &logTaskStorage) != NULL;
    const int tasksReady = logReady && Os_CreateStaticTask(&t10) &&
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
