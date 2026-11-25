/*
 * gp4_hud.c: the in-game HUD of one view, TM2.EXE 0x00434710 (see gp4_hud.h).
 */
#include "gp4_hud.h"

#include <string.h>

typedef const Gp4HudEnv E;

static int16_t R16(E* e, uint32_t a) { int16_t v; memcpy(&v, e->mem(e->user, a), 2); return v; }
static int32_t R32(E* e, uint32_t a) { int32_t v; memcpy(&v, e->mem(e->user, a), 4); return v; }
static uint8_t R8(E* e, uint32_t a) { return *e->mem(e->user, a); }
static void W16(E* e, uint32_t a, int32_t v) { int16_t x = (int16_t)v; memcpy(e->mem(e->user, a), &x, 2); }
static void W32(E* e, uint32_t a, int32_t v) { memcpy(e->mem(e->user, a), &v, 4); }
static void W8(E* e, uint32_t a, int32_t v) { *e->mem(e->user, a) = (uint8_t)v; }
static void OR2(E* e, uint32_t a) { *e->mem(e->user, a) |= 2; }

static uint32_t CALL(E* e, uint32_t addr, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
                     uint32_t a4, uint32_t a5, uint32_t a6, void* l1, void* l2) {
    uint32_t a[7] = {a0, a1, a2, a3, a4, a5, a6};
    return e->call(e->user, addr, n, a, l1, l2);
}
#define C0(f) CALL(e, f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
#define C1(f, a) CALL(e, f, 1, (uint32_t)(a), 0, 0, 0, 0, 0, 0, 0, 0)
#define C2(f, a, b) CALL(e, f, 2, (uint32_t)(a), (uint32_t)(b), 0, 0, 0, 0, 0, 0, 0)
#define C3(f, a, b, c) CALL(e, f, 3, (uint32_t)(a), (uint32_t)(b), (uint32_t)(c), 0, 0, 0, 0, 0, 0)
#define PRIM(p) C1(0x4967f4, p)
#define NOP(a) CALL(e, 0x452d28, 4, (a), 0x24, 0x96, 0x24, 0, 0, 0, 0, 0)
#define TEXT7(f, a, b, c, d, s0, s1, s2) \
    CALL(e, f, 7, (uint32_t)(a), (uint32_t)(b), (uint32_t)(c), (uint32_t)(d), (uint32_t)(s0), (uint32_t)(s1), \
         (uint32_t)(s2), 0, 0)

static int level(int32_t p) { return p > 0x28 ? 3 : p > 0xc ? 2 : p > 0 ? 1 : 0; }
static int32_t clampw(int32_t w, int32_t hi) { return w < 1 ? 1 : w > hi ? hi : w; }
static int32_t mul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }

static void target_name(E* e, int32_t i, int32_t id, uint32_t a, uint32_t b) {
    NOP(2);
    int32_t v = R32(e, 0x4f6a34);
    int32_t k = R8(e, 0x4e685e + i) == 0 ? 0x35 : 0;
    int32_t n = (int32_t)C1(0x45bb20, id);
    TEXT7(0x45222c, 2, R32(e, 0x4e6754), R32(e, 0x4e6758) - k, a, b, R32(e, 0x4e6870 + 4 * n), v);
    C1(0x452d48, 2);
}

void gp4_hud_draw(E* e, int32_t i) {
    if ((uint8_t)C0(0x45c348)) return;
    if (C0(0x46be2c)) return;
    int32_t split = (int8_t)C0(0x4958ac);
    int32_t w = 0, h = 0;
    CALL(e, 0x4938a4, 2, 0, 0, 0, 0, 0, 0, 0, &w, &h);
    if (w != R32(e, 0x4f6a2c) || h != R32(e, 0x4f6a30)) {
        int32_t hw = w / 2, hh = h / 2;
        for (uint32_t k = 0; k != 0x120; k += 0x18) {
            uint32_t b = 0x4e66fc + k;
            W32(e, b + 0x10, mul(R32(e, b), hw) + hw + R32(e, b + 8));
            W32(e, b + 0x14, mul(R32(e, b + 4), hh) + hh + R32(e, b + 0xc));
        }
    }
    uint32_t car = C1(0x45afbc, i);
    uint32_t i2 = (uint32_t)i << 11;
    if (R8(e, 0x4e685c + i)) {
        if (R8(e, 0x4e685e + i)) C2(0x4361d4, R32(e, 0x4f6a34), i);
        if (C0(0x45bab8) == 0) {
            W32(e, 0x4fb164, 0); W32(e, 0x4fb160, 0); W32(e, 0x4fb15c, 0); W32(e, 0x4fb16c, 100);
        }
        int32_t t = R32(e, 0x4fb16c) - 1;
        W32(e, 0x4fb16c, t);
        if (t <= 0) {
            W32(e, 0x4fb15c, R32(e, 0x4fb160));
            W32(e, 0x4fb16c, 100);
            W32(e, 0x4fb164, R32(e, 0x4fb168));
            if (R32(e, 0x4fb15c) == 0) {
                W32(e, 0x4fb15c, (int32_t)C0(0x45bab8));
                W32(e, 0x4fb164, 0x32);
            }
            W32(e, 0x4fb160, (int32_t)C0(0x45bab8));
            W32(e, 0x4fb168, 0x32);
        }
        NOP(2);
        {
            int32_t v = R32(e, 0x4f6a34);
            int32_t s = R32(e, 0x4e68ac + 4u * (uint32_t)R32(e, 0x4fb15c));
            TEXT7(0x452204, 2, R32(e, 0x4e676c), R32(e, 0x4e6770), 0x4f5df4, 0x4f5e00, s, v);
        }
        uint8_t human = 0;
        uint32_t p = CALL(e, 0x429f7c, 2, (uint32_t)i, 0, 0, 0, 0, 0, 0, &human, 0);
        if (p) {
            int32_t id;
            uint32_t st;
            if (human) { id = R16(e, p + 0x90); st = p + 0x138; }
            else { id = R16(e, p + 0x30); st = p + 0x4ec; }
            int32_t x0 = R16(e, 0x4e673c) - 1;
            W16(e, 0x4f6abc + i2, x0);
            int32_t k = R8(e, 0x4e685e + i) == 0 ? 0x35 : 0;
            int32_t y0 = R16(e, 0x4e6740) - 1;
            W16(e, 0x4f6abe + i2, y0 - k);
            int32_t x1 = R16(e, 0x4e673c) + 0x5b;
            W16(e, 0x4f6ac0 + i2, x1);
            W16(e, 0x4f6ac8 + i2, x1);
            W16(e, 0x4f6ac2 + i2, y0 - k);
            W16(e, 0x4f6aca + i2, y0 - k + 0xc);
            W16(e, 0x4f6ac4 + i2, x0);
            W16(e, 0x4f6ac6 + i2, y0 - k + 0xc);
            OR2(e, 0x4f6abb + i2);
            if (split) PRIM(0x4f6ab4 + i2);
            if (!split && R32(e, st + 0xa0) > 0) target_name(e, i, id, 0x4f60d0, 0x4f60dc);
            int32_t pct = mul(R32(e, st + 0xa0), 100) / R16(e, st + 0x3a);
            int lv = level(pct);
            int32_t bw = clampw(mul(pct, 0x5a) / 100, 0x5a);
            {
                int32_t a = R16(e, 0x4f6aa4 + i2), c = R16(e, 0x4f6aac + i2);
                W16(e, 0x4f6ab0 + i2, bw + c);
                W16(e, 0x4f6aa8 + i2, a + bw);
            }
            C2(0x436120, lv, 0x4f6a9c + i2);
            OR2(e, 0x4f6aa3 + i2);
            for (int pass = 0; pass < 2; pass++) {
                int32_t kk = R8(e, 0x4e685e + i) == 0 ? 0x35 : 0;
                W16(e, 0x4f6aa6 + i2, R16(e, 0x4f6aa6 + i2) + (pass ? kk : -kk));
                int32_t v = R16(e, 0x4f6aa6 + i2);
                W16(e, 0x4f6aaa + i2, v);
                W16(e, 0x4f6aae + i2, v + 10);
                W16(e, 0x4f6ab2 + i2, v + 10);
                if (!pass) PRIM(0x4f6a9c + i2);
            }
            if (!split) PRIM(0x4f6ab4 + i2);
            if (split && R32(e, st + 0xa0) > 0) target_name(e, i, id, 0x4f63ac, 0x4f63b8);
        }
    }
    {   /* own health */
        int32_t pct = mul(R32(e, car + 0x1d8), 100) / R16(e, car + 0x172);
        int lv = level(pct);
        int32_t bw = clampw(mul(pct, 0x46) / 100, 0x46);
        W16(e, 0x4f6a68 + i2, R16(e, 0x4e6724));
        W16(e, 0x4f6a6a + i2, R16(e, 0x4e6728));
        int32_t ax = R16(e, 0x4e6724) + 0x46;
        W16(e, 0x4f6a6c + i2, ax);
        int32_t dx = R16(e, 0x4e6728);
        W16(e, 0x4f6a74 + i2, ax);
        W16(e, 0x4f6a6e + i2, dx);
        W16(e, 0x4f6a76 + i2, dx + 10);
        W16(e, 0x4f6a72 + i2, dx + 10);
        dx = R16(e, 0x4e6724);
        ax = R16(e, 0x4f6a68 + i2);
        W16(e, 0x4f6a70 + i2, dx);
        W16(e, 0x4f6a6c + i2, ax + bw);
        W16(e, 0x4f6a74 + i2, bw + R16(e, 0x4f6a70 + i2));
        C2(0x436120, lv, 0x4f6a60 + i2);
        int32_t di = R16(e, 0x4e6724) - 1;
        OR2(e, 0x4f6a67 + i2);
        dx = R16(e, 0x4e6724) + 0x47;
        W16(e, 0x4f6a80 + i2, di);
        W16(e, 0x4f6a84 + i2, dx);
        ax = R16(e, 0x4e6728) - 1;
        W16(e, 0x4f6a8c + i2, dx);
        W16(e, 0x4f6a82 + i2, ax);
        W16(e, 0x4f6a86 + i2, ax);
        ax = R16(e, 0x4e6728) + 0xb;
        W16(e, 0x4f6a88 + i2, di);
        W16(e, 0x4f6a8e + i2, ax);
        W16(e, 0x4f6a8a + i2, ax);
        if (!split) { PRIM(0x4f6a60 + i2); PRIM(0x4f6a78 + i2); }
        else { PRIM(0x4f6a78 + i2); PRIM(0x4f6a60 + i2); }
    }
    if (!(uint8_t)C0(0x4632b0)) {
        uint8_t t = (uint8_t)C0(0x474500);
        if (t == 3 || t == 4) W8(e, 0x4e686b, 0x30);
        else {
            uint8_t c = (uint8_t)((uint8_t)C1(0x474510, i) + 0x2f);
            W8(e, 0x4e686b, c);
            if (c < 0x30 || c > 0x34) W8(e, 0x4e686b, 0x32);
        }
        CALL(e, 0x451a5c, 4, 2, (uint32_t)R32(e, 0x4e676c), (uint32_t)(R32(e, 0x4e6770) - 0x1e), 0x4e6864, 0, 0, 0, 0, 0);
    }
    {   /* turbo */
        int32_t pct = mul(R16(e, car + 0x878), 100) / 3000;
        int lv = level(pct);
        int32_t bw = clampw(mul(pct, 0x2e) / 100, 0x2e);
        int32_t x = R16(e, 0x4e67e4);
        int32_t xm = x - 1, cx = x + 0x2e, ax = x + 0x2f, c;
        W16(e, 0x4f6ae4 + i2, cx);
        W16(e, 0x4f6aec + i2, cx);
        W16(e, 0x4f6afc + i2, ax);
        W16(e, 0x4f6b04 + i2, ax);
        W16(e, 0x4f6ae0 + i2, R16(e, 0x4e67e4));
        if (R8(e, 0x4e685c + i) == 0 || R8(e, 0x4e6860 + i) == 0) {
            int big = R8(e, 0x4e685c + i) != 0;    /* the 0x2d layout */
            int32_t y = R16(e, 0x4e67e8);
            W16(e, 0x4f6aee + i2, y + (big ? 0x2d : 0x21));
            W16(e, 0x4f6aea + i2, y + (big ? 0x2d : 0x21));
            W16(e, 0x4f6b06 + i2, y + (big ? 0x2e : 0x22));
            W16(e, 0x4f6b02 + i2, y + (big ? 0x2e : 0x22));
            W16(e, 0x4f6ae2 + i2, y + (big ? 0x28 : 0x1c));
            W16(e, 0x4f6ae6 + i2, y + (big ? 0x28 : 0x1c));
            W16(e, 0x4f6af8 + i2, xm);
            W16(e, 0x4f6ae8 + i2, R16(e, 0x4e67e4));
            c = R16(e, 0x4e67e8) + (big ? 0x27 : 0x1b);
        } else {
            int32_t y = R16(e, 0x4e67e8);
            W16(e, 0x4f6ae2 + i2, y);
            W16(e, 0x4f6ae6 + i2, y);
            W16(e, 0x4f6b06 + i2, y + 6);
            W16(e, 0x4f6b02 + i2, y + 6);
            W16(e, 0x4f6aee + i2, y + 5);
            W16(e, 0x4f6aea + i2, y + 5);
            W16(e, 0x4f6af8 + i2, xm);
            W16(e, 0x4f6ae8 + i2, R16(e, 0x4e67e4));
            c = R16(e, 0x4e67e8) - 1;
        }
        W16(e, 0x4f6afa + i2, c);
        W16(e, 0x4f6afe + i2, c);
        W16(e, 0x4f6b00 + i2, xm);
        {
            int32_t a = R16(e, 0x4f6ae0 + i2), d = R16(e, 0x4f6ae8 + i2);
            W16(e, 0x4f6ae4 + i2, a + bw);
            W16(e, 0x4f6aec + i2, bw + d);
        }
        C2(0x436120, lv, 0x4f6ad8 + i2);
        if (!split) { PRIM(0x4f6ad8 + i2); PRIM(0x4f6af0 + i2); }
        else {
            OR2(e, 0x4f6af7 + i2);
            OR2(e, 0x4f6adf + i2);
            PRIM(0x4f6af0 + i2);
            PRIM(0x4f6ad8 + i2);
        }
    }
    if (R8(e, 0x4e6860 + i)) {
        int32_t x = R16(e, 0x4e679c), y = R16(e, 0x4e67a0);
        W16(e, 0x4f7048 + i2, x);
        W16(e, 0x4f704e + i2, y);
        W16(e, 0x4f704a + i2, R16(e, 0x4e67a0));
        int32_t cx = R16(e, 0x4e679c) + 0x2d;
        W16(e, 0x4f7064 + i2, cx);
        int32_t dx = R16(e, 0x4e679c) + 0x2a;
        W16(e, 0x4f704c + i2, dx);
        W16(e, 0x4f7054 + i2, dx);
        W16(e, 0x4f706c + i2, cx);
        int32_t bx = R16(e, 0x4e679c);
        W16(e, 0x4f7056 + i2, y + 0x42);
        W16(e, 0x4f7050 + i2, bx);
        W16(e, 0x4f7052 + i2, y + 0x42);
        int32_t yy = R16(e, 0x4e67a0) - 3;
        W16(e, 0x4f7060 + i2, bx - 3);
        W16(e, 0x4f7062 + i2, yy);
        W16(e, 0x4f7066 + i2, yy);
        yy = R16(e, 0x4e67a0) + 0x45;
        W16(e, 0x4f7068 + i2, bx - 3);
        W16(e, 0x4f706e + i2, yy);
        W16(e, 0x4f706a + i2, yy);
        if (split) {
            OR2(e, 0x4f705f + i2);
            OR2(e, 0x4f7047 + i2);
            PRIM(0x4f7058 + i2);
            PRIM(0x4f7040 + i2);
        }
        uint8_t hl[0x28];
        memset(hl, 0, sizeof hl);
        for (int32_t k = 0; k < R16(e, 0x4f6b14 + i2); k++) {
            uint32_t ko = 0x18u * (uint32_t)k, k3 = 0x30u * (uint32_t)k;
            int32_t yo = 8 * k;
            int32_t n = R16(e, car + 2u * (uint32_t)k + 0x856);
            if (n <= 0) continue;
            if ((int8_t)R8(e, car + 0x855) == k) {
                hl[7] = 0x28; hl[4] = 0; hl[5] = 0xc8; hl[6] = 0x64; hl[7] |= 2;
                int16_t v[8];
                int32_t x9 = R16(e, 0x4e679c);
                v[0] = (int16_t)x9;
                v[1] = (int16_t)(R16(e, 0x4e67b8) + yo);
                v[2] = (int16_t)(x9 + 0x2a);
                v[3] = (int16_t)(R16(e, 0x4e67b8) + yo);
                v[6] = (int16_t)(x9 + 0x2a);
                v[7] = (int16_t)(R16(e, 0x4e67b8) + yo + 8);
                v[4] = (int16_t)R16(e, 0x4e679c);
                v[5] = (int16_t)(R16(e, 0x4e67b8) + yo + 8);
                memcpy(hl + 8, v, sizeof v);
                if (split) CALL(e, 0x4967f4, 1, 0, 0, 0, 0, 0, 0, 0, hl, 0);
            }
            W16(e, 0x4f6b20 + i2 + ko, R16(e, 0x4e67b4));
            W8(e, 0x4f6b1c + i2 + ko, 0xff);
            W8(e, 0x4f6b1d + i2 + ko, 0xff);
            W8(e, 0x4f6b1e + i2 + ko, 0xff);
            W16(e, 0x4f6b22 + i2 + ko, R16(e, 0x4e67b8) + yo + 1);
            PRIM(0x4f6b18 + i2 + ko);
            int32_t ax = R16(e, 0x4e67cc) + 2, by = R16(e, 0x4e67d0) + yo;
            uint32_t c = i2 + k3, prim;
            if (n >= 10) {
                W16(e, 0x4f6df2 + c, by);
                W16(e, 0x4f6e08 + c, ax);
                W16(e, 0x4f6e0a + c, by);
                W16(e, 0x4f6df0 + c, R16(e, 0x4e67cc) - 4);
                W8(e, 0x4f6dec + c, 0xff); W8(e, 0x4f6ded + c, 0xff); W8(e, 0x4f6dee + c, 0xff);
                prim = 0x4f6de8 + c;
            } else {
                W16(e, 0x4f6e08 + c, ax);
                W16(e, 0x4f6e0a + c, by);
                W8(e, 0x4f6e04 + c, 0xff); W8(e, 0x4f6e05 + c, 0xff); W8(e, 0x4f6e06 + c, 0xff);
                prim = 0x4f6de8 + c + 0x18;
            }
            C3(0x496dcc, n, prim, 0x4f70d8 + i2);
            if ((int8_t)R8(e, car + 0x855) == k && !split) CALL(e, 0x4967f4, 1, 0, 0, 0, 0, 0, 0, 0, hl, 0);
        }
        if (!split) { PRIM(0x4f7040 + i2); PRIM(0x4f7058 + i2); }
    } else {
        int32_t sel = (int8_t)R8(e, car + 0x855);
        uint8_t ev = R8(e, 0x624ec7);
        if (sel < 0 || sel > 7) sel = 0;
        if (ev != 0xb && ev != 4) sel = R32(e, 0x4e681c + 4u * (uint32_t)sel);
        int32_t n = R16(e, car + 2u * (uint32_t)sel + 0x856);
        if (n > 0) {
            uint32_t o = i2 + 0x18u * (uint32_t)sel;
            W16(e, 0x4f6c8a + o, R16(e, 0x4e67b8) + 0x26);
            W8(e, 0x4f6c84 + o, 0xff); W8(e, 0x4f6c85 + o, 0xff); W8(e, 0x4f6c86 + o, 0xff);
            W16(e, 0x4f6c88 + o, R16(e, 0x4e67b4) - 9);
            PRIM(0x4f6c80 + o);
            uint32_t a = i2 + 0x30u * (uint32_t)sel;
            W8(e, 0x4f6dec + a, 0); W8(e, 0x4f6ded + a, 0x8c); W8(e, 0x4f6dee + a, 10);
            W8(e, 0x4f6e04 + a, 0); W8(e, 0x4f6e05 + a, 0x8c); W8(e, 0x4f6e06 + a, 10);
            int32_t xx = R16(e, 0x4e67cc) - 4;
            int32_t yy = R16(e, 0x4e67d0) + 0x2a;
            uint32_t prim = 0x4f6de8 + a;
            if (n < 10) {
                W16(e, 0x4f6e0a + a, yy);
                prim += 0x18;
            } else {
                W16(e, 0x4f6df2 + a, yy);
                W16(e, 0x4f6e0a + a, yy);
                W16(e, 0x4f6df0 + a, R16(e, 0x4e67cc) - 9);
            }
            W16(e, 0x4f6e08 + a, xx);
            C3(0x496dcc, n, prim, 0x4f70d8 + i2);
        }
    }
    {
        W16(e, 0x4f7184 + i2, R16(e, 0x4e67fc));
        W8(e, 0x4f7180 + i2, 0xff); W8(e, 0x4f7181 + i2, 0xff); W8(e, 0x4f7182 + i2, 0xff);
        W16(e, 0x4f7186 + i2, R16(e, 0x4e6800));
        PRIM(0x4f717c + i2);
        int32_t q = R16(e, car + 0x87a) / 10;
        int32_t dx = R16(e, 0x4e67fc) + 0x1b;
        int32_t si = R16(e, 0x4e6800) - 2;
        uint32_t prim;
        if (q < 10) {
            W8(e, 0x4f71bc + i2, 0xff); W8(e, 0x4f71bd + i2, 0xff); W8(e, 0x4f71be + i2, 0xff);
            W16(e, 0x4f71c2 + i2, si);
            prim = 0x4f71b8 + i2;
        } else {
            W8(e, 0x4f71a4 + i2, 0xff); W8(e, 0x4f71a5 + i2, 0xff); W8(e, 0x4f71a6 + i2, 0xff);
            W8(e, 0x4f71bc + i2, 0xff); W8(e, 0x4f71bd + i2, 0xff); W8(e, 0x4f71be + i2, 0xff);
            W16(e, 0x4f71aa + i2, si);
            W16(e, 0x4f71c2 + i2, si);
            W16(e, 0x4f71a8 + i2, R16(e, 0x4e67fc) + 0x15);
            prim = 0x4f71a0 + i2;
        }
        W16(e, 0x4f71c0 + i2, dx);
        C3(0x496dcc, q, prim, 0x4f70d8 + i2);
    }
    {
        int32_t v = (int32_t)C1(0x45aee8, i);
        if (v < 0) v = (int32_t)(0u - (uint32_t)v);
        int32_t ax = R32(e, 0x4e6814);
        int32_t vv = R32(e, 0x4f6a34);
        int32_t bx = R32(e, 0x4e6818);
        uint8_t s[4];
        int32_t tmpl = R32(e, 0x4e69a0);
        memcpy(s, &tmpl, 4);
        for (int k = 2; k >= 0; k--) {
            int32_t d = v % 10;
            v /= 10;
            if (k == 2 || d != 0 || v != 0) s[k] = (uint8_t)(d + 0x30);
        }
        NOP(4);
        CALL(e, 0x452204, 7, 4, (uint32_t)ax, (uint32_t)bx, 0x4f7170 + i2, 0x4f7128 + i2, 0, (uint32_t)vv, s, 0);
        C1(0x452d48, 4);
    }
    if ((uint8_t)C0(0x4632b0) && R8(e, 0x4f6a3a) == 0) {
        W8(e, 0x4f6a38, 1);
        W8(e, 0x4f6a3a, 1);
    }
    if (R8(e, 0x4f6a38)) C0(0x437134);
    if (C1(0x44eee4, 0x15)) {
        int32_t t = (int32_t)C0(0xc90640);
        int32_t old = R32(e, 0x4f6964);
        W32(e, 0x4f6968, t);
        if ((int32_t)((uint32_t)t - (uint32_t)old) >= 1000 || t < old) {
            W32(e, 0x4f6964, R32(e, 0x4f6968));
            int32_t f = (int32_t)C0(0x4931dc);
            W8(e, 0x4e66f4, f / 10 + 0x30);
            W8(e, 0x4e66f5, f % 10 + 0x30);
        }
        int32_t vv = R32(e, 0x4f6a34);
        int32_t hgt = (int32_t)C0(0x4958e4);
        int32_t wid = (int32_t)C0(0x4958d8);
        TEXT7(0x452204, 2, wid - 0x64, hgt - 0x28, 0x4f6688, 0x4f6694, 0x4e66f4, vv);
    }
}
