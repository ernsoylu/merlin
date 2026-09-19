#include <assert.h>
#include <stdint.h>

#include "ssd1306_frame.h"

int main(void)
{
    uint8_t pixels[SSD1306_FRAME_BYTES];
    Ssd1306_FrameClear(pixels);
    assert(Ssd1306_FrameSetPixel(pixels, 0, 0, 1));
    assert(Ssd1306_FrameSetPixel(pixels, 0, 7, 1));
    assert(Ssd1306_FrameSetPixel(pixels, 0, 8, 1));
    assert(pixels[0] == 0x81U && pixels[SSD1306_FRAME_WIDTH] == 0x01U);
    assert(Ssd1306_FrameSetPixel(pixels, 0, 7, 0));
    assert(pixels[0] == 0x01U);
    assert(!Ssd1306_FrameSetPixel(pixels, SSD1306_FRAME_WIDTH, 0, 1));
    assert(!Ssd1306_FrameSetPixel(pixels, 0, SSD1306_FRAME_HEIGHT, 1));

    const Ssd1306_FrameViewType valid = {
        .width = SSD1306_FRAME_WIDTH,
        .height = SSD1306_FRAME_HEIGHT,
        .sequence = 1,
        .pixels = pixels,
        .pixelBytes = SSD1306_FRAME_BYTES
    };
    assert(Ssd1306_FrameIsValid(&valid));
    Ssd1306_FrameViewType invalid = valid;
    invalid.pixelBytes--;
    assert(!Ssd1306_FrameIsValid(&invalid));
    return 0;
}
