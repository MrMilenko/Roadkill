/*
 * tpc.c: TPC texture page registration, ported from TM2.EXE 0x00496fd8 (see tpc.h).
 */
#include "tpc.h"

#include <string.h>

static uint32_t rd32(const uint8_t* b, size_t o) { return b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24; }

int tpc_register(TpcTable* t, uint8_t* buf, size_t size, uint32_t addr_base, int append, int convert_565) {
    if (!append) {
        /* original: FUN_004b270c(), clear the used entries, zero the counters */
        for (int32_t i = 0; i < t->page_count && i < TPC_MAX_PAGES; i++) memset(&t->pages[i], 0, sizeof t->pages[i]);
        t->shared_count = 0;
        t->counter_a = 0;
        t->next_page = 0;
        t->page_count = 0;
    }
    if (size < 16 || rd32(buf, 0) != 0x4d504354u) return -1;
    if (rd32(buf, 4) != 0x43) return -1;
    int32_t id = (int32_t)rd32(buf, 8);
    int32_t count = (int32_t)rd32(buf, 12);
    int32_t first = t->next_page;
    if (count < 0 || first + count > TPC_MAX_PAGES) return -2;
    t->page_count += count;

    size_t c = 16;
    for (int32_t i = first; i < first + count; i++) {
        if (c + 0x1c > size) return -3;
        int32_t csize = (int32_t)rd32(buf, c);
        uint32_t flags = rd32(buf, c + 8);
        TpcPage* p = &t->pages[i];
        uint32_t dims;
        p->depth = flags & 3;
        if ((flags & 8) == 0) {
            p->pixels = addr_base + (uint32_t)(c + 0x18);
            p->clut = 0;
            p->link = rd32(buf, c + 0xc) + (uint32_t)first;
            dims = rd32(buf, c + 0x18);
            p->has_clut = 0;
        } else {
            size_t off = rd32(buf, c + 0xc) & ~3u;
            if (c + off + 0x18 > size) return -3;
            p->pixels = addr_base + (uint32_t)(c + off + 0x18);
            dims = rd32(buf, c + off + 0x14);
            p->link = (uint32_t)i;
            p->clut = addr_base + (uint32_t)(c + 0x18);
            p->has_clut = 1;
        }
        p->width = (dims * 2) & 0xFFFF;
        p->height = dims >> 16;
        p->flag10 = (flags & 0x10) != 0;
        if ((flags & 0x20) == 0) p->owner = 1;
        else { t->shared_count++; p->owner = 0; }
        p->flag40 = flags & 0x40;
        p->flag80 = flags & 0x80;
        c += 4 + (size_t)((csize / 4) * 4);     /* signed division, as the original */
    }

    /* palette ownership moves to the page that holds the palette */
    for (int32_t i = first; i < first + count; i++) {
        TpcPage* p = &t->pages[i];
        p->clut_moved = 0;
        if (p->owner == 1 && p->has_clut != 1) {
            if (p->link >= TPC_MAX_PAGES) return -3;
            t->pages[p->link].owner = 1;
            p->owner = 0;
            p->clut_moved = 1;
        }
    }
    /* 555 -> 565 palette conversion in place */
    for (int32_t i = first; i < first + count; i++) {
        TpcPage* p = &t->pages[i];
        if (p->has_clut != 1) continue;
        p->clut_moved = 0;
        if (convert_565) {
            size_t o = p->clut - addr_base;
            if (o + 512 > size) return -3;
            for (int k = 0; k < 256; k++) {
                uint16_t v = (uint16_t)(buf[o + 2 * k] | buf[o + 2 * k + 1] << 8);
                v = (uint16_t)((v & 0x1f) | (v & 0x7fe0) * 2);
                buf[o + 2 * k] = (uint8_t)v;
                buf[o + 2 * k + 1] = (uint8_t)(v >> 8);
            }
            p->depth = 9;
        }
    }
    /* pages without a palette use their link's */
    for (int32_t i = first; i < first + count; i++) {
        TpcPage* p = &t->pages[i];
        if (p->clut == 0) {
            if (p->link >= TPC_MAX_PAGES) return -3;
            p->clut = t->pages[p->link].clut;
        }
    }
    t->next_page = first + count;
    return append ? first : id;
}

int tpc_page_rgba(const TpcPage* p, const uint8_t* buf, size_t size, uint32_t addr_base,
                  uint8_t* out, uint8_t* stp_mask) {
    if (p->depth != 1 || p->clut == 0) return -1;
    size_t px = p->pixels - addr_base, cl = p->clut - addr_base, n = (size_t)p->width * p->height;
    if (px > size || size - px < n || cl > size || size - cl < 512) return -1;
    for (size_t i = 0; i < n; i++) {
        uint8_t k = buf[px + i];
        uint16_t c = (uint16_t)(buf[cl + 2 * k] | buf[cl + 2 * k + 1] << 8);
        out[4 * i + 0] = (uint8_t)(((c >> 10) & 0x1f) << 3);
        out[4 * i + 1] = (uint8_t)(((c >> 5) & 0x1f) << 3);
        out[4 * i + 2] = (uint8_t)((c & 0x1f) << 3);
        out[4 * i + 3] = c == 0 ? 0 : 255;
        if (stp_mask) stp_mask[i] = (uint8_t)(c >> 15);
    }
    return 0;
}
