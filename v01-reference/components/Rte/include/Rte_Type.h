#ifndef RTE_TYPE_H
#define RTE_TYPE_H

#include <stdint.h>

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

#endif
