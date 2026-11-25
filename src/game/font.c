/*
 * font.c: the front end's bitmap fonts (see font.h).
 */
#include "font.h"

static uint8_t remap(uint8_t c) {
    switch (c) {
    case '.': return 0x3b;
    case '!': return 0x5c;
    case ',': return 0x5e;
    case '\'': return 0x60;
    case '*': return 0x7b;
    default: return c;
    }
}

int32_t font_glyph(uint8_t c) {
    uint8_t m = remap(c);
    if (m < 0x30 || m > 0x7c) return 0x2f;
    return m - 0x30;
}

int32_t font_string_width(const TM2Font* f, const uint8_t* s) {
    int32_t w = 0;
    for (; *s; s++) {
        uint8_t c = *s;
        if (c == 8) { w -= 1; continue; }
        if (c == 10) { w = 0; continue; }
        if (c == 13) continue;
        if (c == 0x20) { w += f->data[0x11c] + f->spacing; continue; }
        w += f->data[font_glyph(c) * 6 + 2] + f->spacing;
    }
    return w;
}

int32_t font_print(const TM2Font* f, TM2FontPrint* p, const uint8_t* s) {
    int32_t n = 0, x = *p->x;
    if (!*s) return 0;
    for (; *s; s++, n++) {
        uint8_t c = *s;
        if (c == 8) { x -= 1; continue; }
        if (c == 10) { x = 0; *p->y += f->data[0x105]; continue; }
        if (c == 13) { *p->y += f->data[0x105]; continue; }
        if (c == 0x20) { x += f->data[0x11c] + f->spacing; continue; }
        const uint8_t* g = f->data + font_glyph(c) * 6;
        if (!g[2] || !g[3]) continue;
        uint8_t* r = p->sprite;
        r[0x10] = (uint8_t)(g[0] + p->base_u);
        r[0x11] = (uint8_t)(g[1] + p->base_v);
        r[0x0c] = g[2]; r[0x0d] = 0;
        r[0x0e] = g[3]; r[0x0f] = 0;
        r[0x12] = (uint8_t)(g[0] + p->base_u + g[2]);
        r[0x13] = (uint8_t)(g[1] + p->base_v + g[3]);
        r[4] = r[5] = r[6] = 0xff;
        r[7] &= 0xfd;
        *p->x = x;
        int16_t y = (int16_t)((int16_t)(g[4] | g[5] << 8) + ((int16_t)*p->y - g[3]));
        r[0x0a] = (uint8_t)y; r[0x0b] = (uint8_t)((uint16_t)y >> 8);
        r[0x08] = (uint8_t)x; r[0x09] = (uint8_t)((uint32_t)x >> 8);
        if (p->draw) p->draw(p->user, r);
        x = *p->x + f->data[font_glyph(c) * 6 + 2] + f->spacing;
    }
    *p->x = x;
    return n;
}

int32_t font_sprite_print(const TM2Font* f, TM2FontPrint* p, uint8_t* out, const uint8_t* s) {
    int32_t n = 0;
    for (; *s; s++, n++, out += 24) {
        uint8_t c = *s;
        if (c == 8) { *p->x -= 1; continue; }
        if (c == 10) { *p->x = 0; *p->y += f->data[0x105]; continue; }
        if (c == 13) { *p->y += f->data[0x105]; continue; }
        if (c == 0x20) { *p->x += f->data[0x11c] + f->spacing; continue; }
        const uint8_t* g = f->data + font_glyph(c) * 6;
        for (int k = 0; k < 24; k++) out[k] = p->sprite[k];
        out[0x10] = (uint8_t)(g[0] + p->base_u);
        out[0x11] = (uint8_t)(g[1] + p->base_v);
        out[0x0c] = g[2]; out[0x0d] = 0;
        out[0x0e] = g[3]; out[0x0f] = 0;
        out[0x12] = (uint8_t)(g[0] + p->base_u + g[2]);
        out[0x13] = (uint8_t)(g[1] + p->base_v + g[3]);
        out[0x08] = (uint8_t)*p->x; out[0x09] = (uint8_t)((uint32_t)*p->x >> 8);
        int16_t y = (int16_t)(((int16_t)*p->y - g[3]) + (int16_t)(g[4] | g[5] << 8));
        out[4] = out[5] = out[6] = 0xff;
        out[0x0a] = (uint8_t)y; out[0x0b] = (uint8_t)((uint16_t)y >> 8);
        if (p->draw) p->draw(p->user, out);
        *p->x += f->data[font_glyph(c) * 6 + 2] + f->spacing;
    }
    return n;
}

void font_sprite_print_xy(const TM2Font* f, TM2FontPrint* p, int32_t x, int32_t y, uint8_t* out, const uint8_t* s) {
    *p->x = x;
    *p->y = y;
    font_sprite_print(f, p, out, s);
}

void font_sprite_print_centered(const TM2Font* f, TM2FontPrint* p, int32_t x, int32_t y, uint8_t* out, const uint8_t* s) {
    int32_t w = font_string_width(f, s);
    *p->x = x - (w >> 1);
    *p->y = y;
    font_sprite_print(f, p, out, s);
}
