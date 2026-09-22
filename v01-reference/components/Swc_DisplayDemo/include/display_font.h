#ifndef DISPLAY_FONT_H
#define DISPLAY_FONT_H

#include <stdint.h>

/* Generic 5x7 glyph data. This file knows about letter shapes and nothing
   about what any particular screen chooses to spell. */

#define DISPLAY_FONT_WIDTH 5U
#define DISPLAY_FONT_HEIGHT 7U
#define DISPLAY_FONT_SPACING 1U

/* Five column bytes, one per glyph column, bit 0 = top row. Returns 0 when the
   character is outside the supported set, which the caller renders as a gap. */
const uint8_t *DisplayFont_Glyph(char value);

/* Rendered width in pixels, spacing between glyphs included, trailing spacing
   excluded. Zero for an empty or null string. */
uint16_t DisplayFont_TextWidth(const char *text, uint8_t scale);

#endif
