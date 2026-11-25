/*
 * wmem.h: game memory as the weapon code sees it. The weapon and effect records keep the
 * original's byte layout, including 32 bit pointers between records (a missile's render object,
 * a sprite's owner position, frame tables in the executable's data). Those pointers stay game
 * addresses; WMem maps a game address range to the host buffer that holds it.
 *
 * Little endian field access helpers (the records are not aligned host structs).
 */
#ifndef TM2_WMEM_H
#define TM2_WMEM_H

#include <stddef.h>
#include <stdint.h>

#define WMEM_MAX_REGIONS 16

typedef struct {
    uint32_t addr;      /* game address of host[0] */
    uint32_t size;
    uint8_t* host;
} WMemRegion;

typedef struct {
    int n;
    WMemRegion r[WMEM_MAX_REGIONS];
} WMem;

/* host pointer of game address a (len bytes must lie in one region), NULL when unmapped */
static inline uint8_t* wmem_ptr(const WMem* m, uint32_t a, uint32_t len) {
    for (int i = 0; i < m->n; i++) {
        const WMemRegion* r = &m->r[i];
        if (a >= r->addr && a - r->addr <= r->size && len <= r->size - (a - r->addr))
            return r->host + (a - r->addr);
    }
    return NULL;
}

static inline uint8_t  w_u8(const uint8_t* p, int o) { return p[o]; }
static inline int8_t   w_i8(const uint8_t* p, int o) { return (int8_t)p[o]; }
static inline uint16_t w_u16(const uint8_t* p, int o) { return (uint16_t)(p[o] | (p[o + 1] << 8)); }
static inline int16_t  w_i16(const uint8_t* p, int o) { return (int16_t)w_u16(p, o); }
static inline uint32_t w_u32(const uint8_t* p, int o) {
    return (uint32_t)p[o] | ((uint32_t)p[o + 1] << 8) | ((uint32_t)p[o + 2] << 16) | ((uint32_t)p[o + 3] << 24);
}
static inline int32_t  w_i32(const uint8_t* p, int o) { return (int32_t)w_u32(p, o); }
static inline void w_s8(uint8_t* p, int o, uint32_t v) { p[o] = (uint8_t)v; }
static inline void w_s16(uint8_t* p, int o, uint32_t v) { p[o] = (uint8_t)v; p[o + 1] = (uint8_t)(v >> 8); }
static inline void w_s32(uint8_t* p, int o, uint32_t v) {
    p[o] = (uint8_t)v; p[o + 1] = (uint8_t)(v >> 8); p[o + 2] = (uint8_t)(v >> 16); p[o + 3] = (uint8_t)(v >> 24);
}

#endif /* TM2_WMEM_H */
