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

typedef struct {
    Rte_EnvironmentalDataType value;
} Rte_EnvironmentalSlotType;

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

void Rte_EnvironmentalPublish(Rte_EnvironmentalSlotType *slot,
                              const Rte_EnvironmentalDataType *value);
void Rte_EnvironmentalRead(const Rte_EnvironmentalSlotType *slot,
                           Rte_EnvironmentalDataType *out);
int Rte_EnvironmentalIsFresh(const Rte_EnvironmentalDataType *value,
                             int64_t nowUs, uint32_t maxAgeMs);

#endif
