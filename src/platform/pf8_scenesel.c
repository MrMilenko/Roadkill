/*
 * pf8_scenesel.c: TM2.EXE 0x0046ac4c, an unlisted function (a scene vtable slot, pointer at 0x004e5760; the title
 * screen's scene reaches it): selects the scene's current item. Ported over Pf2Env (pf2_env.h); emulator diff:
 * tools/emu/check_pf8_scenesel.sh.
 *
 * this +0x0c the screen (+0x40 set to 1, +0x44 the mode byte = this +0x10), by the mode (jump table 0x0046ac38):
 * 0: screen +0xf94 = the entry list (+0xbc) item at +0x48 (its +0x50), when 0 <= +0x48 < +0xb8, else 0;
 * 1 / 2 / 3: screen +0xf94 = +0xf80 / +0xf84 / +0xf88; 4: +0xf94 = 0. Above 4 nothing (returns this).
 * Returns eax as the original leaves it (edx / ebx are saved).
 */
#include "pf2_env.h"
#include "pf8_scenesel.h"

#define R32 pf2_r32

uint32_t pf8_0046ac4c(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)d; (void)b; (void)c;
    pf2_w32(e, R32(e, a + 0xc) + 0x40, 1);
    uint8_t m = pf2_r8(e, a + 0x10);
    pf2_w8(e, R32(e, a + 0xc) + 0x44, m);
    if (m > 4) return a;
    uint32_t o;
    if (m == 0) {
        o = R32(e, a + 0xc);
        int32_t i = (int32_t)R32(e, o + 0x48);
        if (i >= 0 && i < (int32_t)R32(e, o + 0xb8)) {
            uint32_t v = R32(e, R32(e, o + 0xbc + (uint32_t)i * 4) + 0x50);
            pf2_w32(e, o + 0xf94, v);
            return v;
        }
    } else if (m < 4) {
        o = R32(e, a + 0xc);
        pf2_w32(e, o + 0xf94, R32(e, o + 0xf80 + 4 * (uint32_t)(m - 1)));
        return o;
    }
    o = R32(e, a + 0xc);
    pf2_w32(e, o + 0xf94, 0);
    return o;
}
