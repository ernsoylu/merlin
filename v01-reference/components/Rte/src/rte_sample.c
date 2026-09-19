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

void Rte_SampleRepublish(Rte_SampleType *slot)
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
    SAMPLE_LOCK();
    slot->value = *value;
    slot->value.sequence++;
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
