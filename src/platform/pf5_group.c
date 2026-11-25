/*
 * pf5_group.c: group_read, TM2.EXE 0x004bb03c (see pf5_group.h).
 */
#include "pf5_group.h"

uint32_t pf5_group_read(Pf2Env* e, uint32_t group, uint32_t fd, uint32_t voices, uint32_t samples) {
    uint32_t total = 0;
    uint32_t r = pf2_call3(e, 0x4c9ff0, fd, group, 4);
    if (r != 0xffffffffu) total = r;
    pf2_w32(e, group + 8, pf2_call1(e, 0x4c4508, pf2_r32(e, group) << 2));
    pf2_w32(e, group + 4, voices);
    total += pf2_call3(e, 0x4bac7c, voices, fd, samples);
    uint32_t cursor = samples;
    for (int32_t k = 1; k < (int32_t)pf2_r32(e, group); k++) {
        uint32_t src = pf2_r32(e, group + 4);
        cursor += pf2_r32(e, src + 0x10) * 0x44u;
        uint32_t dst = src + 0x18u * (uint32_t)k;
        for (uint32_t o = 0; o < 0x14; o += 4) pf2_w32(e, dst + o, pf2_r32(e, src + o));
        pf2_w32(e, dst + 0x14, cursor);
        for (int32_t j = 0; j < (int32_t)pf2_r32(e, dst + 0x10); j++) {
            uint32_t a = pf2_r32(e, src + 0x14) + 0x44u * (uint32_t)j;
            uint32_t b = pf2_r32(e, dst + 0x14) + 0x44u * (uint32_t)j;
            for (uint32_t o = 4; o <= 0x20; o += 4) pf2_w32(e, b + o, pf2_r32(e, a + o));
            pf2_w32(e, b + 0x28, pf2_r32(e, a + 0x28));
            pf2_w32(e, b, pf2_r32(e, a));
        }
    }
    return total;
}
