/*
 * en3_small.c: small engine functions of TM2.EXE 0x00488000..0x00490000 (see en3_small.h).
 * Verified under emulation: tools/emu/check_en3_small.sh.
 */
#include "en3_small.h"

uint32_t en3_collide_set_9f7880(En2Env* e, uint32_t v)
{
    en2_w32(e, 0x009f7880u, v);
    en2_w8(e, 0x009f7890u, 0);
    return v & 0xffff00ffu;
}

uint32_t en3_math_create_vector(En2Env* e, uint32_t angle, uint32_t out)
{
    const uint32_t F = e->scratch;   /* F + 0: sin, F + 4: cos */
    en2_call3(e, 0x0048b8a0u, (uint32_t)(int32_t)(int16_t)angle, F + 4, F);
    en2_w32(e, out, 0x1000);
    en2_w32(e, out + 4, 0);
    en2_w32(e, out + 8, 0);
    en2_w32(e, out + 0xc, 0);
    en2_w32(e, out + 0x10, en2_r32(e, F + 4));
    en2_w32(e, out + 0x18, 0);
    en2_w32(e, out + 0x14, en2_r32(e, F));
    en2_w32(e, out + 0x1c, 0u - en2_r32(e, F));
    en2_w32(e, out + 0x20, en2_r32(e, F + 4));
    return out;
}

uint32_t en3_dpc_collide_entry(En2Env* e, uint32_t point, uint32_t out)
{
    en2_w32(e, 0x009f7820u, 0);
    en2_w32(e, 0x009f7830u, 0);
    en2_w32(e, 0x009f7888u, 0);
    en2_w16(e, 0x009f7828u, 0xffff);
    en2_w16(e, 0x009f782au, 0xffff);
    en2_w32(e, out, 0);
    uint32_t root = en2_call0(e, 0x00492c44u);
    if (en2_r32(e, root + 0x15c) != 0) {
        unsigned s = en2_r8(e, 0x004e8288u) & 31;
        int32_t x = (int32_t)en2_r32(e, point) >> s;
        int32_t y = (int32_t)en2_r32(e, point + 4) >> s;
        int32_t z = (int32_t)en2_r32(e, point + 8) >> s;
        if (en2_call3(e, 0x004bca88u, (uint32_t)x, (uint32_t)y, (uint32_t)z) != 0) {
            en2_w32(e, 0x009f7820u, 2);
        } else {
            en2_call3(e, 0x0048d0b8u, root, point, 0x009f7820u);
            if (en2_r32(e, 0x009f7820u) == 1) en2_w32(e, out, en2_r32(e, root + 0x158));
        }
    }
    return 0x009f7820u;
}

static int32_t plane_dist(En2Env* e, uint32_t pl, uint32_t p)
{
    uint32_t nx = (uint32_t)(int32_t)(int16_t)en2_r16(e, pl), ny = (uint32_t)(int32_t)(int16_t)en2_r16(e, pl + 2);
    uint32_t nz = (uint32_t)(int32_t)(int16_t)en2_r16(e, pl + 4);
    uint32_t s = ny * en2_r32(e, p + 4) + nx * en2_r32(e, p) + nz * en2_r32(e, p + 8);
    return (int32_t)((uint32_t)((int32_t)s >> 12) + (uint32_t)(int32_t)(int16_t)en2_r16(e, pl + 6));
}

int32_t en3_sphere_bands(En2Env* e, uint32_t cell, uint32_t point, uint32_t hit, uint32_t shift)
{
    const uint32_t F = e->scratch;   /* F + 0: plane A, F + 8: plane B, F + 0x10 / 0x14: hit, shift */
    en2_w32(e, F + 0x10, hit);
    en2_w32(e, F + 0x14, shift);
    int32_t d = 0;
    {
        uint32_t v[5] = {cell, point, shift, F + 8, F};
        if (e->call(e->user, 0x004bcb70u, v, 5) == 0) return 0;
    }
    d = plane_dist(e, F + 8, point);
    if (en2_r8(e, en2_r32(e, cell + 4) + 0x12) & 1) {
        if (d >= 0) d = plane_dist(e, F, point);
    }
    if (d < 0 && d > (int32_t)(0u - (50u << (shift & 31)))) {
        en2_w32(e, hit, 1);
        en2_w32(e, hit + 0x10, (uint32_t)d);
    }
    return d;
}

void en3_audio_focus_label(En2Env* e, uint32_t cfg, uint32_t w)
{
    uint32_t t = 0;
    if (w == cfg + 0x48c) t = cfg + 0x9c;
    else if (w == cfg + 0x658) t = cfg + 0x118;
    else if (w == cfg + 0x824) t = cfg + 0x16c;
    else if (w == cfg + 0x9f0) t = cfg + 0x1c0;
    else if (w == cfg + 0xbbc) t = en2_r32(e, cfg + 0x1120) ? cfg + 0x214 : cfg + 0x16c;
    else if (w == cfg + 0xd88) t = en2_r32(e, cfg + 0x1120) ? cfg + 0x268 : cfg + 0x16c;
    if (t) en2_call1(e, 0x004a6f80u, t);
}
