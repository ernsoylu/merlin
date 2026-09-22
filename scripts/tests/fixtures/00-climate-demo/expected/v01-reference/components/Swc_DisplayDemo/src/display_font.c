#include "display_font.h"

/* Only the glyphs the reference wordmark needs. Extend the table rather than
   starting a second one. */
typedef struct {
    char value;
    uint8_t columns[DISPLAY_FONT_WIDTH];
} DisplayFont_GlyphType;

static const DisplayFont_GlyphType GLYPHS[] = {
    {'M', {0x7FU, 0x02U, 0x04U, 0x02U, 0x7FU}},
    {'E', {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U}},
    {'R', {0x7FU, 0x09U, 0x09U, 0x19U, 0x66U}},
    {'L', {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U}},
    {'I', {0x41U, 0x41U, 0x7FU, 0x41U, 0x41U}},
    {'N', {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU}},
};

#define GLYPH_COUNT (sizeof(GLYPHS) / sizeof(GLYPHS[0]))

static char upper(char value)
{
    return value >= 'a' && value <= 'z' ? (char)(value - ('a' - 'A')) : value;
}

const uint8_t *DisplayFont_Glyph(char value)
{
    const char wanted = upper(value);
    for (uint8_t i = 0U; i < (uint8_t)GLYPH_COUNT; ++i) {
        if (GLYPHS[i].value == wanted) {
            return GLYPHS[i].columns;
        }
    }
    return 0;
}

uint16_t DisplayFont_TextWidth(const char *text, uint8_t scale)
{
    if (text == 0 || text[0] == '\0' || scale == 0U) {
        return 0U;
    }
    uint16_t count = 0U;
    while (text[count] != '\0') {
        count++;
    }
    const uint16_t cells = (uint16_t)(count * DISPLAY_FONT_WIDTH +
                                      (count - 1U) * DISPLAY_FONT_SPACING);
    return (uint16_t)(cells * scale);
}
