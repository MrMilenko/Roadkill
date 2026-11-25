/*
 * gp4_font.c: font setup, TM2.EXE 0x0045289c (see gp4_font.h).
 */
#include "gp4_font.h"

#include <string.h>

static void wr32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }
static void wr16(uint8_t* p, uint16_t v) { memcpy(p, &v, 2); }
static uint32_t rdu(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }

#define G(a) (e->g + ((a) - 0x005024d0u))

uint32_t gp4_font_init(const Gp4FontEnv* e) {
    wr32(G(0x5024d0), 0);
    wr32(G(0x5024d4), 0);
    wr32(G(0x502804), 0x4e7206);
    wr32(G(0x502800), 0x4e7206);
    wr32(G(0x502808), 0x4e6ffc);
    wr32(G(0x5025e8), 3);
    wr32(G(0x5027f8), 0x4e6c60);
    wr32(G(0x5025d8), 0xffffffffu);
    wr32(G(0x5027fc), 0x4e6c60);
    wr32(G(0x5025dc), 0xffffffffu);
    wr32(G(0x50280c), 0x4e6c60);
    wr32(G(0x5025e4), 0);
    wr32(G(0x502814), 0x4e7038);
    wr32(G(0x5025f4), 1);
    wr32(G(0x5025e0), 0);
    wr32(G(0x5025f0), 0xffffffffu);
    wr32(G(0x5025ec), 7);
    wr32(G(0x502810), 0x4e6e2e);
    static const uint8_t over[7] = {0xed, 0xf6, 0xf0, 0xf5, 0xf5, 0xf9, 0xee};
    for (int i = 0; i < 8; i++) {
        uint32_t dh = e->desc[i];
        uint8_t* d = e->mem(e->user, dh);
        if (rdu(d + 4) == 0) {
            uint32_t h = e->call(e->user, 0x4960bc, rdu(d), dh + 0xc, dh + 0x10);
            d = e->mem(e->user, dh);
            wr32(d + 8, h);
            wr32(e->mem(e->user, dh) + 4, 1);
        }
        uint8_t* r = G(0x502518) + 0x18 * i;
        r[7] = 0x7c;
        uint32_t p = (uint32_t)(0x1ff - e->page_of[i]);
        wr32(r + 0x14, p);
        r[5] = 0x20;
        r[0x10] = r[0x11] = r[0x12] = r[0x13] = 0;
        d = e->mem(e->user, e->desc[i]);
        uint8_t* t = e->pages + (uint32_t)(p * 0x30u);
        wr32(t + 0x00, rdu(d + 8));
        wr32(t + 0x04, rdu(d + 0xc));
        wr32(t + 0x08, rdu(d + 0x10));
        wr32(t + 0x0c, 0x5025f8);
        wr32(t + 0x10, 1);
        wr32(t + 0x20, 1);
        wr32(t + 0x28, 1);
        wr32(t + 0x2c, 0);
        *G(0x502818 + i) = e->mem(e->user, e->desc[i])[0x14];
        if (i < 7) *G(0x502818 + i) = over[i];
    }
    for (int k = 0; k < 256; k++) wr16(G(0x5025f8) + 2 * k, 0);
    static const uint8_t rgb[27][3] = {
        {0xfc, 0x90, 0xb8}, {0xff, 0xff, 0}, {0, 0, 0}, {0, 0xfc, 0}, {0, 0, 0xfc},
        {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8},
        {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8},
        {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8},
        {100, 0xff, 100}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8}, {0xfc, 0x90, 0xb8},
        {0, 0, 0}};
    uint32_t c = 0;
    for (int k = 0; k < 26; k++) {
        c = e->call(e->user, 0x496590, rgb[k][0], rgb[k][1], rgb[k][2]);
        wr16(G(0x5027c4) + 2 * k, (uint16_t)c);
    }
    return c;
}
