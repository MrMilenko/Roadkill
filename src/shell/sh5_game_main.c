/*
 * sh5_game_main.c: the game's start and end, TM2.EXE 0x00477124 (see sh5_game_main.h).
 * DECOMPILER TRAPS: several calls get registers left from the code before them, kept here as
 * the original passes them: the display init's third argument is the caller's ebx, the first
 * audio method gets its own vtable pointer as the second argument, the font file path the root
 * 5 left in ebx, bg_tim_load the root 1 in ebx.
 */
#include "sh5_game_main.h"

#define R32(a) sh_r32(e, a)
#define W8(a, v) sh_w8(e, a, v)
#define W32(a, v) sh_w32(e, a, v)
#define FB(o) (fb + (uint32_t)(o))
#define C0(f) sh3_call(e, f, 0, 0, 0, 0, 0, 0)
#define C1(f, a) sh3_call(e, f, 1, a, 0, 0, 0, 0)
#define C2(f, a, b) sh3_call(e, f, 2, a, b, 0, 0, 0)
#define C3(f, a, b, c) sh3_call(e, f, 3, a, b, c, 0, 0)
#define C4(f, a, b, c, d) sh3_call(e, f, 4, a, b, c, d, 0)
#define C5(f, a, b, c, d, x) sh3_call(e, f, 5, a, b, c, d, x)

#define AUDIO_CFG 0x00624edcu
#define DISPLAY 0x00c84840u

/* one intro picture; returns 1 when skipped (a button or the timeout check) */
static int intro_picture(const ShEnv* e, uint32_t fb, uint32_t name) {
    C3(0x00499408u, FB(0), name, 1);
    C4(0x0046f784u, FB(0), 0, 1, 0);            /* bg_tim_load */
    C2(0x0046f7c4u, 0, 0);                      /* bg_tim_draw */
    C0(0x00495b14u);
    uint32_t t0 = C0(0x00493038u);
    for (;;) {
        C0(0x00493748u);
        uint32_t d = C0(0x00493038u) - t0;
        if ((uint8_t)C1(0x00493694u, 0)) return 1;
        if ((uint8_t)C2(0x004936acu, 0x16, 0)) return 1;
        if ((uint8_t)C2(0x004936acu, 0x14, 0)) return 1;
        if (d >= 2000000) return 0;
    }
}

uint32_t sh5_game_main(const ShEnv* e, uint32_t fb, uint32_t ebx_in) {
    C1(0x00486870u, AUDIO_CFG);
    W32(C1(0x004bb1b0u, FB(0x384)) + 0x1c, 0x004e5700u);    /* display object constructed */
    W32(0x00626010u, FB(0x384));
    C1(0x004b92b0u, FB(0x3a4));                             /* audio object */
    W32(0x00626014u, FB(0x3a4));
    uint32_t r = C3(0x004b3d80u, DISPLAY, 0, ebx_in);
    if (r == 1) {
        C2(0x004bb1e4u, FB(0x384), 1);
        return 1;
    }
    C2(0x004b50f8u, DISPLAY, 0);
    r = C5(0x004b45a0u, DISPLAY, 0x280, 0x1e0, 0x10, 0);
    if (r) {
        C2(0x004bb1e4u, FB(0x384), 1);
        return r;
    }
    C1(0x0049e0c0u, 0);
    C2(0x004ac4b0u, FB(0x3b0), 0);
    C3(0x004ac550u, FB(0x3b0), 0x80000002u, 0);
    C1(0x004ac590u, FB(0x3b0));
    static const uint32_t inits[4] = {0x00495924u, 0x00496fb4u, 0x00492f30u, 0x00493444u};
    for (int k = 0; k < 4; k++) C0(inits[k]);
    uint32_t v = C0(0x0046cbf4u);
    /* 0x00498b2c(eax, edx) stores both: eax = 0x0046cbec's result, edx = v (left there by the
     * original's mov edx, eax before the 0x0046cbec call) */
    C2(0x00498b2cu, C2(0x0046cbecu, v, v), v);
    static const uint32_t inits2[5] = {0x0044e060u, 0x0046cbfcu, 0x004b84a0u, 0x004b7fa4u, 0x004b92c0u};
    for (int k = 0; k < 5; k++) C0(inits2[k]);
    C1(0x00472ec0u, R32(0x00626010u));
    {
        uint32_t au = R32(0x00626014u), vt = R32(au + 8);
        C3(R32(vt), au, vt, 5);
    }
    C0(0x0045289cu);
    C3(0x00499408u, FB(0x12c), 0x004e21f8u, 5);
    C2(0x004b9140u, R32(0x00626010u), FB(0x12c));
    static const uint32_t inits3[5] = {0x00495328u, 0x00492df4u, 0x00493aa4u, 0x004a1224u, 0x00457464u};
    for (int k = 0; k < 5; k++) C0(inits3[k]);
    W8(0x00624ec7u, 1);
    W8(0x00624ea9u, 0); W8(0x00624eaau, 0); W8(0x00624eabu, 0); W8(0x00624eacu, 0);
    W8(0x00624ec8u, 0); W8(0x00624ec9u, 0); W8(0x00624ecau, 0);
    W8(0x00624ed0u, 1);
    W8(0x00624edau, (uint8_t)R32(0x00c85098u));
    W8(0x00624ed1u, 1);
    W8(0x00624edbu, (uint8_t)R32(0x00c8508cu));
    for (uint32_t o = 0xc; o <= 0x60; o += 0xc) {
        W32(0x00624db0u + o, 0);
        W32(0x00624db4u + o, 0);
        W32(0x00624dacu + o, 0);
    }
    C1(0x004b9c7cu, 1);
    if (!intro_picture(e, fb, 0x004e27bfu) && !intro_picture(e, fb, 0x004e27d3u) &&
        !intro_picture(e, fb, 0x004e27e7u)) {
        C3(0x00499408u, FB(0x258), 0x004e27fau, 6);
        C1(0x004b9674u, 0x00c850ecu);
        {
            uint32_t a[6] = {FB(0x258), 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xf};
            if (e->call) e->call(e->user, 0x004ba15cu, 6, a);       /* the intro movie */
        }
        C3(0x004b94f4u, 0x00c850ecu, 0x004e2559u, 5);
        C3(0x00499408u, FB(0x258), 0x004e2559u, 5);
        C2(0x004b9140u, R32(0x00626010u), FB(0x258));
        C2(0x00486bb4u, AUDIO_CFG, 1);
    }
    C2(0x00486bb4u, AUDIO_CFG, 1);
    C0(0x00485338u);                    /* the shell loop */
    if (R32(0x00624db4u) == 0) {
        C1(0x004b9674u, 0x00c850ecu);
        C1(0x004b8fc8u, 0x00c850bcu);
        C0(0x004a9c38u);
        C0(0x0049e44cu);
        C0(0x00495ad8u);
        C3(0x004b41a0u, DISPLAY, 1, 1);
        W32(0x00624db4u, 1);
    }
    C2(0x004868c0u, AUDIO_CFG, 1);
    C2(0x004bb1e4u, FB(0x384), 1);
    return 0;
}
