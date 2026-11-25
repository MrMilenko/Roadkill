/* cb_fx_draw.c: see cb_fx_draw.h */
#include "combat/cb_fx_draw.h"

void cb_fx_particles_draw(const CbFxDrawEnv* e, uint32_t view) {
    uint8_t prim[CB_PRIM_SIZE + 8];
    e->prim_init(e->user, prim, 5);
    e->prim_vec(e->user, prim, 1, 0x14, 0x14, 0);
    e->prim_700(e->user, prim, 0);
    e->prim_6d4(e->user, prim, 0, 0);
    e->prim_color(e->user, prim, 0, 0xFF, 0xFF, 0xFF);
    for (int k = 0; k < 20; k++) {
        uint32_t a = 0x00500034 + 0x24 * (uint32_t)k;
        const uint8_t* p = wmem_ptr(e->mem, a, 0x24);
        if (w_i32(p, 0x1C) <= 0)
            continue;
        uint32_t f = p[0x18];
        e->prim_vec(e->user, prim, 0, w_i32(p, 0), w_i32(p, 4), w_i32(p, 8));
        const uint8_t* t = wmem_ptr(e->mem, w_u32(p, 0x20) + f * 8, 8);
        e->prim_uv(e->user, prim, t[0], t[1], t[2], t[3]);
        t = wmem_ptr(e->mem, w_u32(wmem_ptr(e->mem, a, 0x24), 0x20) + f * 8, 8);
        e->prim_page(e->user, prim, w_u16(t, 4));
        e->draw(e->user, view, prim);
    }
}

void cb_fx_sprites_draw(const CbFxDrawEnv* e, uint32_t view, int32_t mode) {
    uint8_t prim[CB_PRIM_SIZE + 8];
    e->prim_init(e->user, prim, 5);
    e->prim_6c0(e->user, prim, 1);
    e->particles_draw(e->user, view, mode);
    for (int k = 0; k < 150; k++) {
        uint32_t a = 0x00500304 + 0x30 * (uint32_t)k;
        uint8_t* s = wmem_ptr(e->mem, a, 0x30);
        int32_t frame = w_i8(s, 0x0A);
        if (frame < 0)
            continue;
        if (mode > 0 && frame == 1 && w_i16(s, 0x0C) == 0)
            continue;
        if (s[1]) {
            const uint8_t* p = wmem_ptr(e->mem, w_u32(s, 0x2C), 12);
            w_s32(s, 0x14, w_u32(p, 0));
            w_s32(s, 0x18, w_u32(wmem_ptr(e->mem, w_u32(s, 0x2C), 12), 4));
            w_s32(s, 0x1C, w_u32(wmem_ptr(e->mem, w_u32(s, 0x2C), 12), 8));
        }
        int32_t loops = w_i8(s, 6);
        if (loops == 1)
            e->prim_6d4(e->user, prim, 0, -8);
        else if (loops == 8)
            e->prim_6d4(e->user, prim, 1, 8);
        else
            e->prim_6d4(e->user, prim, 0, 0);
        if (s[2])
            e->prim_color(e->user, prim, 0, s[3], s[4], s[5]);
        int32_t q = w_i8(s, 0x0A) / (int32_t)s[9];
        const uint8_t* t = wmem_ptr(e->mem, w_u32(s, 0x10) + (uint32_t)q * 8, 8);
        e->prim_uv(e->user, prim, t[0], t[1], t[2], t[3]);
        e->prim_page(e->user, prim, w_u16(t, 4));
        int32_t size = (int32_t)s[8] * (int32_t)wmem_ptr(e->mem, w_u32(s, 0x10), 8)[2] / 8;
        e->prim_vec(e->user, prim, 0, w_i32(s, 0x14), w_i32(s, 0x18), w_i32(s, 0x1C));
        e->prim_vec(e->user, prim, 1, size, size, 0);
        e->prim_700(e->user, prim, s[0] == 1);
        e->draw(e->user, view, prim);
    }
    uint8_t* blink = wmem_ptr(e->mem, 0x00501f24, 1);
    *blink = (uint8_t)(1 - *blink);
}
