/*
 * sh5_target_damage.c: damage to a destructible target, TM2.EXE 0x00470d3c (see
 * sh5_target_damage.h).
 * DECOMPILER TRAPS: the Ghidra output loses the second argument of the animation steps of types
 * 0x1fa / 0x1fb: the register still holds the effect id given to 0x00456fb0 (4 / 5), not the
 * instance. The dish's object lookup (0x00470b14) is by the instance, not the type.
 */
#include "sh5_target_damage.h"

#define R8(a) sh_r8(e, a)
#define R16(a) sh_r16(e, a)
#define R32(a) sh_r32(e, a)
#define W8(a, v) sh_w8(e, a, v)
#define W16(a, v) sh_w16(e, a, v)
#define W32(a, v) sh_w32(e, a, v)
#define FB(o) (fb + (uint32_t)(o))
#define C0(f) sh3_call(e, f, 0, 0, 0, 0, 0, 0)
#define C1(f, a) sh3_call(e, f, 1, a, 0, 0, 0, 0)
#define C2(f, a, b) sh3_call(e, f, 2, a, b, 0, 0, 0)
#define C3(f, a, b, c) sh3_call(e, f, 3, a, b, c, 0, 0)
#define C4(f, a, b, c, d) sh3_call(e, f, 4, a, b, c, d, 0)

#define TARGETS 0x006226c8u
#define LAST 0x006226c4u        /* the last explosion */

static uint32_t s16(const ShEnv* e, uint32_t a) { return (uint32_t)(int32_t)(int16_t)R16(a); }

/* n explosions of size r around pos: kinds (a, b) then the flash kind f */
static void blasts(const ShEnv* e, uint32_t pos, int n, uint32_t r, uint32_t a, uint32_t b, uint32_t f) {
    for (int k = 0; k < n; k++) {
        W32(LAST, C4(0x0044a40cu, pos, r, r, r));
        C2(a, R32(LAST), 0);
        C2(b, R32(LAST), 0);
        C2(0x004516bcu, f, R32(LAST));
    }
}

uint8_t sh5_target_damage(const ShEnv* e, uint32_t fb, int32_t type, int32_t inst, uint32_t dmg) {
    W32(FB(8), (uint32_t)type);
    for (int k = 0; k < 8; k++) W8(FB(k), R8(0x004e7fecu + (uint32_t)k));
    int32_t found = -1;
    for (int32_t k = 0; k < 0x32; k++) {
        uint32_t r = TARGETS + 0x14 * (uint32_t)k;
        if ((int32_t)s16(e, r) == type && (int32_t)s16(e, r + 2) == inst && (int16_t)R16(r + 4) > 0) found = k;
    }
    if (found < 0) return 0;
    uint32_t t = TARGETS + 0x14 * (uint32_t)found;
    W16(t + 4, (uint16_t)(R16(t + 4) - (uint16_t)dmg));
    if ((int16_t)R16(t + 4) > 0) return 0;
    if ((int16_t)R16(t + 4) < 0) W16(t + 4, 0);
    if (R8(t + 6)) C3(0x00470320u, s16(e, t), s16(e, t + 2), 1);
    else C3(0x004707b0u, s16(e, t), s16(e, t + 2), 0);
    uint16_t ty = R16(t);
    uint32_t pos = t + 8;
    if (ty == 0x1f4) {                  /* the satellite dish */
        if ((uint8_t)C0(0x00474508u) == 0xc) {
            if (found == 0x11) {
                C4(0x0046f8b0u, 0x3e, 0, 0, 0x63);
                C4(0x0046f8b0u, 0x1e, 0, 0, 0x63);
            } else {
                C4(0x0046f8b0u, 0x1e, 0, 0, 0x50);
            }
        } else {
            if (C0(0x0046cc54u) == 0) C1(0x0046f974u, pos);
            else C2(0x0046f980u, pos, 0xffffffffu);
            uint32_t s = C0(0x0046f990u);
            uint32_t v = C0(0x0046f988u);
            C4(0x0046f8b0u, 0x1e, v, s, 0x63);
        }
        C2(0x004511ecu, pos, 0);
        uint32_t o = C2(0x00470b14u, s16(e, t + 2), 0);
        if (R8(o + 6) > 2) {
            W8(o + 7, 2);
            uint32_t a[9] = {o, pos, FB(0), 0, 0, 0, 0xa, 0x3c, 0};
            if (e->call) e->call(e->user, 0x0048a738u, 9, a);     /* debris burst */
        }
        W8(o + 7, 1);
        return 1;
    }
    switch (ty) {
    case 0x1f5:
        C2(0x00450864u, pos, 0);
        C2(0x00451014u, pos, 0);
        C2(0x004516bcu, 2, pos);
        return 1;
    case 0x1f6:
        W32(t + 0x10, R32(t + 0x10) + 0x50);
        blasts(e, pos, 2, 0x332, 0x00450864u, 0x00451014u, 3);
        return 1;
    case 0x1f7: case 0x208:
        W32(t + 0x10, R32(t + 0x10) + 0x50);
        blasts(e, pos, 3, 0x332, 0x00450864u, 0x00451014u, 3);
        return 1;
    case 0x1f9: case 0x1fa: case 0x1fb:
        W32(t + 0x10, R32(t + 0x10) + 0x110);
        for (int k = 0; k < 3; k++) {
            W32(LAST, C4(0x0044a40cu, pos, 0xcc, 0xcc, 0xcc));
            if (C0(0x004899c2u) & 1) C2(0x00450984u, R32(LAST), 0);
            else C2(0x00450a10u, R32(LAST), 0);
            C2(0x0045109cu, R32(LAST), 0);
            C2(0x004516bcu, 4, R32(LAST));
        }
        if ((int16_t)R16(t) == 0x1fa || (int16_t)R16(t) == 0x1fb) {
            uint32_t fx = (int16_t)R16(t) == 0x1fa ? 4 : 5;
            C2(0x00456fb0u, fx - 4, fx);
            C3(0x00470320u, s16(e, t), fx, 0);
            W16(t + 4, 0x19);
            for (int k = 0; k < 3; k++) {
                W32(LAST, C4(0x0044a40cu, pos, 0x199, 0x199, 0x199));
                C2(0x004516bcu, 4, R32(LAST));
            }
        }
        return 1;
    case 0x1f8: case 0x1fc: case 0x1fd: case 0x1fe: case 0x207:
        W32(t + 0x10, R32(t + 0x10) + 0x50);
        blasts(e, pos, 2, 0x199, 0x004508f4u, 0x00451058u, 4);
        if ((int16_t)R16(t) == 0x1fc && R8(0x00622ab0u) == 0 && R8(0x00622ab1u) == 0) {
            W8(0x00622ab0u, 1);
            W16(0x00622ab4u, 0);
            C3(0x00470320u, 0x1fc, s16(e, t + 2), 0);
        }
        if ((int16_t)R16(t) == 0x207 && R8(0x00622adcu) == 0 && (int8_t)R8(0x00622addu) == 0) {
            W8(0x00622adcu, 1);
            C3(0x00470320u, 0x207, s16(e, t + 2), 0);
        }
        if ((int16_t)R16(t) == 0x1fd && R8(0x00622adcu) == 0 && (int8_t)R8(0x00622addu) == 1) {
            W8(0x00622adcu, 1);
            C3(0x00470320u, 0x1fd, s16(e, t + 2), 0);
        }
        if ((int16_t)R16(t) == 0x1fe) {     /* the two player target events */
            for (uint32_t j = 0; j < 2; j++) {
                uint32_t ev = 0x00622af4u + 0x2c * j;
                if (R16(t + 2) != R16(ev + 2) || R8(ev) || R8(ev + 1)) continue;
                W8(ev, 1);
                C3(0x00470320u, s16(e, t), s16(e, t + 2), 0);
                if (j) {
                    C1(0x0042f2dcu, 0);
                    C1(0x0042f2dcu, 5);
                } else {
                    C1(0x0042f2dcu, 2);
                    C1(0x0042f2dcu, 4);
                }
            }
        }
        return 1;
    default:
        return 1;
    }
}
