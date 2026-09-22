#include "DisplayDemo.h"

#include "display_font.h"

/* The reference wordmark. P-19 asks for origin, rotation, mirroring and
   clipping to be judged by eye; asymmetric letterforms show all four in a
   way a checkerboard cannot. */
#define DISPLAY_DEMO_WORDMARK "MERLIN"
#define DISPLAY_DEMO_SCALE 3U

static void set_pixel(Rte_MonochromeFrameType *frame, uint16_t x,
                      uint16_t y, int on)
{
    if (x >= RTE_MONOCHROME_FRAME_WIDTH || y >= RTE_MONOCHROME_FRAME_HEIGHT) {
        return;
    }
    const uint16_t index = (uint16_t)(x +
        (y / 8U) * RTE_MONOCHROME_FRAME_WIDTH);
    const uint8_t mask = (uint8_t)(1U << (y % 8U));
    if (on) {
        frame->pixels[index] |= mask;
    } else {
        frame->pixels[index] &= (uint8_t)~mask;
    }
}

static void fill_block(Rte_MonochromeFrameType *frame, uint16_t x, uint16_t y,
                       uint8_t scale)
{
    for (uint8_t dy = 0U; dy < scale; ++dy) {
        for (uint8_t dx = 0U; dx < scale; ++dx) {
            set_pixel(frame, (uint16_t)(x + dx), (uint16_t)(y + dy), 1);
        }
    }
}

static void draw_glyph(Rte_MonochromeFrameType *frame, const uint8_t *glyph,
                       uint16_t x, uint16_t y, uint8_t scale)
{
    for (uint8_t col = 0U; col < DISPLAY_FONT_WIDTH; ++col) {
        for (uint8_t row = 0U; row < DISPLAY_FONT_HEIGHT; ++row) {
            if ((glyph[col] >> row) & 1U) {
                fill_block(frame, (uint16_t)(x + col * scale),
                           (uint16_t)(y + row * scale), scale);
            }
        }
    }
}

static void draw_text(Rte_MonochromeFrameType *frame, uint16_t x, uint16_t y,
                      uint8_t scale, const char *text)
{
    const uint16_t advance =
        (uint16_t)((DISPLAY_FONT_WIDTH + DISPLAY_FONT_SPACING) * scale);
    for (uint16_t i = 0U; text[i] != '\0'; ++i) {
        const uint8_t *glyph = DisplayFont_Glyph(text[i]);
        if (glyph != 0) {
            draw_glyph(frame, glyph, (uint16_t)(x + i * advance), y, scale);
        }
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
        frame->pixels[i] = 0U;
    }
    for (uint16_t x = 0; x < frame->width; ++x) {
        set_pixel(frame, x, 0U, 1);
        set_pixel(frame, x, (uint16_t)(frame->height - 1U), 1);
    }
    for (uint16_t y = 0; y < frame->height; ++y) {
        set_pixel(frame, 0U, y, 1);
        set_pixel(frame, (uint16_t)(frame->width - 1U), y, 1);
    }
    const uint16_t textWidth =
        DisplayFont_TextWidth(DISPLAY_DEMO_WORDMARK, DISPLAY_DEMO_SCALE);
    const uint16_t textHeight =
        (uint16_t)(DISPLAY_FONT_HEIGHT * DISPLAY_DEMO_SCALE);
    draw_text(frame, (uint16_t)((frame->width - textWidth) / 2U),
              (uint16_t)((frame->height - textHeight) / 2U),
              DISPLAY_DEMO_SCALE, DISPLAY_DEMO_WORDMARK);
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
