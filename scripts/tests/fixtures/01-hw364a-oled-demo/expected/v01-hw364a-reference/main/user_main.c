#include <stdio.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

#include "DisplayDemo.h"
#include "EcuM.h"
#include "Mcal_Gpt.h"
#include "Mcal_Mcu.h"
#include "Mcal_I2c.h"
#include "Mcal_Wdg.h"
#include "Mcal_Wlan.h"
#include "Os_Wrapper.h"
#include "Hm_Debounce.h"
#include "sdkconfig.h"
#include "ssd1306_frame.h"

#define HW364A_SDA_GPIO 14
#define HW364A_SCL_GPIO 12
#define HW364A_I2C_PORT I2C_NUM_0
#define HW364A_OLED_ADDRESS 0x3CU
#define HW364A_TASK_PERIOD_MS 600U
#define HW364A_TASK_DEADLINE_MS 550U
#define HW364A_TASK_JITTER_MS 50U
#define HW364A_GOOD_FRAMES_TO_CLEAR 5U
/* SSD1306 pixel-data control byte, matching ssd1306_frame.c's
 * SSD1306_CONTROL_DATA; used to isolate per-chunk transfer timing (TST-OLED-03)
 * from the one-shot command burst issued during Ssd1306_Initialize. */
#define HW364A_SSD1306_CONTROL_DATA 0x40U

#ifdef HW364A_INJECT_INIT_FAILURE
/* TST-OLED-05: no device answers this address on the HW-364A bus, so init
 * deterministically NACKs without touching the soldered panel. */
#define HW364A_INIT_ADDRESS 0x10U
#else
#define HW364A_INIT_ADDRESS HW364A_OLED_ADDRESS
#endif

static esp_err_t last_i2c_error;
static uint32_t s_chunkCount;
static int64_t s_chunkTimeTotalUs;
static int64_t s_chunkTimeMaxUs;
static uint32_t s_injectFailuresRemaining;

static Ssd1306_InstanceType s_display;
static DisplayDemo_CtxType s_demo;
static Mcal_I2cHandleType s_i2c;
static Mcal_WdgHandleType s_wdg;
static Hm_RuntimeType s_hm;
static int s_hmSequenceActive;
static TaskHandle_t s_displayTaskHandle;
#if CONFIG_MERLIN_ENABLE_WLAN
static Mcal_WlanHandleType s_wlan;
#endif

static int64_t time_us(void)
{
    int64_t value = 0;
    (void)Mcal_Gpt_GetTimeUs(&value);
    return value;
}

static Mcal_ResultType map_error(esp_err_t error)
{
    if (error == ESP_OK) {
        return MCAL_OK;
    }
    if (error == ESP_ERR_TIMEOUT) {
        return MCAL_TIMEOUT;
    }
    if (error == ESP_FAIL) {
        return MCAL_NACK;
    }
    return MCAL_HW_FAIL;
}

static Mcal_ResultType transfer(Mcal_I2cHandleType *handle, uint8_t address,
                                uint8_t control, const uint8_t *data,
                                uint16_t length)
{
    i2c_cmd_handle_t command = i2c_cmd_link_create();
    if (command == 0) {
        return MCAL_HW_FAIL;
    }
    i2c_master_start(command);
    i2c_master_write_byte(command, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(command, control, true);
    for (uint16_t i = 0U; i < length; ++i) {
        i2c_master_write_byte(command, data[i], true);
    }
    i2c_master_stop(command);
    const esp_err_t error = i2c_master_cmd_begin(
        (i2c_port_t)handle->port, command,
        (handle->timeoutMs + portTICK_PERIOD_MS - 1U) / portTICK_PERIOD_MS);
    last_i2c_error = error;
    i2c_cmd_link_delete(command);
    return map_error(error);
}

static Mcal_ResultType write_register(void *context, uint8_t address,
                                      uint8_t reg, const uint8_t *data,
                                      uint16_t length)
{
    Mcal_I2cHandleType *handle = context;
    if (handle == 0 || !handle->initialized || data == 0 || length == 0U) {
        return MCAL_INVALID_ARG;
    }
    const int64_t startUs = time_us();
    Mcal_ResultType result;
    if (s_injectFailuresRemaining > 0U) {
        /* TST-OLED-04: software-injected NACK burst, no electrical fault
         * needed. Distinct from a real bus fault, and recorded as such. */
        s_injectFailuresRemaining--;
        last_i2c_error = ESP_FAIL;
        result = MCAL_NACK;
    } else {
        result = transfer(handle, address, reg, data, length);
    }
    const int64_t elapsedUs = time_us() - startUs;
    if (reg == HW364A_SSD1306_CONTROL_DATA) {
        s_chunkCount++;
        s_chunkTimeTotalUs += elapsedUs;
        if (elapsedUs > s_chunkTimeMaxUs) {
            s_chunkTimeMaxUs = elapsedUs;
        }
    }
    return result;
}

static Mcal_ResultType recover(void *context)
{
    Mcal_I2cHandleType *handle = context;
    if (handle == 0 || !handle->initialized) {
        return MCAL_INVALID_ARG;
    }
    (void)i2c_driver_delete((i2c_port_t)handle->port);
    (void)gpio_set_direction((gpio_num_t)handle->sclPin, GPIO_MODE_OUTPUT_OD);
    (void)gpio_set_direction((gpio_num_t)handle->sdaPin, GPIO_MODE_INPUT);
    for (uint8_t pulse = 0U; pulse < 9U &&
         gpio_get_level((gpio_num_t)handle->sdaPin) == 0; ++pulse) {
        (void)gpio_set_level((gpio_num_t)handle->sclPin, 0U);
        ets_delay_us(5U);
        (void)gpio_set_level((gpio_num_t)handle->sclPin, 1U);
        ets_delay_us(5U);
    }
    return Mcal_I2c_Init(handle, &(Mcal_I2cConfigType){
        .port = handle->port, .sdaPin = handle->sdaPin,
        .sclPin = handle->sclPin, .frequencyHz = handle->frequencyHz,
        .timeoutMs = handle->timeoutMs, .pullups = handle->pullups
    });
}

Mcal_ResultType Mcal_I2c_Init(Mcal_I2cHandleType *handle,
                              const Mcal_I2cConfigType *config)
{
    if (handle == 0 || config == 0 || config->frequencyHz == 0U ||
        config->timeoutMs == 0U || config->port < 0) {
        return MCAL_INVALID_ARG;
    }
    const i2c_config_t native = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sdaPin,
        .sda_pullup_en = config->pullups ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .scl_io_num = config->sclPin,
        .scl_pullup_en = config->pullups ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .clk_stretch_tick = 300
    };
    /* ESP8266 RTOS SDK v3.4 requires install before parameter configuration. */
    esp_err_t error = i2c_driver_install((i2c_port_t)config->port,
                                         I2C_MODE_MASTER);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        return map_error(error);
    }
    error = i2c_param_config((i2c_port_t)config->port, &native);
    if (error != ESP_OK) {
        return map_error(error);
    }
    /* ESP8266 RTOS SDK v3.4's I2C driver NACKs the first transaction after
     * i2c_param_config regardless of target address; every transaction after
     * that succeeds. Absorb the one-time failure here so callers never see
     * it. Verified across 6 consecutive resets on HW-364A hardware. */
    i2c_cmd_handle_t warmup = i2c_cmd_link_create();
    if (warmup != 0) {
        i2c_master_start(warmup);
        i2c_master_write_byte(warmup, (0x00U << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(warmup);
        (void)i2c_master_cmd_begin((i2c_port_t)config->port, warmup,
                                   50U / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(warmup);
    }
    *handle = (Mcal_I2cHandleType){
        .port = config->port, .timeoutMs = config->timeoutMs,
        .sdaPin = config->sdaPin, .sclPin = config->sclPin,
        .frequencyHz = config->frequencyHz, .pullups = config->pullups,
        .initialized = 1U
    };
    return MCAL_OK;
}

Mcal_I2cInterfaceType Mcal_I2c_GetInterface(Mcal_I2cHandleType *handle)
{
    return (Mcal_I2cInterfaceType){
        .context = handle, .writeRegister = write_register,
        .recover = recover
    };
}

static void print_status(const Ssd1306_InstanceType *display)
{
    printf("{\"display\":\"onboardOled\",\"health\":%u,\"completed\":%u,\"failures\":%u,\"recoveries\":%u}\n",
           (unsigned)display->health, (unsigned)display->lastCompletedSequence,
           (unsigned)display->transferFailures,
           (unsigned)display->recoveryCount);
}

static void hm_observe_sequence(void)
{
    const int active = Hm_RuntimeObserveSequence(
        &s_hm, s_display.lastCompletedSequence);
    if (active != s_hmSequenceActive) {
        s_hmSequenceActive = active;
        printf("{\"hm\":\"sequence\",\"active\":%d,\"failed\":%u}\n",
               active, (unsigned)s_hm.sequence.failed);
    }
}

/* Direct-to-task notification startup gate (PROJECT_DEFINITION §11): the
 * display task blocks immediately on creation and only proceeds once
 * app_main has finished bus/panel init and releases it. Watchdog subscription
 * happens after the gate opens, and is fed exactly once per activation. The
 * Mcal_Wdg adapter records that this SDK has no unsubscribe API, so SAFE_HALT
 * relies on parking the task rather than an explicit stop call. */
static void display_task(void *argument)
{
    uint32_t *bootLoopCounter = argument;
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (Mcal_Wdg_Init(&s_wdg, &(Mcal_WdgConfigType){.timeoutMs = 15000U}) != MCAL_OK) {
        puts("{\"system\":\"SAFE_HALT\",\"reason\":\"WDT_INIT\"}");
        return;
    }

    const TickType_t periodTicks = pdMS_TO_TICKS(HW364A_TASK_PERIOD_MS);
    TickType_t lastWake = xTaskGetTickCount();
    Os_ReleaseStateType release;
    Os_ReleaseInit(&release, (int64_t)lastWake + periodTicks);
    Os_ReleaseConfigure(&release, periodTicks, pdMS_TO_TICKS(HW364A_TASK_JITTER_MS));

    uint32_t activation = 0U;
    uint32_t goodFrames = 0U;
    for (;;) {
        vTaskDelayUntil(&lastWake, periodTicks);
        const TickType_t actualWake = xTaskGetTickCount();
        activation++;
        if (Os_ReleaseSkip(&release, actualWake, periodTicks)) {
            Hm_RuntimeRecordRtf(&s_hm, 3U);
            printf("{\"rtf\":\"RTF-003-SKIP\",\"activation\":%u,\"skipped\":%u}\n",
                   activation, (unsigned)release.skippedActivations);
            (void)Mcal_Wdg_Feed(&s_wdg);
            continue;
        }
        if (Os_ReleaseRecordWake(&release, actualWake)) {
            Hm_RuntimeRecordRtf(&s_hm, 3U);
            printf("{\"rtf\":\"RTF-LATE\",\"activation\":%u,\"late\":%u}\n",
                   activation, (unsigned)release.lateActivations);
        }

        const int64_t startUs = time_us();
        DisplayDemo_Run(&s_demo);
        const Rte_MonochromeFrameType *frame = DisplayDemo_GetFrame(&s_demo);
        const Ssd1306_FrameViewType view = {
            .width = frame->width, .height = frame->height,
            .sequence = frame->sequence, .pixels = frame->pixels,
            .pixelBytes = RTE_MONOCHROME_FRAME_BYTES
        };
        if (Ssd1306_SubmitFrame(&s_display, &view) != MCAL_OK) {
            Hm_RuntimeRecordRtf(&s_hm, 6U);
            print_status(&s_display);
        }
        while (s_display.activeValid) {
            const uint32_t failures = s_display.transferFailures;
            (void)Ssd1306_TransferChunk(&s_display);
            if (s_display.transferFailures != failures) {
                Hm_RuntimeRecordRtf(&s_hm, 6U);
            }
        }

#ifdef HW364A_INJECT_SLOW_ACTIVATION
        /* TST-OLED-08: one deliberately slow activation, above the 550 ms
         * software deadline but far below the 15 s TWDT timeout, so the
         * fault is observable while the watchdog stays silent. The overrun
         * also pushes the next vTaskDelayUntil() boundary into the past,
         * which is what exercises the skip/re-anchor path above. */
        if (activation == 6U) {
            vTaskDelay(pdMS_TO_TICKS(1300U));
        }
#endif

        const int64_t elapsedUs = time_us() - startUs;
        if (Os_DeadlineCheck(&release, elapsedUs,
                             (int64_t)HW364A_TASK_DEADLINE_MS * 1000)) {
            Hm_RuntimeRecordRtf(&s_hm, 2U);
            /* CONFIG_NEWLIB_NANO_FORMAT drops %lld; elapsedUs fits in 32 bits
             * for any activation on this reference (worst case ~1.3e6 us). */
            printf("{\"rtf\":\"RTF-002-DEADLINE\",\"activation\":%u,\"elapsedUs\":%ld}\n",
                   activation, (long)elapsedUs);
        }
        hm_observe_sequence();
        /* Exactly one feed for each completed activation. */
        (void)Mcal_Wdg_Feed(&s_wdg);

        print_status(&s_display);
        if (s_display.health == SSD1306_HEALTH_READY) {
            goodFrames++;
            if (bootLoopCounter != 0 && goodFrames >= HW364A_GOOD_FRAMES_TO_CLEAR) {
                *bootLoopCounter = 0U;
                goodFrames = 0U;
            }
        } else {
            goodFrames = 0U;
        }

#ifdef HW364A_INJECT_AUTO_RESET
        /* TST-OLED-08 boot-loop path: force a software reset well before
         * goodFrames could clear the counter, repeatedly, with no external
         * pin toggle involved -- this is the actual scenario RTC_DATA_ATTR
         * is meant to survive (soft reset / esp_restart), unlike an external
         * RST-pin pulse. */
        if (activation == 2U) {
            printf("{\"fault_inject\":\"AUTO_RESET\",\"bootLoopCounter\":%u}\n",
                   (unsigned)*bootLoopCounter);
            (void)Mcal_Mcu_Reset();
        }
#endif

#ifdef HW364A_INJECT_FAILURE_BURST
        if (activation == 3U) {
            s_injectFailuresRemaining = 5U;
            puts("{\"fault_inject\":\"NACK_BURST_START\",\"count\":5}");
        }
#endif

        if (s_chunkCount > 0U && (activation % 5U) == 0U) {
            printf("{\"chunk_us_avg\":%ld,\"chunk_us_max\":%ld,\"chunks_total\":%u,"
                   "\"heap_free\":%u,\"stack_hwm_words\":%u}\n",
                   (long)(s_chunkTimeTotalUs / (int64_t)s_chunkCount),
                   (long)s_chunkTimeMaxUs, (unsigned)s_chunkCount,
                   (unsigned)esp_get_free_heap_size(),
                   (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }
    }
}

void app_main(void)
{
    /* RTC_NOINIT_ATTR is the ESP8266 SDK's reset-retained section; RTC_DATA_ATTR
     * is initialized from the image and therefore cannot count boot loops. */
    static RTC_NOINIT_ATTR EcuM_BootLoopType bootLoop;
    static EcuM_ContextType ecum;
    Mcal_McuResetReasonType resetReason = MCAL_MCU_RESET_UNKNOWN;
    int64_t nowUs = 0;

    (void)Mcal_Mcu_GetResetReason(&resetReason);
    (void)Mcal_Gpt_GetTimeUs(&nowUs);
    EcuM_ContextInit(&ecum, 0U);
    if (EcuM_EvaluateBootLoop(&ecum, &bootLoop, resetReason, nowUs)) {
        printf("{\"system\":\"SAFE_HALT\",\"reason\":\"BOOT_LOOP\",\"resets\":%u,\"resetReason\":%d}\n",
               (unsigned)bootLoop.resets, (int)resetReason);
        return;
    }
    printf("{\"boot\":\"start\",\"bootLoopCounter\":%u,\"resetReason\":%d}\n",
           (unsigned)bootLoop.resets, (int)resetReason);

    /* This SDK build has configSUPPORT_STATIC_ALLOCATION disabled, so task
     * creation here is a one-time heap allocation at boot -- not the
     * steady-state control-path churn REQ-RUN-003 targets, which the
     * TST-OLED-07 heap trace below covers separately. */
    if (xTaskCreate(display_task, "display", 2048, &bootLoop.resets, 5,
                    &s_displayTaskHandle) != pdPASS) {
        puts("{\"system\":\"SAFE_HALT\",\"reason\":\"TASK_INIT\"}");
        return;
    }

    const Mcal_I2cConfigType config = {
        .port = HW364A_I2C_PORT, .sdaPin = HW364A_SDA_GPIO,
        .sclPin = HW364A_SCL_GPIO, .frequencyHz = 100000U,
        .timeoutMs = 1000U, .pullups = 1U
    };
    const Mcal_ResultType i2cResult = Mcal_I2c_Init(&s_i2c, &config);
    printf("hw364a_i2c_init result=%u\n", (unsigned)i2cResult);
    if (i2cResult != MCAL_OK) {
        puts("{\"system\":\"SAFE_HALT\",\"reason\":\"I2C_INIT\"}");
        return;
    }
    vTaskDelay(100U / portTICK_PERIOD_MS);
    Ssd1306_InstanceInit(&s_display, HW364A_INIT_ADDRESS,
                         Mcal_I2c_GetInterface(&s_i2c));
    const Mcal_ResultType displayResult = Ssd1306_Initialize(&s_display);
    printf("hw364a_oled_init result=%u native=%d\n", (unsigned)displayResult,
           (int)last_i2c_error);
    if (displayResult != MCAL_OK) {
        puts("{\"display\":\"onboardOled\",\"health\":\"DEGRADED\"}");
        /* No boot-loop counter clear, no task release: the display task
         * stays parked on its notification forever, so no frame/ready
         * report is ever emitted for a failed init. */
        return;
    }
#if CONFIG_MERLIN_ENABLE_WLAN
    const Mcal_ResultType wlanInit = Mcal_Wlan_Init(&s_wlan);
    const Mcal_ResultType wlanStart = wlanInit == MCAL_OK
        ? Mcal_Wlan_Start(&s_wlan) : wlanInit;
    printf("{\"wlan\":\"lifecycle\",\"init\":%u,\"start\":%u}\n",
           (unsigned)wlanInit, (unsigned)wlanStart);
    if (wlanInit != MCAL_OK) {
        printf("{\"rtf\":\"RTF-005-WLAN\",\"stage\":\"init\","
               "\"result\":%u}\n", (unsigned)wlanInit);
    } else if (wlanStart != MCAL_OK) {
        printf("{\"rtf\":\"RTF-005-WLAN\",\"stage\":\"start\","
               "\"result\":%u}\n", (unsigned)wlanStart);
    }
#endif
    DisplayDemo_Init(&s_demo);
    Hm_RuntimeInit(&s_hm, 2U, 1U);
    xTaskNotifyGive(s_displayTaskHandle);
}
