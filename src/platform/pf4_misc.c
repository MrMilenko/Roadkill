/*
 * pf4_misc.c: small sound / CD / runtime helpers of TM2.EXE below 0x004bb000 (docs/PLATFORM.md Parts
 * A / C), ported over Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf4_misc.sh.
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32

/* 0x004bae50 group_alloc_voice(eax group, edx id): id != -1 and a voice already has it (id table
   +8): that voice. Else the first voice none of whose samples is playing (dsbuf_get_status
   0x004b999c on each sample's buffer, DSBSTATUS_PLAYING bit) gets the id and is returned; none
   free: id -1 -> voice 0, else 0. ORIGINAL BUG: a voice with no samples is judged by the status
   left by the previous voice (an unset local for voice 0; not tested). */
uint32_t pf4_group_alloc_voice(Pf2Env* e, uint32_t g, uint32_t id) {
    if (id != 0xffffffffu && (int32_t)R32(e, g) > 0) {
        uint32_t p = R32(e, g + 8);
        for (uint32_t i = 0;; i++, p += 4) {
            if (id == R32(e, p)) return i * 24 + R32(e, g + 4);
            if ((int32_t)(i + 1) >= (int32_t)R32(e, g)) break;
        }
    }
    uint32_t st = e->scratch;
    for (uint32_t i = 0; (int32_t)i < (int32_t)R32(e, g); i++) {
        uint32_t v = R32(e, g + 4) + 0x18 * i;
        for (uint32_t j = 0; (int32_t)j < (int32_t)R32(e, v + 0x10); j++) {
            pf2_call3(e, 0x004b999cu, 0x00c850ecu, R32(e, R32(e, v + 0x14) + 0x44 * j + 0x40), st);
            W32(e, st, (pf2_r8(e, st) & 1) != 0);
            if (R32(e, st) == 1) break;
        }
        if (R32(e, st) == 0) {
            W32(e, R32(e, g + 8) + 4 * i, id);
            return R32(e, g + 4) + 0x18 * i;
        }
    }
    return id == 0xffffffffu ? R32(e, g + 4) : 0;
}

/* 0x004b84a0 sound_settings_defaults: the default sound setting block 0x00c85060..0x00c850b8 */
void pf4_sound_settings_defaults(Pf2Env* e) {
    static const uint32_t v[][2] = {
        {0xc85084, 0x7fff}, {0xc85090, 0x7fff}, {0xc850a4, 0x7f}, {0xc850b4, 0x7f}, {0xc85078, 0x7fff},
        {0xc8507c, 0}, {0xc85088, 0}, {0xc85094, 0}, {0xc850a8, 0}, {0xc850b8, 0}, {0xc85060, 0x18},
        {0xc85068, 0}, {0xc85070, 0x18}, {0xc850a0, 0}, {0xc850ac, 0x80}, {0xc850b0, 0x80},
        {0xc85080, 0x7fff}, {0xc8508c, 0x7fff}, {0xc85098, 0x7fff}, {0xc85074, 0x2d}, {0xc8509c, 0x63}};
    for (unsigned k = 0; k < sizeof v / sizeof v[0]; k++) W32(e, v[k][0], v[k][1]);
}

/* 0x004b8694 cd_close(eax player): if open: with the aux volume flag (+0x1c), the aux device
   (0x004b8eac(player, 1)) >= 0 gets back the volume saved at open (+0x0c); MCI_CLOSE (0x804, no
   flags, no parameters). Returns the player. */
uint32_t pf4_cd_close(Pf2Env* e, uint32_t cd) {
    if (R32(e, cd + 0x18) == 0) return cd;
    uint32_t vol = R32(e, cd + 0xc);
    if (R32(e, cd + 0x1c)) {
        uint32_t dev = pf2_call2(e, 0x004b8eacu, cd, 1);
        if ((int32_t)dev >= 0) pf2_call2(e, 0x00c90600u, dev, vol);
    }
    pf2_call4(e, 0x00c90614u, R32(e, cd), 0x804, 0, 0);
    return cd;
}

/* 0x004b90d0 sound_startup: dsound_init(0x00c850ec) (edx = 3, ignored by it and kept), sound level
   0x00c850e0 = 3; returns dsound_init's result */
uint32_t pf4_sound_startup(Pf2Env* e) {
    uint32_t r = pf2_call2(e, 0x004b94f4u, 0x00c850ecu, 3);
    W32(e, 0x00c850e0u, 3);
    return r;
}

/* 0x004b270c tex_pages_free: free(page[i] - 0x2000) for each texture page 0x00be0350 (count
   0x004e85d8, re-read each step); count = 0 */
void pf4_tex_pages_free(Pf2Env* e) {
    for (uint32_t i = 0; (int32_t)i < (int32_t)R32(e, 0x004e85d8u); i++)
        pf2_call1(e, 0x004bf76fu, R32(e, 0x00be0350u + 4 * i) - 0x2000);
    W32(e, 0x004e85d8u, 0);
}

/* 0x004b27e6 crt_sprintf(stack buffer, format, ...; cdecl): the runtime formatter 0x004c92ec(eax
   buffer, edx format, ebx &va (a local holding the address of the variable arguments), ecx the
   character writer 0x004b27d3); buffer[count] = 0; returns the count. va_ptr is the address of the
   first variable argument (passed in; on the original's stack). */
uint32_t pf4_crt_sprintf(Pf2Env* e, uint32_t buf, uint32_t fmt, uint32_t va_ptr) {
    uint32_t va = e->scratch;
    W32(e, va, va_ptr);
    uint32_t n = pf2_call4(e, 0x004c92ecu, buf, fmt, va, 0x004b27d3u);
    pf2_w8(e, buf + n, 0);
    return n;
}
