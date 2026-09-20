#include "Rte_Sample.h"
#include "Rte_Type.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
/* ponytail: one global lock; split per port only if measured contention matters. */
static portMUX_TYPE sampleLock = portMUX_INITIALIZER_UNLOCKED;
#define SAMPLE_LOCK() portENTER_CRITICAL(&sampleLock)
#define SAMPLE_UNLOCK() portEXIT_CRITICAL(&sampleLock)
#else
#define SAMPLE_LOCK()
#define SAMPLE_UNLOCK()
#endif

#ifdef ESP_PLATFORM
#define RTE_XCORE_MEMW() __asm__ volatile("memw" ::: "memory")
#else
#define RTE_XCORE_MEMW() __asm__ volatile("" ::: "memory")
#endif

void Rte_SamplePublish(Rte_SampleType *slot, int32_t value,
                       Rte_SampleQualityType quality, int64_t sampleTimeUs)
{
    SAMPLE_LOCK();
    slot->value = value;
    slot->quality = quality;
    slot->sampleTimeUs = sampleTimeUs;
    slot->sequence++;
    SAMPLE_UNLOCK();
}

void Rte_SampleRepublish(const Rte_SampleType *slot)
{
    /* Cached data keeps its acquisition time and sequence. */
    (void)slot;
}

void Rte_SampleRead(const Rte_SampleType *slot, Rte_SampleType *out)
{
    SAMPLE_LOCK();
    *out = *slot;
    SAMPLE_UNLOCK();
}

int Rte_SampleIsFresh(const Rte_SampleType *sample, int64_t nowUs, uint32_t maxAgeMs)
{
    return sample->quality == RTE_SAMPLE_VALID &&
           nowUs >= sample->sampleTimeUs &&
           nowUs - sample->sampleTimeUs <= (int64_t)maxAgeMs * 1000;
}

void Rte_EnvironmentalPublish(Rte_EnvironmentalSlotType *slot,
                              const Rte_EnvironmentalDataType *value)
{
    /* sequence and sampleTimeUs identify the acquisition, so the slot carries
     * them as the provider set them. Incrementing either here would put the
     * slot one ahead of the sample it holds, and would make republishing a
     * cached reading look like a new one. */
    SAMPLE_LOCK();
    slot->value = *value;
    SAMPLE_UNLOCK();
}

void Rte_EnvironmentalRead(const Rte_EnvironmentalSlotType *slot,
                           Rte_EnvironmentalDataType *out)
{
    SAMPLE_LOCK();
    *out = slot->value;
    SAMPLE_UNLOCK();
}

int Rte_EnvironmentalIsFresh(const Rte_EnvironmentalDataType *value,
                             int64_t nowUs, uint32_t maxAgeMs)
{
    if (value == 0 || nowUs < value->sampleTimeUs ||
        nowUs - value->sampleTimeUs > (int64_t)maxAgeMs * 1000) {
        return 0;
    }
    return value->quality[0] == RTE_QUALITY_VALID &&
           value->quality[1] == RTE_QUALITY_VALID &&
           value->quality[2] == RTE_QUALITY_VALID;
}

void Rte_EnvironmentalXcorePublish(Rte_EnvironmentalXcoreSlotType *slot,
                                   const Rte_EnvironmentalDataType *value)
{
    if (slot == 0 || value == 0) {
        return;
    }
    /* ponytail: one writer per slot; serialize writers if that changes. */
    const uint32_t version = __atomic_load_n(&slot->version, __ATOMIC_RELAXED) & ~1U;
    __atomic_store_n(&slot->version, version + 1U, __ATOMIC_RELEASE);
    RTE_XCORE_MEMW();
    slot->value = *value;
    RTE_XCORE_MEMW();
    __atomic_store_n(&slot->version, version + 2U, __ATOMIC_RELEASE);
}

int Rte_EnvironmentalXcoreRead(const Rte_EnvironmentalXcoreSlotType *slot,
                               Rte_EnvironmentalDataType *out)
{
    if (slot == 0 || out == 0) {
        return 0;
    }
    for (uint8_t retry = 0U; retry < RTE_XCORE_READ_RETRIES; ++retry) {
        const uint32_t start = __atomic_load_n(&slot->version, __ATOMIC_ACQUIRE);
        if ((start & 1U) != 0U) {
            continue;
        }
        RTE_XCORE_MEMW();
        *out = slot->value;
        RTE_XCORE_MEMW();
        const uint32_t end = __atomic_load_n(&slot->version, __ATOMIC_ACQUIRE);
        if (start == end) {
            return 1;
        }
    }
    return 0;
}

static int event_config_valid(const Rte_EventQueueConfigType *config)
{
    return config != 0 && config->burst != 0U &&
           config->burst <= RTE_EVENT_QUEUE_CAPACITY &&
           config->serviceRate != 0U &&
           config->serviceRate <= RTE_EVENT_QUEUE_CAPACITY &&
           config->fullPolicy <= RTE_EVENT_DROP_OLDEST;
}

Rte_EventResultType Rte_EventQueueInit(Rte_EventQueueType *queue,
                                       const Rte_EventQueueConfigType *config)
{
    if (queue == 0 || !event_config_valid(config)) {
        return RTE_EVENT_INVALID;
    }
    *queue = (Rte_EventQueueType){.config = *config};
#ifdef ESP_PLATFORM
    queue->handle = xQueueCreateStatic(RTE_EVENT_QUEUE_CAPACITY,
                                       sizeof(uint32_t),
                                       (uint8_t *)queue->storage,
                                       &queue->native);
    if (queue->handle == 0) {
        return RTE_EVENT_INVALID;
    }
#endif
    return RTE_EVENT_OK;
}

Rte_EventResultType Rte_EventQueuePush(Rte_EventQueueType *queue,
                                       uint32_t event)
{
    if (queue == 0 || !event_config_valid(&queue->config)) {
        return RTE_EVENT_INVALID;
    }
#ifdef ESP_PLATFORM
    if (xQueueSend(queue->handle, &event, 0U) == pdPASS) {
        return RTE_EVENT_OK;
    }
    if (queue->config.fullPolicy == RTE_EVENT_DROP_NEWEST) {
        queue->dropped++;
        return RTE_EVENT_FULL;
    }
    uint32_t discarded;
    (void)xQueueReceive(queue->handle, &discarded, 0U);
    queue->dropped++;
    return xQueueSend(queue->handle, &event, 0U) == pdPASS
        ? RTE_EVENT_OK : RTE_EVENT_FULL;
#else
    if (queue->count == RTE_EVENT_QUEUE_CAPACITY) {
        queue->dropped++;
        if (queue->config.fullPolicy == RTE_EVENT_DROP_NEWEST) {
            return RTE_EVENT_FULL;
        }
        queue->head = (uint8_t)((queue->head + 1U) % RTE_EVENT_QUEUE_CAPACITY);
        queue->count--;
    }
    queue->storage[queue->tail] = event;
    queue->tail = (uint8_t)((queue->tail + 1U) % RTE_EVENT_QUEUE_CAPACITY);
    queue->count++;
    return RTE_EVENT_OK;
#endif
}

Rte_EventResultType Rte_EventQueuePop(Rte_EventQueueType *queue,
                                      uint32_t *event)
{
    if (queue == 0 || event == 0 || !event_config_valid(&queue->config)) {
        return RTE_EVENT_INVALID;
    }
#ifdef ESP_PLATFORM
    return xQueueReceive(queue->handle, event, 0U) == pdPASS
        ? RTE_EVENT_OK : RTE_EVENT_FULL;
#else
    if (queue->count == 0U) {
        return RTE_EVENT_FULL;
    }
    *event = queue->storage[queue->head];
    queue->head = (uint8_t)((queue->head + 1U) % RTE_EVENT_QUEUE_CAPACITY);
    queue->count--;
    return RTE_EVENT_OK;
#endif
}

uint8_t Rte_EventQueuePending(const Rte_EventQueueType *queue)
{
    if (queue == 0) {
        return 0U;
    }
#ifdef ESP_PLATFORM
    return (uint8_t)uxQueueMessagesWaiting(queue->handle);
#else
    return queue->count;
#endif
}

uint8_t Rte_EventQueueService(Rte_EventQueueType *queue,
                              Rte_EventHandlerFn handler, void *context)
{
    if (queue == 0 || handler == 0 || !event_config_valid(&queue->config)) {
        return 0U;
    }
    uint8_t serviced = 0U;
    uint32_t event;
    while (serviced < queue->config.serviceRate &&
           Rte_EventQueuePop(queue, &event) == RTE_EVENT_OK) {
        handler(event, context);
        serviced++;
    }
    return serviced;
}
