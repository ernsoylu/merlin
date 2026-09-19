#include "DisplayDemo.h"

static void set_pixel(Rte_MonochromeFrameType *frame, uint16_t x,
                      uint16_t y, int on)
{
    const uint16_t index = (uint16_t)(x +
        (y / 8U) * RTE_MONOCHROME_FRAME_WIDTH);
    const uint8_t mask = (uint8_t)(1U << (y % 8U));
    if (on) {
        frame->pixels[index] |= mask;
    } else {
        frame->pixels[index] &= (uint8_t)~mask;
    }
}

void DisplayDemo_Init(DisplayDemo_CtxType *context)
{
    *context = (DisplayDemo_CtxType){
        .frame = {
            .width = RTE_MONOCHROME_FRAME_WIDTH,
            .height = RTE_MONOCHROME_FRAME_HEIGHT,
            .quality = RTE_QUALITY_INITIAL
        }
    };
}

void DisplayDemo_Run(DisplayDemo_CtxType *context)
{
    Rte_MonochromeFrameType *frame = &context->frame;
    frame->sequence++;
    frame->quality = RTE_QUALITY_VALID;
    for (uint16_t i = 0; i < RTE_MONOCHROME_FRAME_BYTES; ++i) {
        frame->pixels[i] = (uint8_t)(((i + frame->sequence) & 1U) ?
                                     0xAAU : 0x55U);
    }
    for (uint16_t x = 0; x < frame->width; ++x) {
        set_pixel(frame, x, 0U, 1);
        set_pixel(frame, x, (uint16_t)(frame->height - 1U), 1);
    }
    for (uint16_t y = 0; y < frame->height; ++y) {
        set_pixel(frame, 0U, y, 1);
        set_pixel(frame, (uint16_t)(frame->width - 1U), y, 1);
    }
    for (uint8_t bit = 0; bit < 8U; ++bit) {
        set_pixel(frame, (uint16_t)(120U + bit), 4U,
                  (frame->sequence >> bit) & 1U);
    }
}

const Rte_MonochromeFrameType *DisplayDemo_GetFrame(
    const DisplayDemo_CtxType *context)
{
    return context == 0 ? 0 : &context->frame;
}
