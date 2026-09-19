#include <assert.h>
#include <stdint.h>

#include "ssd1306_frame.h"

typedef struct {
    Mcal_ResultType result;
    Mcal_ResultType recoveryResult;
    uint32_t writes;
    uint32_t recoveries;
    uint8_t lastRegister;
    uint8_t firstData;
    uint16_t lastLength;
} MockDisplayI2cType;

static Mcal_ResultType write_register(void *context, uint8_t address,
                                      uint8_t reg, const uint8_t *data,
                                      uint16_t length)
{
    MockDisplayI2cType *mock = context;
    (void)address;
    mock->writes++;
    mock->lastRegister = reg;
    mock->lastLength = length;
    if (length != 0U) {
        mock->firstData = data[0];
    }
    return mock->result;
}

static Mcal_ResultType recover(void *context)
{
    MockDisplayI2cType *mock = context;
    mock->recoveries++;
    return mock->recoveryResult;
}

int main(void)
{
    MockDisplayI2cType mock = {.result = MCAL_OK, .recoveryResult = MCAL_OK};
    const Mcal_I2cInterfaceType i2c = {
        .context = &mock, .writeRegister = write_register,
        .recover = recover
    };
    Ssd1306_InstanceType display;
    Ssd1306_InstanceInit(&display, 0x3CU, i2c);
    assert(Ssd1306_Initialize(&display) == MCAL_OK);
    assert(display.health == SSD1306_HEALTH_READY);

    uint8_t first[SSD1306_FRAME_BYTES];
    uint8_t second[SSD1306_FRAME_BYTES];
    for (uint16_t i = 0; i < SSD1306_FRAME_BYTES; ++i) {
        first[i] = 0xA5U;
        second[i] = 0x5AU;
    }
    const Ssd1306_FrameViewType frame1 = {
        .width = SSD1306_FRAME_WIDTH, .height = SSD1306_FRAME_HEIGHT,
        .sequence = 1U, .pixels = first, .pixelBytes = SSD1306_FRAME_BYTES
    };
    const Ssd1306_FrameViewType frame2 = {
        .width = SSD1306_FRAME_WIDTH, .height = SSD1306_FRAME_HEIGHT,
        .sequence = 2U, .pixels = second, .pixelBytes = SSD1306_FRAME_BYTES
    };
    assert(Ssd1306_SubmitFrame(&display, &frame1) == MCAL_OK);
    first[0] = 0U;
    assert(Ssd1306_SubmitFrame(&display, &frame2) == MCAL_OK);
    for (unsigned int i = 0; i < SSD1306_FRAME_BYTES / SSD1306_TRANSFER_CHUNK_BYTES; ++i) {
        assert(Ssd1306_TransferChunk(&display) == MCAL_OK);
        if (i == 0U) {
            assert(mock.firstData == 0xA5U);
        }
    }
    assert(display.lastCompletedSequence == 1U);
    assert(display.activeValid && display.activeSequence == 2U);

    mock.result = MCAL_NACK;
    assert(Ssd1306_TransferChunk(&display) == MCAL_NACK);
    assert(Ssd1306_TransferChunk(&display) == MCAL_NACK);
    assert(Ssd1306_TransferChunk(&display) == MCAL_NACK);
    assert(mock.recoveries == 1U && display.recoveryCount == 1U);
    mock.result = MCAL_OK;
    assert(Ssd1306_TransferChunk(&display) == MCAL_OK);
    assert(display.transferFailures == 3U);
    return 0;
}
