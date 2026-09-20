#ifndef RTE_TYPE_H
#define RTE_TYPE_H

#include <stdint.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#endif

typedef enum {
    RTE_QUALITY_INITIAL = 0,
    RTE_QUALITY_VALID,
    RTE_QUALITY_INVALID
} Rte_QualityType;

typedef struct {
    float temperatureDegC;
    float humidityPercent;
    float pressurePa;
    Rte_QualityType quality[3];
    int64_t sampleTimeUs;
    uint32_t sequence;
} Rte_EnvironmentalDataType;

typedef struct {
    Rte_EnvironmentalDataType value;
} Rte_EnvironmentalSlotType;

#define RTE_XCORE_READ_RETRIES 3U

typedef struct {
    uint32_t version;
    Rte_EnvironmentalDataType value;
} Rte_EnvironmentalXcoreSlotType;

#define RTE_MONOCHROME_FRAME_WIDTH 128U
#define RTE_MONOCHROME_FRAME_HEIGHT 64U
#define RTE_MONOCHROME_FRAME_BYTES \
    (RTE_MONOCHROME_FRAME_WIDTH * RTE_MONOCHROME_FRAME_HEIGHT / 8U)

typedef enum {
    RTE_DISPLAY_UNINIT = 0,
    RTE_DISPLAY_READY,
    RTE_DISPLAY_DEGRADED
} Rte_DisplayHealthType;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t sequence;
    Rte_QualityType quality;
    uint8_t pixels[RTE_MONOCHROME_FRAME_BYTES];
} Rte_MonochromeFrameType;

typedef struct {
    Rte_DisplayHealthType health;
    uint32_t lastCompletedSequence;
    uint32_t transferFailures;
    uint32_t recoveryCount;
} Rte_DisplayHealthReportType;

#define RTE_EVENT_QUEUE_CAPACITY 4U

typedef enum {
    RTE_EVENT_DROP_NEWEST = 0,
    RTE_EVENT_DROP_OLDEST
} Rte_EventFullPolicyType;

typedef enum {
    RTE_EVENT_OK = 0,
    RTE_EVENT_INVALID,
    RTE_EVENT_FULL
} Rte_EventResultType;

typedef struct {
    uint8_t burst;
    uint8_t serviceRate;
    Rte_EventFullPolicyType fullPolicy;
} Rte_EventQueueConfigType;

typedef struct {
    Rte_EventQueueConfigType config;
    uint32_t storage[RTE_EVENT_QUEUE_CAPACITY];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint32_t dropped;
#ifdef ESP_PLATFORM
    StaticQueue_t native;
    QueueHandle_t handle;
#endif
} Rte_EventQueueType;

typedef void (*Rte_EventHandlerFn)(uint32_t event, void *context);

void Rte_EnvironmentalPublish(Rte_EnvironmentalSlotType *slot,
                              const Rte_EnvironmentalDataType *value);
void Rte_EnvironmentalRead(const Rte_EnvironmentalSlotType *slot,
                           Rte_EnvironmentalDataType *out);
int Rte_EnvironmentalIsFresh(const Rte_EnvironmentalDataType *value,
                             int64_t nowUs, uint32_t maxAgeMs);
void Rte_EnvironmentalXcorePublish(Rte_EnvironmentalXcoreSlotType *slot,
                                   const Rte_EnvironmentalDataType *value);
int Rte_EnvironmentalXcoreRead(const Rte_EnvironmentalXcoreSlotType *slot,
                               Rte_EnvironmentalDataType *out);
Rte_EventResultType Rte_EventQueueInit(Rte_EventQueueType *queue,
                                       const Rte_EventQueueConfigType *config);
Rte_EventResultType Rte_EventQueuePush(Rte_EventQueueType *queue,
                                       uint32_t event);
Rte_EventResultType Rte_EventQueuePop(Rte_EventQueueType *queue,
                                      uint32_t *event);
uint8_t Rte_EventQueuePending(const Rte_EventQueueType *queue);
uint8_t Rte_EventQueueService(Rte_EventQueueType *queue,
                              Rte_EventHandlerFn handler, void *context);

#endif
