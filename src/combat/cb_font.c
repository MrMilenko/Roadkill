/* cb_font.c: see cb_font.h */
#include "combat/cb_font.h"

int32_t cb_font_char_width(const TM2Font* f, uint8_t c) {
    if (c == 10 || c == 13)
        return 0;
    uint32_t w = c == 0x20 ? f->data[0x11C] : f->data[6 * font_glyph(c) + 2];
    return (int32_t)(w + (uint32_t)f->spacing);
}

int32_t cb_font_print_centered(const TM2Font* f, TM2FontPrint* p, int32_t x, int32_t y, const uint8_t* s) {
    int32_t w = font_string_width(f, s);
    *p->y = y;
    *p->x = (int32_t)((uint32_t)x - (uint32_t)(w >> 1));
    return font_print(f, p, s);
}
