#include <assert.h>
#include <string.h>

#include "DisplayDemo.h"
#include "ssd1306_frame.h"

typedef struct {
    uint32_t writes;
} MockI2c;

static Mcal_ResultType write_frame(void *context, uint8_t address,
                                   uint8_t reg, const uint8_t *data,
                                   uint16_t length)
{
    MockI2c *mock = context;
    (void)address;
    (void)reg;
    (void)data;
    assert(length > 0U);
    mock->writes++;
    return MCAL_OK;
}

int main(void)
{
    DisplayDemo_CtxType demo;
    DisplayDemo_Init(&demo);
    DisplayDemo_Run(&demo);
    const Rte_MonochromeFrameType *first = DisplayDemo_GetFrame(&demo);
    uint8_t snapshot[RTE_MONOCHROME_FRAME_BYTES];
    memcpy(snapshot, first->pixels, sizeof(snapshot));
    assert(first->sequence == 1U);
    assert(first->quality == RTE_QUALITY_VALID);

    DisplayDemo_Run(&demo);
    const Rte_MonochromeFrameType *second = DisplayDemo_GetFrame(&demo);
    assert(second->sequence == 2U);
    assert(memcmp(snapshot, second->pixels, sizeof(snapshot)) != 0);

    MockI2c mock = {0};
    Mcal_I2cInterfaceType i2c = {
        .context = &mock, .writeRegister = write_frame
    };
    Ssd1306_InstanceType display;
    Ssd1306_InstanceInit(&display, 0x3C, i2c);
    Ssd1306_FrameViewType view = {
        .width = second->width, .height = second->height,
        .sequence = second->sequence, .pixels = second->pixels,
        .pixelBytes = RTE_MONOCHROME_FRAME_BYTES
    };
    assert(Ssd1306_SubmitFrame(&display, &view) == MCAL_OK);
    for (uint16_t i = 0; i < RTE_MONOCHROME_FRAME_BYTES /
                              SSD1306_TRANSFER_CHUNK_BYTES; ++i) {
        assert(Ssd1306_TransferChunk(&display) == MCAL_OK);
    }
    assert(display.lastCompletedSequence == 2U);
    assert(mock.writes == 32U);
    return 0;
}
