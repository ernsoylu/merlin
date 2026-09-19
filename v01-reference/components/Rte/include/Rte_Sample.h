#ifndef RTE_SAMPLE_H
#define RTE_SAMPLE_H

#include <stdint.h>

typedef enum {
    RTE_SAMPLE_INITIAL = 0,
    RTE_SAMPLE_VALID,
    RTE_SAMPLE_INVALID
} Rte_SampleQualityType;

typedef struct {
    int32_t value;
    Rte_SampleQualityType quality;
    int64_t sampleTimeUs;
    uint32_t sequence;
} Rte_SampleType;

void Rte_SamplePublish(Rte_SampleType *slot, int32_t value,
                       Rte_SampleQualityType quality, int64_t sampleTimeUs);
void Rte_SampleRepublish(const Rte_SampleType *slot);
void Rte_SampleRead(const Rte_SampleType *slot, Rte_SampleType *out);
int Rte_SampleIsFresh(const Rte_SampleType *sample, int64_t nowUs, uint32_t maxAgeMs);

#endif
