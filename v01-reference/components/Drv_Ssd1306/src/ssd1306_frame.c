#include "ssd1306_frame.h"

#define SSD1306_CONTROL_COMMAND 0x00U
#define SSD1306_CONTROL_DATA 0x40U
#define SSD1306_MAX_FAILURES_BEFORE_RECOVERY 3U
#define SSD1306_RECOVERY_COOLDOWN_ACTIVATIONS 10U

void Ssd1306_FrameClear(uint8_t pixels[SSD1306_FRAME_BYTES])
{
    for (uint16_t i = 0; i < SSD1306_FRAME_BYTES; ++i) {
        pixels[i] = 0U;
    }
}

int Ssd1306_FrameSetPixel(uint8_t pixels[SSD1306_FRAME_BYTES],
                          uint16_t x, uint16_t y, int on)
{
    if (x >= SSD1306_FRAME_WIDTH || y >= SSD1306_FRAME_HEIGHT) {
        return 0;
    }

    const uint16_t index = (uint16_t)(x + (y / 8U) * SSD1306_FRAME_WIDTH);
    const uint8_t mask = (uint8_t)(1U << (y % 8U));
    if (on) {
        pixels[index] |= mask;
    } else {
        pixels[index] &= (uint8_t)~mask;
    }
    return 1;
}

int Ssd1306_FrameIsValid(const Ssd1306_FrameViewType *frame)
{
    return frame != 0 && frame->pixels != 0 &&
           frame->width == SSD1306_FRAME_WIDTH &&
           frame->height == SSD1306_FRAME_HEIGHT &&
           frame->pixelBytes == SSD1306_FRAME_BYTES;
}

static void promote_pending(Ssd1306_InstanceType *instance)
{
    if (!instance->pendingValid) {
        return;
    }
    for (uint16_t i = 0; i < SSD1306_FRAME_BYTES; ++i) {
        instance->active[i] = instance->pending[i];
    }
    instance->activeSequence = instance->pendingSequence;
    instance->offset = 0U;
    instance->activeValid = 1U;
    instance->pendingValid = 0U;
}

void Ssd1306_InstanceInit(Ssd1306_InstanceType *instance, uint8_t address,
                          Mcal_I2cInterfaceType i2c)
{
    *instance = (Ssd1306_InstanceType){
        .i2c = i2c, .address = address, .health = SSD1306_HEALTH_UNINIT
    };
}

Mcal_ResultType Ssd1306_Initialize(Ssd1306_InstanceType *instance)
{
    static const uint8_t commands[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xAF
    };
    if (instance == 0 || instance->i2c.writeRegister == 0) {
        return MCAL_INVALID_ARG;
    }
    const Mcal_ResultType result = instance->i2c.writeRegister(
        instance->i2c.context, instance->address, SSD1306_CONTROL_COMMAND,
        commands, sizeof(commands));
    instance->health = result == MCAL_OK ? SSD1306_HEALTH_READY
                                         : SSD1306_HEALTH_DEGRADED;
    return result;
}

Mcal_ResultType Ssd1306_SubmitFrame(Ssd1306_InstanceType *instance,
                                    const Ssd1306_FrameViewType *frame)
{
    if (instance == 0 || !Ssd1306_FrameIsValid(frame)) {
        return MCAL_INVALID_ARG;
    }
    uint8_t *destination = instance->activeValid ? instance->pending
                                                 : instance->active;
    for (uint16_t i = 0; i < SSD1306_FRAME_BYTES; ++i) {
        destination[i] = frame->pixels[i];
    }
    if (instance->activeValid) {
        instance->pendingSequence = frame->sequence;
        instance->pendingValid = 1U;
    } else {
        instance->activeSequence = frame->sequence;
        instance->offset = 0U;
        instance->activeValid = 1U;
    }
    return MCAL_OK;
}

Mcal_ResultType Ssd1306_TransferChunk(Ssd1306_InstanceType *instance)
{
    if (instance == 0 || instance->i2c.writeRegister == 0) {
        return MCAL_INVALID_ARG;
    }
    if (!instance->activeValid) {
        promote_pending(instance);
        return MCAL_OK;
    }
    const uint16_t remaining = SSD1306_FRAME_BYTES - instance->offset;
    const uint16_t length = remaining < SSD1306_TRANSFER_CHUNK_BYTES
        ? remaining : SSD1306_TRANSFER_CHUNK_BYTES;
    const Mcal_ResultType result = instance->i2c.writeRegister(
        instance->i2c.context, instance->address, SSD1306_CONTROL_DATA,
        &instance->active[instance->offset], length);
    if (result != MCAL_OK) {
        instance->transferFailures++;
        instance->consecutiveFailures++;
        instance->health = SSD1306_HEALTH_DEGRADED;
        if (instance->consecutiveFailures >= SSD1306_MAX_FAILURES_BEFORE_RECOVERY &&
            instance->recoveryCooldown == 0U && instance->i2c.recover != 0) {
            const Mcal_ResultType recovery = instance->i2c.recover(
                instance->i2c.context);
            instance->recoveryCount++;
            instance->consecutiveFailures = 0U;
            instance->recoveryCooldown = SSD1306_RECOVERY_COOLDOWN_ACTIVATIONS;
            if (recovery == MCAL_OK) {
                instance->health = SSD1306_HEALTH_READY;
            }
        }
        return result;
    }
    instance->consecutiveFailures = 0U;
    if (instance->recoveryCooldown > 0U) {
        instance->recoveryCooldown--;
    }
    instance->offset = (uint16_t)(instance->offset + length);
    if (instance->offset == SSD1306_FRAME_BYTES) {
        instance->lastCompletedSequence = instance->activeSequence;
        instance->activeValid = 0U;
        promote_pending(instance);
    }
    instance->health = SSD1306_HEALTH_READY;
    return MCAL_OK;
}
