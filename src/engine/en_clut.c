/*
 * en_clut.c: palette (CLUT) animations, ported from TM2.EXE 0x00499984..0x0049a073
 * (see en_clut.h). All state is addressed in the game's address space through env->mem().
 */
#include "en_clut.h"

#include <string.h>

#define NDEFS 25
#define DEF_SIZE 0x18
#define NSLOTS 15
#define SLOT_SIZE 0x228

static int32_t r32(EnClutEnv* e, uint32_t a) { int32_t v; memcpy(&v, e->mem(e->user, a), 4); return v; }
static void w32(EnClutEnv* e, uint32_t a, int32_t v) { memcpy(e->mem(e->user, a), &v, 4); }
static uint16_t r16(EnClutEnv* e, uint32_t a) { uint16_t v; memcpy(&v, e->mem(e->user, a), 2); return v; }
static void w16(EnClutEnv* e, uint32_t a, uint16_t v) { memcpy(e->mem(e->user, a), &v, 2); }
static uint32_t page_at(EnClutEnv* e, uint32_t def) { return e->pages + (uint32_t)r32(e, def + 0x14) * 0x30u; }

uint32_t en_clut_new_def(EnClutEnv* e, uint32_t a, uint32_t b) {
    int32_t last = e->last;
    uint32_t r = 0;
    int dup = 0;
    if (last >= 0) {
        int32_t end = last * DEF_SIZE;
        for (int32_t off = 0; off <= end; off += DEF_SIZE)
            if (r16(e, e->defs + (uint32_t)off) == a && r16(e, e->defs + (uint32_t)off + 2) == b) { dup = 1; break; }
    }
    if (!dup) {
        /* ORIGINAL BUG: the index grows on every call once the table is full (25, 26, ...), and
         * later searches then run past the 25 entries into the slot table */
        last++;
        if (last < NDEFS) r = e->defs + (uint32_t)last * DEF_SIZE;
    }
    e->last = last;
    return r;
}

uint32_t en_clut_find_def(EnClutEnv* e, uint32_t a, uint32_t b) {
    if (e->last < 0) return 0;
    int32_t end = e->last * DEF_SIZE;
    for (int32_t off = 0; off <= end; off += DEF_SIZE)
        if (r16(e, e->defs + (uint32_t)off) == a && r16(e, e->defs + (uint32_t)off + 2) == b)
            return e->defs + (uint32_t)off;
    return 0;
}

uint32_t en_clut_find_slot(EnClutEnv* e, uint32_t a, uint32_t b) {
    for (uint32_t i = 0; i < NSLOTS; i++) {
        uint32_t s = e->slots + i * SLOT_SIZE;
        if (r32(e, s) == 0) continue;
        uint32_t d = (uint32_t)r32(e, s + 0x20);
        if (r16(e, d) == a && r16(e, d + 2) == b) return s;
    }
    return 0;
}

uint32_t en_clut_free_slot(EnClutEnv* e) {
    for (uint32_t i = 0; i < NSLOTS; i++)
        if (r32(e, e->slots + i * SLOT_SIZE) == 0) return e->slots + i * SLOT_SIZE;
    return 0;
}

void en_clut_last_differing(EnClutEnv* e, uint32_t s) {
    uint32_t d = (uint32_t)r32(e, s + 0x20);
    uint32_t pal = (uint32_t)r32(e, s + 0x24);
    int32_t n = r32(e, d + 0x10);
    uint16_t lastc = r16(e, pal + (uint32_t)n * 2u - 2u);
    for (int32_t i = n - 2; i >= 0; i--)
        if (r16(e, pal + (uint32_t)i * 2u) != lastc) { w32(e, s + 0x1c, i); break; }
}

/* the common part of the three starts: slot fields, palette address and its copy */
static void copy_palette(EnClutEnv* e, uint32_t s, uint32_t d) {
    uint32_t pal = (uint32_t)r32(e, page_at(e, d) + 0xc);
    w32(e, s + 0x24, (int32_t)pal);
    for (int32_t i = 0; i < r32(e, d + 0x10); i++)
        w16(e, s + 0x28 + 2u * (uint32_t)i, r16(e, (uint32_t)r32(e, s + 0x24) + 2u * (uint32_t)i));
}

void en_clut_start_sweep(EnClutEnv* e, uint32_t a, uint32_t b, int32_t speed) {
    uint32_t d = en_clut_find_def(e, a, b);
    if (!d) return;
    uint32_t s = en_clut_free_slot(e);
    if (!s) return;
    w32(e, s + 0xc, 0); w32(e, s + 4, -1); w32(e, s + 8, -1); w32(e, s, 1);
    w32(e, s + 0x20, (int32_t)d); w32(e, s + 0x14, speed);
    copy_palette(e, s, d);
    w32(e, page_at(e, d) + 0x10, 1);
    en_clut_last_differing(e, s);
    w32(e, s + 0x10, r32(e, s + 0x1c) >> 2);
}

void en_clut_start_fade(EnClutEnv* e, uint32_t a, uint32_t b, int32_t divisor) {
    uint32_t d = en_clut_find_def(e, a, b);
    if (!d) return;
    uint32_t s = en_clut_free_slot(e);
    if (!s) return;
    w32(e, s + 0xc, 0); w32(e, s + 4, 0); w32(e, s, 1);
    w32(e, s + 0x20, (int32_t)d);
    copy_palette(e, s, d);
    w32(e, page_at(e, d) + 0x10, 1);
    en_clut_last_differing(e, s);
    w32(e, s + 0x14, r32(e, s + 0x1c) / divisor + 1);
}

void en_clut_stop_fade(EnClutEnv* e, uint32_t a, uint32_t b) {
    uint32_t s = en_clut_find_slot(e, a, b);
    if (!s) return;
    w32(e, s + 4, 0);
    w32(e, s + 0xc, 2);
}

void en_clut_start_cycle(EnClutEnv* e, uint32_t a, uint32_t b, int32_t speed) {
    uint32_t d = en_clut_find_def(e, a, b);
    if (!d) return;
    uint32_t s = en_clut_free_slot(e);
    if (!s) return;
    w32(e, s + 0xc, 0); w32(e, s + 4, -1); w32(e, s + 0x18, 0); w32(e, s, 1);
    w32(e, s + 0x14, speed); w32(e, s + 0x20, (int32_t)d);
    copy_palette(e, s, d);
    if (r32(e, d + 4) == 3) { w32(e, s + 0x1c, 7); return; }
    en_clut_last_differing(e, s);
}

void en_clut_register(EnClutEnv* e, uint32_t node, int32_t kind) {
    uint8_t type = *e->mem(e->user, node);
    if (type != 0) { if (e->bad_node) e->bad_node(e->user, type); return; }
    uint32_t d = en_clut_new_def(e, r16(e, node + 2), r16(e, node + 0x14));
    if (!d) return;
    uint32_t poly = (uint32_t)r32(e, node + 0xc);
    uint32_t w = (uint32_t)r32(e, poly + 0x10);
    uint32_t p = poly + 0x18 + 4u * (w << 1 >> 29) + 4u * (w << 13 >> 29);
    w32(e, d + 0x10, 0x100);
    w32(e, d + 0x14, r16(e, p) & 0x1ff);
    w16(e, d, r16(e, node + 2));
    w16(e, d + 2, r16(e, node + 0x14));
    w32(e, d + 4, kind);
    if (kind == 0) en_clut_start_cycle(e, r16(e, d), r16(e, d + 2), 2);
    if (r32(e, d + 4) == 3) en_clut_start_cycle(e, r16(e, d), r16(e, d + 2), 2);
    if (r32(e, d + 4) == 2) w16(e, d + 2, 0);
}

void en_clut_step_sweep(EnClutEnv* e, uint32_t s) {
    int32_t c = r32(e, s + 0xc) + 1;
    w32(e, s + 0xc, c);
    if (c <= r32(e, s + 0x14)) return;
    w32(e, s + 0xc, 0);
    int32_t pos = r32(e, s + 4) + 1;
    int32_t len = r32(e, s + 0x1c);
    w32(e, s + 4, pos);
    uint32_t pal = (uint32_t)r32(e, s + 0x24);
    if (pos >= len && len <= r32(e, s + 8)) {
        w32(e, s, 0);
    } else if (pos < r32(e, s + 0x10)) {
        w16(e, pal + 2u * (uint32_t)pos, 0x1042);
    } else if (len - 2 > pos) {
        int32_t t = r32(e, s + 8) + 1;
        w32(e, s + 8, t);
        w16(e, pal + 2u * (uint32_t)pos, 0x1042);
        w16(e, (uint32_t)r32(e, s + 0x24) + 2u * (uint32_t)r32(e, s + 8), 0);
    } else {
        int32_t t = r32(e, s + 8);
        if (t < len) {
            w32(e, s + 8, t + 1);
            w16(e, pal + 2u * (uint32_t)(t + 1), 0);
        }
    }
    w32(e, s + 0x18, r32(e, s + 0x18) == 0);
}

void en_clut_step_fade(EnClutEnv* e, uint32_t s) {
    int32_t m = r32(e, s + 0xc);
    if (m == 0) {
        for (int32_t k = 0; k < r32(e, s + 0x14); k++) {
            int32_t pos = r32(e, s + 4);
            if (pos < r32(e, s + 0x1c)) {
                w16(e, (uint32_t)r32(e, s + 0x24) + 2u * (uint32_t)pos, 0);
                w32(e, s + 4, pos + 1);
            } else {
                w32(e, s + 0xc, 1);
            }
        }
        if (r32(e, s + 0xc) == 1) w32(e, s + 4, 0);
    } else if (m == 2) {
        for (int32_t k = 0; k < r32(e, s + 0x14); k++) {
            int32_t pos = r32(e, s + 4);
            if (pos < r32(e, s + 0x1c)) {
                w16(e, (uint32_t)r32(e, s + 0x24) + 2u * (uint32_t)pos, r16(e, s + 0x28 + 2u * (uint32_t)pos));
                w32(e, s + 4, r32(e, s + 4) + 1);
            } else {
                w32(e, s, 0);
            }
        }
    }
}

void en_clut_step_cycle(EnClutEnv* e, uint32_t s) {
    int32_t c = r32(e, s + 0xc) + 1;
    w32(e, s + 0xc, c);
    if (c <= r32(e, s + 0x14)) return;
    if (r32(e, s + 0x18) != 0) {
        /* rotate the palette into the slot's copy and point the page at the copy */
        for (int32_t i = 0; i < r32(e, s + 0x1c); i++)
            w16(e, s + 0x2a + 2u * (uint32_t)i, r16(e, (uint32_t)r32(e, s + 0x24) + 2u * (uint32_t)i));
        w16(e, s + 0x28, r16(e, (uint32_t)r32(e, s + 0x24) + 2u * (uint32_t)r32(e, s + 0x1c)));
        w32(e, page_at(e, (uint32_t)r32(e, s + 0x20)) + 0xc, (int32_t)(s + 0x28));
    } else {
        /* rotate the copy back into the palette and point the page at the palette */
        for (int32_t i = 0; i < r32(e, s + 0x1c); i++)
            w16(e, (uint32_t)r32(e, s + 0x24) + 2u * (uint32_t)i + 2u, r16(e, s + 0x28 + 2u * (uint32_t)i));
        w16(e, (uint32_t)r32(e, s + 0x24), r16(e, s + 0x28 + 2u * (uint32_t)r32(e, s + 0x1c)));
        w32(e, page_at(e, (uint32_t)r32(e, s + 0x20)) + 0xc, r32(e, s + 0x24));
    }
    int32_t t = r32(e, s + 0x18) == 0;
    w32(e, s + 0xc, 0);
    w32(e, s + 0x18, t);
}

void en_clut_update(EnClutEnv* e) {
    for (uint32_t i = 0; i < NSLOTS; i++) {
        uint32_t s = e->slots + i * SLOT_SIZE;
        if (r32(e, s) == 0) continue;
        int32_t kind = r32(e, (uint32_t)r32(e, s + 0x20) + 4);
        if (kind == 0 || kind == 3) en_clut_step_cycle(e, s);
        else if (kind == 1) en_clut_step_sweep(e, s);
        else if (kind == 2) en_clut_step_fade(e, s);
    }
}
