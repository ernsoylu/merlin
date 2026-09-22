#include <assert.h>

#include "display_font.h"

int main(void)
{
    /* Column bytes are LSB = top row: 'L' is a full left column plus a bottom
       row, so every trailing column carries only bit 6. */
    const uint8_t *l = DisplayFont_Glyph('L');
    assert(l != 0);
    assert(l[0] == 0x7FU);
    for (uint8_t i = 1U; i < DISPLAY_FONT_WIDTH; ++i) {
        assert(l[i] == 0x40U);
    }

    /* 'I' is symmetric about its centre column; 'M' and 'N' are not, which is
       what makes the wordmark readable as a mirroring check. */
    const uint8_t *i_glyph = DisplayFont_Glyph('I');
    assert(i_glyph[0] == i_glyph[4] && i_glyph[1] == i_glyph[3]);
    const uint8_t *n = DisplayFont_Glyph('N');
    assert(n[1] != n[3]);

    assert(DisplayFont_Glyph('m') == DisplayFont_Glyph('M'));
    assert(DisplayFont_Glyph('Z') == 0);
    assert(DisplayFont_Glyph('\0') == 0);

    assert(DisplayFont_TextWidth("M", 1U) == DISPLAY_FONT_WIDTH);
    assert(DisplayFont_TextWidth("MERLIN", 1U) == 35U);
    assert(DisplayFont_TextWidth("MERLIN", 3U) == 105U);
    assert(DisplayFont_TextWidth("", 3U) == 0U);
    assert(DisplayFont_TextWidth(0, 3U) == 0U);
    assert(DisplayFont_TextWidth("MERLIN", 0U) == 0U);

    /* The wordmark has to fit the panel at the scale the demo renders it. */
    assert(DisplayFont_TextWidth("MERLIN", 3U) <= 128U);
    assert(DISPLAY_FONT_HEIGHT * 3U <= 64U);
    return 0;
}
