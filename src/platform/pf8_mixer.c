/*
 * pf8_mixer.c: TM2.EXE 0x00485ebc, an unlisted function (boot path: the static constructor 0x00473da0
 * builds the global 0x00624edc with it): the audio settings screen object's constructor, which also reads
 * the mixer's speaker and wave volumes. Ported over Pf2Env (pf2_env.h); emulator diff:
 * tools/emu/check_pf8_mixer.sh. Calls get the four registers as the original has them.
 *
 * this: members built by chained constructors (each on the previous one's returned eax): 0x004a6ac8,
 * +0x9c 0x004a5a50, 5 x 0x004a65d0 (+0x7c, then by 0x54), 8 x 0x004a4838 (by 0x1d0 / 0x1cc), 0x004ac4b0;
 * vtable +0x34 = 0x004e57f4, +0x1124 = 0, +0x38 / +0x3c / +0x40 = 0x004890c4(this, 4 / 0x1008 / 0x1005).
 * With the mixer open (+0x1120, handle +0x1128): +0x44 / +0x48 = the volume control value of the
 * speakers (MIXERLINE_COMPONENTTYPE_DST_SPEAKERS 4, control 0x50030002) and of the wave out line (4,
 * 0x50030003): mixerGetLineInfo (COMPONENTTYPE) -> mixerGetLineControls (ONEBYTYPE) ->
 * mixerGetControlDetails; a failure closes the mixer flag (+0x1120 = 0) and gives 0. Only the structure
 * fields named are set (the rest is stack garbage in the original).
 */
#include "pf2_env.h"
#include "pf8_mixer.h"

#define R32 pf2_r32
#define W32 pf2_w32

typedef struct { Pf2Env* e; uint32_t a, d, b, c; } Regs;
static uint32_t call4(Regs* r, uint32_t fn) {
    uint32_t v[4] = {r->a, r->d, r->b, r->c};
    return r->a = r->e->call(r->e->user, fn, v, 4);
}

#define I_mixerGetLineInfoA 0x00c90628u
#define I_mixerGetLineControlsA 0x00c90624u
#define I_mixerGetControlDetailsA 0x00c9061cu

/* one line's volume: 0 on a failure (and the mixer flag cleared) */
static uint32_t volume(Pf2Env* e, uint32_t self, uint32_t ml, uint32_t mlc, uint32_t mc, uint32_t mcd,
                       uint32_t val, uint32_t ctype, int* failed_controls) {
    uint32_t hmx = R32(e, self + 0x1128);
    W32(e, ml, 0xa8);
    W32(e, ml + 0x18, 4);
    if (pf2_call3(e, I_mixerGetLineInfoA, hmx, ml, 3)) { W32(e, self + 0x1120, 0); return 0; }
    W32(e, mlc + 4, R32(e, ml + 0xc));
    W32(e, mlc + 0xc, R32(e, ml + 0x24));
    W32(e, mlc + 0x10, 0x94);
    W32(e, mlc, 0x18);
    W32(e, mlc + 0x14, mc);
    W32(e, mlc + 8, ctype);
    if (pf2_call3(e, I_mixerGetLineControlsA, R32(e, self + 0x1128), mlc, 2)) {
        W32(e, self + 0x1120, 0);
        *failed_controls = 1;
        return 0;
    }
    W32(e, mcd, 0x18);
    W32(e, mcd + 0x10, 4);
    W32(e, mcd + 4, R32(e, mc + 4));
    W32(e, mcd + 8, R32(e, ml + 0x1c));
    W32(e, mcd + 0x14, val);
    W32(e, mcd + 0xc, 0);
    pf2_call3(e, I_mixerGetControlDetailsA, R32(e, self + 0x1128), mcd, 0);
    return R32(e, val);
}

uint32_t pf8_00485ebc(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    R.b = a;
    call4(&R, 0x004a6ac8u);
    R.a += 0x9c;
    call4(&R, 0x004a5a50u);
    R.a += 0x7c;
    call4(&R, 0x004a65d0u);
    for (int k = 0; k < 4; k++) { R.a += 0x54; call4(&R, 0x004a65d0u); }
    R.a += 0x54;
    call4(&R, 0x004a4838u);
    R.a += 0x1d0;
    call4(&R, 0x004a4838u);
    R.a += 0x1cc;
    call4(&R, 0x004a4838u);
    R.a += 0x1cc;
    call4(&R, 0x004a4838u);
    R.b = R.a - 0x824;
    R.a = R.b + 0x9f0;
    call4(&R, 0x004a4838u);
    R.a += 0x1cc;
    call4(&R, 0x004a4838u);
    R.a += 0x1cc;
    call4(&R, 0x004a4838u);
    R.a += 0x1cc;
    R.d = 0;
    call4(&R, 0x004a4838u);
    R.a += 0x1d8;
    call4(&R, 0x004ac4b0u);
    uint32_t s = R.b = R.a - 0x112c;
    R.d = 4;
    W32(e, s + 0x34, 0x004e57f4u);
    R.a = s;
    W32(e, s + 0x1124, 0);
    W32(e, s + 0x38, call4(&R, 0x004890c4u));
    R.d = 0x1008;
    R.a = s;
    W32(e, s + 0x3c, call4(&R, 0x004890c4u));
    R.d = 0x1005;
    R.a = s;
    W32(e, s + 0x40, call4(&R, 0x004890c4u));
    if (R32(e, s + 0x1120) == 0) {
        W32(e, s + 0x48, 0);
        W32(e, s + 0x44, 0);
        return s;
    }
    uint32_t f = e->scratch + 0x200;        /* the original's 0x2e8 byte frame, same offsets */
    int fc = 0;
    W32(e, s + 0x44, volume(e, s, f, f + 0x2c0, f + 0x1e4, f + 0x278, f + 0x2d8, 0x50030002u, &fc));
    fc = 0;
    W32(e, s + 0x48, volume(e, s, f + 0xa8, f + 0x2a8, f + 0x150, f + 0x290, f + 0x2e0, 0x50030003u, &fc));
    return s;
}
