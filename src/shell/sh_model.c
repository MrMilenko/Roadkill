/*
 * sh_model.c: car model display object update (see sh_model.h).
 */
#include "sh_model.h"
#include "../math/tm2math.h"

static uint32_t call(const ShEnv* e, uint32_t fn, int n, uint32_t a0, uint32_t a1, uint32_t a2) {
    uint32_t a[3] = {a0, a1, a2};
    return e->call ? e->call(e->user, fn, n, a) : 0;
}

void sh_model_display_update(const ShEnv* e, uint32_t o, uint32_t list) {
    if ((int16_t)sh_r16(e, o + 0xc) == 0) return;
    if (sh_r32(e, o + 0x18) == 0) {
        sh_w32(e, o + 0x18, call(e, 0x00492d90u, 1, o, 0, 0));          /* entity_alloc */
        uint32_t v = call(e, 0x00495864u, 1, 0, 0, 0);                    /* damage variant 0 */
        sh_w32(e, sh_r32(e, o + 0x18) + 4, v);
        call(e, 0x00492de4u, 2, sh_r32(e, o + 0x18), o + 0x50, 0);       /* entity init */
    }
    for (uint32_t k = 0; k < 16; k++) {
        uint32_t p = o + 0x18 * k;
        if ((int32_t)k < (int32_t)sh_r8(e, o + 0x22c)) {
            sh_w8(e, p + 0xbe, 1);
            sh_w32(e, p + 0xc0, sh_r32(e, o + 4));
        } else {
            sh_w8(e, p + 0xbe, 0);
            sh_w32(e, p + 0xc0, 0);
        }
    }
    sh_w32(e, sh_r32(e, o + 0x18) + 0xe8, sh_r32(e, o + 0x1c));
    sh_w32(e, sh_r32(e, o + 0x18) + 0xec, sh_r32(e, o + 0x20));
    sh_w32(e, sh_r32(e, o + 0x18) + 0xf0, sh_r32(e, o + 0x24));
    sh_w16(e, sh_r32(e, o + 0x18) + 0xe0, sh_r16(e, o + 0x2c));
    sh_w16(e, sh_r32(e, o + 0x18) + 0xe2, sh_r16(e, o + 0x30));
    sh_w16(e, sh_r32(e, o + 0x18) + 0xe4, sh_r16(e, o + 0x28));
    uint32_t ent = sh_r32(e, o + 0x18);
    int16_t ang[3] = {(int16_t)sh_r16(e, ent + 0xe0), (int16_t)sh_r16(e, ent + 0xe2), (int16_t)sh_r16(e, ent + 0xe4)};
    int32_t m[9];
    tm2_build_matrix(ang, m);                                            /* 0x0048c360 */
    for (int k = 0; k < 9; k++) sh_w32(e, ent + 0xb0 + 4 * (uint32_t)k, (uint32_t)m[k]);
    if ((int16_t)sh_r16(e, o + 0xe) != 0) {
        call(e, 0x004948b4u, 1, 0, 0, 0);
    } else {
        int32_t id = (int16_t)sh_r16(e, o + 0x10);
        if (id != -1) sh_w32(e, sh_r32(e, o + 0x18), (uint32_t)id);
        int32_t z = (int32_t)sh_r32(e, o + 0x14), d;
        if (z < 0) {
            uint32_t a = (uint32_t)(z < 0 ? -(int64_t)z : z);     /* |INT_MIN| stays INT_MIN, as cdq/xor/sub */
            d = (int32_t)a == INT32_MIN ? 0 : 8 / (int32_t)a;
        } else {
            d = (int32_t)((uint32_t)z * 8u);
        }
        call(e, 0x00494ec4u, 3, 0, (uint32_t)d, 1);
    }
    call(e, 0x004a10fcu, 3, list, sh_r32(e, o + 0x18), 0);
}
