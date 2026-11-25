/*
 * en_sprite.c: 2D sprite helpers, ported from TM2.EXE 0x00496d04, 0x00496da0, 0x00496dcc,
 * 0x00496ee8, 0x00496fac (see en_sprite.h).
 */
#include "en_sprite.h"

#include <string.h>

static uint8_t r8(EnSpriteEnv* e, uint32_t a) { return *e->mem(e->user, a); }
static void w8(EnSpriteEnv* e, uint32_t a, uint8_t v) { *e->mem(e->user, a) = v; }
static uint16_t r16(EnSpriteEnv* e, uint32_t a) { uint16_t v; memcpy(&v, e->mem(e->user, a), 2); return v; }
static void w16(EnSpriteEnv* e, uint32_t a, uint16_t v) { memcpy(e->mem(e->user, a), &v, 2); }
static uint32_t r32(EnSpriteEnv* e, uint32_t a) { uint32_t v; memcpy(&v, e->mem(e->user, a), 4); return v; }
static void w32(EnSpriteEnv* e, uint32_t a, uint32_t v) { memcpy(e->mem(e->user, a), &v, 4); }

int32_t en_sprite_parse_rects(EnSpriteEnv* e, uint32_t node, uint32_t out, uint32_t max) {
    if (node == 0 || r8(e, node) != 0) return 0;
    uint32_t cnt = r32(e, node + 0x10);
    uint32_t poly = r32(e, node + 0xc);
    int32_t n = (int32_t)(max < cnt ? max : cnt);
    for (int32_t i = 0; i < n; i++) {
        uint32_t uv = poly + 0x18 + 4u * (r32(e, poly + 0x10) << 1 >> 29);
        w8(e, out, r8(e, uv));
        w8(e, out + 1, r8(e, uv + 2));
        w8(e, out + 2, (uint8_t)(r8(e, uv + 8) - r8(e, uv + 0xc)));
        w8(e, out + 3, (uint8_t)(r8(e, uv + 0xa) - r8(e, uv + 6)));
        w16(e, out + 4, r16(e, uv + ((r8(e, poly) & 0x7f) == 3 ? 0xc : 0x10)));
        out += 8;
        poly += 4u * r8(e, poly + 2);
    }
    return n;
}

int32_t en_sprite_parse_2d(EnSpriteEnv* e, uint32_t node, uint32_t out, uint32_t max) {
    int32_t n = en_sprite_parse_rects(e, node, out, max);
    for (int32_t i = 0; i < n; i++) {
        uint32_t r = out + 8u * (uint32_t)i;
        w8(e, r + 2, (uint8_t)(r8(e, r + 2) + 1));
        w8(e, r + 3, (uint8_t)(r8(e, r + 3) + 1));
    }
    return n;
}

/* one digit into a SPRT record (full: also u + w, v + h and the page) */
static void digit(EnSpriteEnv* e, uint32_t p, uint32_t d, int full) {
    w8(e, p + 0x10, r8(e, d));
    w8(e, p + 0x11, r8(e, d + 1));
    w16(e, p + 0xc, r8(e, d + 2));
    w16(e, p + 0xe, r8(e, d + 3));
    if (!full) return;
    w8(e, p + 0x12, (uint8_t)(r8(e, d) + r8(e, d + 2)));
    w8(e, p + 0x13, (uint8_t)(r8(e, d + 1) + r8(e, d + 3)));
    w32(e, p + 0x14, r16(e, d + 4));
}

void en_sprite_print_number(EnSpriteEnv* e, int32_t value, uint32_t prim, uint32_t rects) {
    if (value < 10) {
        digit(e, prim, rects + 8u * (uint32_t)value, 1);
        e->add_prim(e->user, prim);
        return;
    }
    if (value >= 100) value %= 100;
    digit(e, prim, rects + 8u * (uint32_t)(value / 10), 1);
    e->add_prim(e->user, prim);
    digit(e, prim + 0x18, rects + 8u * (uint32_t)(value % 10), 1);
    e->add_prim(e->user, prim + 0x18);
}

void en_sprite_print_number2(EnSpriteEnv* e, int32_t value, uint32_t prim, uint32_t rects) {
    if (value < 10) {
        digit(e, prim, rects + 8u * (uint32_t)value, 0);
        e->add_prim(e->user, prim);
        return;
    }
    if (value >= 100) value %= 100;
    digit(e, prim, rects + 8u * (uint32_t)(value / 10), 0);
    e->add_prim(e->user, prim);
    w16(e, prim + 0x20, (uint16_t)(r16(e, prim + 8) + r16(e, prim + 0xc) + 1));
    digit(e, prim + 0x18, rects + 8u * (uint32_t)(value % 10), 0);
    e->add_prim(e->user, prim + 0x18);
}

uint32_t en_sprite_get_a36c54(EnSpriteEnv* e) { return r32(e, 0xa36c54u); }

uint32_t en_load_4bit_clut(EnSpriteEnv* e, const EnFileIo* io, uint32_t path, uint32_t w_out, uint32_t h_out) {
    uint32_t f = io->open(e->user, path);
    if (f == 0xffffffffu) { w32(e, w_out, 0); w32(e, h_out, 0); return 0; }
    /* the original reads into stack words; on a file shorter than 4 bytes their rest is stale */
    uint8_t ta[2] = { 0, 0 }, tb[2] = { 0, 0 };
    io->read(e->user, f, ta, 2);
    io->read(e->user, f, tb, 2);
    uint16_t a = (uint16_t)(ta[0] | ta[1] << 8), b = (uint16_t)(tb[0] | tb[1] << 8);
    w32(e, w_out, (uint32_t)a + 1);
    w32(e, h_out, (uint32_t)b + 1);
    uint32_t size = r32(e, w_out) * r32(e, h_out);
    uint8_t slot = r8(e, 0xa2dba4u);
    uint32_t buf = 0;
    if (slot < 5) { buf = 0xa05ba4u + ((uint32_t)slot << 15); w8(e, 0xa2dba4u, (uint8_t)(slot + 1)); }
    if (!buf) {
        io->close(e->user, f);
        w32(e, w_out, 0);   /* EDX is 0 here */
        w32(e, h_out, 0);
        return 0;
    }
    io->read(e->user, f, e->mem(e->user, buf), size);
    io->close(e->user, f);
    return buf;
}

static void line(EnOutlineEnv* e, void* user, uint8_t* p, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    uint16_t v[4] = { (uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1 };
    memcpy(p + 8, v, 8);
    e->add_prim(user, p);
}

void en_viewport_outline(EnOutlineEnv* e, void* user, uint8_t r, uint8_t g, uint8_t b) {
    if (!e->split(user)) { e->clear_color(user, r, g, b); return; }
    int32_t v[4];
    e->viewport(user, v);
    if (v[0] == 0) return;
    uint8_t p[16] = { 0 };
    p[4] = r; p[5] = g; p[6] = b; p[7] = 0x40;
    line(e, user, p, v[0], v[1], v[2], v[1]);
    line(e, user, p, v[2], v[1], v[2], v[3]);
    line(e, user, p, v[0], v[3], v[2], v[3]);
    line(e, user, p, v[0], v[1], v[0], v[3]);
}
