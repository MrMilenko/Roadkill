/*
 * pf8_sndplay.c: TM2.EXE 0x00485af0, an unlisted function (a vtable slot, pointer at 0x004e5794; the title
 * screen's scene reaches it): plays the object's sound. Ported over Pf2Env (pf2_env.h); emulator diff:
 * tools/emu/check_pf8_sndplay.sh.
 *
 * this +0x10 / +0x14 / +0x18 the sound arguments, +0x1c the sound object (its vtable at its own +0x1c):
 * slot 0 (eax sound, edx +0x10, ebx +0x14, ecx +0x18; stack 99, the play counter 0x004e8278 (then + 1), 0),
 * then 0x004b9270 (sfx_update) on the sound object. Without a sound object: returns this.
 */
#include "pf2_env.h"
#include "pf8_sndplay.h"

#define R32 pf2_r32

uint32_t pf8_00485af0(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)d; (void)b; (void)c;
    uint32_t o = R32(e, a + 0x1c);
    if (!o) return a;
    uint32_t n = R32(e, 0x004e8278u);
    pf2_w32(e, 0x004e8278u, n + 1);
    uint32_t v[7] = {o, R32(e, a + 0x10), R32(e, a + 0x14), R32(e, a + 0x18), 0x63, n, 0};
    e->call(e->user, R32(e, R32(e, o + 0x1c)), v, 7);
    uint32_t w[4] = {R32(e, a + 0x1c), v[1], v[2], v[3]};
    return e->call(e->user, 0x004b9270u, w, 4);
}
