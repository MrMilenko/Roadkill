/*
 * pe.c: read only view of a 32 bit PE executable (see pe.h).
 */
#include "pe.h"

static uint32_t rd16(const uint8_t* p) { return (uint32_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

int pe_open(PeImage* pe, const uint8_t* d, size_t n) {
    pe->data = d; pe->size = n; pe->nsec = 0;
    if (n < 0x40 || d[0] != 'M' || d[1] != 'Z') return -1;
    uint32_t h = rd32(d + 0x3c);
    if ((size_t)h + 24 > n || d[h] != 'P' || d[h + 1] != 'E' || d[h + 2] || d[h + 3]) return -1;
    if (rd16(d + h + 4) != 0x14c) return -1;                       /* i386 */
    uint32_t nsec = rd16(d + h + 6), opt = rd16(d + h + 20);
    if ((size_t)h + 24 + opt > n || opt < 32 || nsec > 16) return -1;
    pe->image_base = rd32(d + h + 24 + 28);
    for (uint32_t i = 0; i < nsec; i++) {
        size_t o = (size_t)h + 24 + opt + 40 * i;
        if (o + 40 > n) return -1;
        pe->sec[i].va = pe->image_base + rd32(d + o + 12);
        pe->sec[i].raw_size = rd32(d + o + 16);
        pe->sec[i].raw_off = rd32(d + o + 20);
        if (pe->sec[i].raw_off == 0) pe->sec[i].raw_size = 0;     /* uninitialized (.bss): no file bytes */
        if ((size_t)pe->sec[i].raw_off + pe->sec[i].raw_size > n) return -1;
    }
    pe->nsec = (int)nsec;
    return 0;
}

const uint8_t* pe_at(const PeImage* pe, uint32_t va, size_t len) {
    for (int i = 0; i < pe->nsec; i++) {
        uint32_t v = pe->sec[i].va, s = pe->sec[i].raw_size;
        if (va >= v && va - v < s && len <= s - (va - v)) return pe->data + pe->sec[i].raw_off + (va - v);
    }
    return 0;
}
