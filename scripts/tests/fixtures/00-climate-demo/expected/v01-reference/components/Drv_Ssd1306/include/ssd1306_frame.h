#ifndef SSD1306_FRAME_H
#define SSD1306_FRAME_H

#include <stdint.h>

#include "Mcal_I2c.h"

#define SSD1306_FRAME_WIDTH 128U
#define SSD1306_FRAME_HEIGHT 64U
#define SSD1306_FRAME_BYTES (SSD1306_FRAME_WIDTH * SSD1306_FRAME_HEIGHT / 8U)
#define SSD1306_TRANSFER_CHUNK_BYTES 32U

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t sequence;
    const uint8_t *pixels;
    uint16_t pixelBytes;
} Ssd1306_FrameViewType;

void Ssd1306_FrameClear(uint8_t pixels[SSD1306_FRAME_BYTES]);
int Ssd1306_FrameSetPixel(uint8_t pixels[SSD1306_FRAME_BYTES],
                          uint16_t x, uint16_t y, int on);
int Ssd1306_FrameIsValid(const Ssd1306_FrameViewType *frame);

typedef enum {
    SSD1306_HEALTH_UNINIT = 0,
    SSD1306_HEALTH_READY,
    SSD1306_HEALTH_DEGRADED
} Ssd1306_HealthType;

typedef struct {
    Mcal_I2cInterfaceType i2c;
    uint8_t address;
    uint8_t active[SSD1306_FRAME_BYTES];
    uint8_t pending[SSD1306_FRAME_BYTES];
    uint32_t activeSequence;
    uint32_t pendingSequence;
    uint32_t lastCompletedSequence;
    uint16_t offset;
    uint8_t activeValid;
    uint8_t pendingValid;
    uint8_t consecutiveFailures;
    uint8_t recoveryCooldown;
    Ssd1306_HealthType health;
    uint32_t transferFailures;
    uint32_t recoveryCount;
} Ssd1306_InstanceType;

void Ssd1306_InstanceInit(Ssd1306_InstanceType *instance, uint8_t address,
                          Mcal_I2cInterfaceType i2c);
Mcal_ResultType Ssd1306_Initialize(Ssd1306_InstanceType *instance);
Mcal_ResultType Ssd1306_SubmitFrame(Ssd1306_InstanceType *instance,
                                    const Ssd1306_FrameViewType *frame);
Mcal_ResultType Ssd1306_TransferChunk(Ssd1306_InstanceType *instance);

#endif
