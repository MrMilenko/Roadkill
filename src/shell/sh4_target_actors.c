/*
 * sh4_target_actors.c: the target actors' frame update, TM2.EXE 0x00471b14 (see
 * sh4_target_actors.h). Locals live in the original's frame (fb) because the matrix step and
 * car position calls write through pointers to them.
 * Machine details kept: the "near" test takes the largest of the three absolute differences
 * in the original's compare order (absolute values of INT_MIN stay negative and count as near),
 * the facing angle uses dy = 1 when dy is 0, the rotation byte +0xe5 gets 8 added (a byte add),
 * the throw strength is i16 speed / 32 (signed, rounds to zero).
 */
#include "sh4_target_actors.h"

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

static int32_t iabs(int32_t v) { return (int32_t)(((uint32_t)v ^ (uint32_t)(v >> 31)) - (uint32_t)(v >> 31)); }
static int32_t i16at(const ShEnv* e, uint32_t a) { return (int16_t)R16(a); }

static void anim(const ShEnv* e, uint32_t a, uint32_t step) {
    C3(0x00470320u, (uint32_t)i16at(e, a + 4), (uint32_t)i16at(e, a + 6), step);
}

/* the position at fb +0x0c against the node's: largest absolute difference / 8 < 10 */
static int near(const ShEnv* e, uint32_t fb, uint32_t node) {
    int32_t dx = (int32_t)(R32(FB(0xc)) - R32(node + 0xe8));
    int32_t dy = (int32_t)(R32(FB(0x10)) - R32(node + 0xec));
    int32_t dz = (int32_t)(R32(FB(0x14)) - R32(node + 0xf0));
    int32_t big = iabs(dy) > iabs(dz) ? iabs(dy) : iabs(dz);
    int32_t m = iabs(dx) > big ? dx : (iabs(dy) > iabs(dz) ? dy : dz);
    return (iabs(m) >> 3) < 10;
}

/* the hit's music cue and crowd sound (not for type 0x205) */
static void hit_cue(const ShEnv* e, uint32_t node) {
    if (C0(0x0046cc54u) == 0) C1(0x0046f974u, node + 0xe8);
    else C2(0x0046f980u, node + 0xe8, 0xffffffffu);
}
static void crowd(const ShEnv* e) {
    uint32_t s = C0(0x0046f990u);
    uint32_t v = C0(0x0046f988u);
    C4(0x0046f8b0u, 0x19, v * 2, s, 0x32);
}


void sh4_target_actors_update(const ShEnv* e, uint32_t fb) {
    W32(FB(0x1c), 0);
    if ((int32_t)R32(0x00622b54u) <= 0) return;
    W32(FB(0x20), 0x00622b58u);
    do {
        uint32_t a = R32(FB(0x20));
        uint32_t node = R32(a + 0x48);
        int32_t g = (int32_t)C1(0x0042c2d4u, node + 0xe8);
        if ((int32_t)(R32(node + 0xf0) - (uint32_t)g) > 0xc8) anim(e, a, 0);
        int32_t ty = i16at(e, a + 4);
        if ((ty == 0x202 || ty == 0x205) && (int8_t)R8(a + 2) == 0) {
            W32(FB(0), 0);
            W32(FB(8), 0);
            W32(FB(4), 7);
            C3(0x0048cc14u, R32(a + 0x48) + 0xb0, FB(0), FB(0xc));
            W32(R32(a + 0x48) + 0xe8, R32(R32(a + 0x48) + 0xe8) + R32(FB(0xc)));
            W32(R32(a + 0x48) + 0xec, R32(R32(a + 0x48) + 0xec) + R32(FB(0x10)));
            W32(R32(a + 0x48) + 0xf0, R32(R32(a + 0x48) + 0xf0) + R32(FB(0x14)));
            uint32_t n = R32(a + 0x48);
            uint32_t dx = R32(a + 0xc) - R32(n + 0xe8);
            uint32_t dy = R32(a + 0x10) - R32(n + 0xec);
            if (dy == 0) dy = 1;
            W16(R32(a + 0x48) + 0xe4, (uint16_t)C2(0x0048b71cu, dx, dy));
            C2(0x0048c360u, R32(a + 0x48) + 0xe0, R32(a + 0x48) + 0xb0);
            if (iabs((int32_t)(R32(R32(a + 0x48) + 0xe8) - R32(a + 0xc))) < 0x50 &&
                iabs((int32_t)(R32(R32(a + 0x48) + 0xec) - R32(a + 0x10))) < 0x50) {
                W8(a + 1, R8(a + 1) ^ 1);
                uint8_t et = (uint8_t)C0(0x00474508u);
                uint32_t tab;
                switch (et) {
                case 1: tab = 0x004e7d58u; break;
                case 3: tab = 0x004e7cc8u; break;
                case 6: tab = 0x004e7ddcu; break;
                case 8: tab = 0x004e7e9cu; break;
                case 2: case 4: tab = 0; break;
                default: return;            /* the rest of the actors are skipped */
                }
                if (tab) {
                    for (int k = 0; k < 3; k++) {
                        uint32_t row = (uint32_t)((int8_t)R8(a) * 0x18) + (uint32_t)((int8_t)R8(a + 1) * 0xc);
                        W32(a + 0xc + 4 * k, R32(tab + row + 4 * k));
                    }
                }
                W8(R32(a + 0x48) + 0xe5, R8(R32(a + 0x48) + 0xe5) + 8);
            }
        }
        if ((int8_t)R8(a + 2) == 0) {
            W32(FB(0x18), 0);
            while ((int32_t)C0(0x0045afb4u) > (int32_t)R32(FB(0x18))) {      /* human cars */
                uint32_t i = R32(FB(0x18));
                C2(0x0045adacu, i, FB(0xc));
                node = R32(a + 0x48);
                if (near(e, fb, node)) {
                    hit_cue(e, node);
                    if (i16at(e, a + 4) != 0x205) crowd(e);
                    uint32_t s = C0(0x0046f990u);
                    uint32_t v = C0(0x0046f988u);
                    C4(0x0046f8b0u, 3, v, s, 0x2d);
                    W8(a + 2, 1);
                    W16(a + 8, 0);
                    uint32_t car = C1(0x0045afbcu, i);
                    W16(a + 0xa, (uint16_t)(((int32_t)R32(car + 0x6cc) >> 16) / 32));
                    uint32_t mat = C1(0x0045ae24u, i);
                    for (int k = 0; k < 12; k++) W32(a + 0x18 + 4 * k, R32(mat + 0xb0 + 4 * k));
                    anim(e, a, 2);
                    W32(R32(a + 0x48) + 0xf0, R32(R32(a + 0x48) + 0xf0) + 0x28);
                    C1(0x00457398u, R32(a + 0x48) + 0xe8);
                    uint32_t n = R32(a + 0x48);
                    uint32_t hits = R32(0x00622d20u) + 1;
                    uint32_t z = R32(n + 0xf0) - 0x28;
                    W32(0x00622d20u, hits);
                    W32(n + 0xf0, z);
                    if (hits == 10) {          /* the tenth hit: armor and health */
                        uint32_t p = C1(0x0045afbcu, R32(FB(0x18)));
                        int32_t d = (int32_t)((int32_t)R32(p + 0x86a) >> 16) - ((int32_t)R32(p + 0x868) >> 16);
                        if (d > 5) d = 5;
                        W16(p + 0x85a, (uint16_t)(i16at(e, p + 0x85a) + d));
                        int32_t mx = (int32_t)R32(p + 0x170) >> 16;
                        int32_t hp = (int32_t)R32(p + 0x1d8) + mx / 3;
                        W32(p + 0x1d8, (uint32_t)hp);
                        W32(p + 0x1d8, (uint32_t)(mx > hp ? hp : mx));
                        C2(0x00436040u, 0x21, 0);
                        if (i16at(e, p) == 0) C4(0x0046f8b0u, 0x18, 0, 0, 0x41);
                    }
                }
                W32(FB(0x18), R32(FB(0x18)) + 1);
            }
            W32(FB(0x24), 0);
            while ((int32_t)C0(0x0045b078u) > (int32_t)R32(FB(0x24))) {      /* AI cars */
                uint32_t j = R32(FB(0x24));
                C2(0x0045add0u, j, FB(0xc));
                node = R32(a + 0x48);
                if (near(e, fb, node)) {
                    if (i16at(e, a + 4) != 0x205) {
                        hit_cue(e, node);
                        crowd(e);
                    }
                    W8(a + 2, 1);
                    W16(a + 8, 0);
                    uint32_t car = C1(0x0045b080u, R32(FB(0x24)));
                    W16(a + 0xa, (uint16_t)(((int32_t)R32(car + 0x710) >> 16) / 32));
                    uint32_t mat = C1(0x0045ae2cu, R32(FB(0x24)));
                    for (int k = 0; k < 12; k++) W32(a + 0x18 + 4 * k, R32(mat + 0xb0 + 4 * k));
                    anim(e, a, 2);
                    W32(R32(a + 0x48) + 0xf0, R32(R32(a + 0x48) + 0xf0) + 0x28);
                    C1(0x00457398u, R32(a + 0x48) + 0xe8);
                    W32(R32(a + 0x48) + 0xf0, R32(R32(a + 0x48) + 0xf0) - 0x28);
                }
                W32(FB(0x24), R32(FB(0x24)) + 1);
            }
        }
        if ((int8_t)R8(a + 2) == 1) {          /* thrown */
            if ((uint8_t)C0(0x00474508u) == 5 && i16at(e, a + 6) == 1) {
                W32(FB(0x10), 0);
                W32(FB(0x14), 0);
                W32(FB(0xc), 0);
            } else {
                W32(FB(0), 0);
                W32(FB(4), (uint32_t)(i16at(e, a + 0xa) + 5));
                W32(FB(8), 0);
                C3(0x0048cc14u, a + 0x18, FB(0), FB(0xc));
            }
            W32(R32(a + 0x48) + 0xe8, R32(R32(a + 0x48) + 0xe8) + R32(FB(0xc)));
            W32(R32(a + 0x48) + 0xec, R32(R32(a + 0x48) + 0xec) + R32(FB(0x10)));
            W32(R32(a + 0x48) + 0xf0, R32(R32(a + 0x48) + 0xf0) + R32(FB(0x14)));
            if ((uint8_t)C0(0x00474508u) == 5 && i16at(e, a + 6) == 1 && i16at(e, a + 8) == 0xa)
                C2(0x00457138u, 0x004e7e54u, 0);
            int16_t w = (int16_t)R16(a + 8);
            W16(a + 8, (uint16_t)(w + 1));
            if (w > 0x28) W8(a + 2, 2);
        }
        if ((int8_t)R8(a + 2) == 2) {          /* lying */
            int16_t w = (int16_t)R16(a + 8);
            W16(a + 8, (uint16_t)(w + 1));
            if (w > 0x12c) {
                W32(R32(R32(FB(0x20)) + 0x48) + 8, 1);
                W8(R32(FB(0x20)) + 2, 0);
                C1(0x004713f0u, R32(FB(0x20)));         /* targetActorPlace */
                uint32_t b = R32(FB(0x20));
                anim(e, b, 1);
            }
        }
        W32(FB(0x20), R32(FB(0x20)) + 0x4c);
        W32(FB(0x1c), R32(FB(0x1c)) + 1);
    } while ((int32_t)R32(FB(0x1c)) < (int32_t)R32(0x00622b54u));
}
