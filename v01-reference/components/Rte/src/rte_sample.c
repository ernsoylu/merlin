#include "Rte_Sample.h"

void Rte_SamplePublish(Rte_SampleType *slot, int32_t value,
                       Rte_SampleQualityType quality, int64_t sampleTimeUs)
{
    slot->value = value;
    slot->quality = quality;
    slot->sampleTimeUs = sampleTimeUs;
    slot->sequence++;
}

void Rte_SampleRepublish(Rte_SampleType *slot)
{
    /* Cached data keeps its acquisition time and sequence. */
    (void)slot;
}

void Rte_SampleRead(const Rte_SampleType *slot, Rte_SampleType *out)
{
    *out = *slot;
}

int Rte_SampleIsFresh(const Rte_SampleType *sample, int64_t nowUs, uint32_t maxAgeMs)
{
    return sample->quality == RTE_SAMPLE_VALID &&
           nowUs >= sample->sampleTimeUs &&
           nowUs - sample->sampleTimeUs <= (int64_t)maxAgeMs * 1000;
}
