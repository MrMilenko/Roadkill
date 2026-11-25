/*
 * pf8_cheats.c: TM2.EXE 0x0044e1cc, an unlisted function (not in Ghidra's function list; its address is
 * handed out by subsys_game_data_init 0x0044e062): the cheat / debug code handler. eax = the code
 * number 0..38 (a 39 entry jump table at 0x0044e130). Ported over Pf2Env (pf2_env.h); emulator diff:
 * tools/emu/check_pf8_cheats.sh. Calls get the four registers as the original has them.
 *
 * Globals: 0x004ffedc cheats enabled, 0x004ffee0..0x004ffefc / 0x004fff24..0x004fff2c / 0x004fff5c..
 * 0x004fff74 toggles, 0x004fff58 debug mode, 0x004e6be4 a second enable, 0x004e6a80 the message
 * counter. Calls: 0x0049e108 (multiplayer?), 0x00436040(text id, 0, -, 99) the message, 0x0046f8cc(sound,
 * 0, 0; stack counter) the sound, 0x0045afbc(0) the player car, 0x0043ac5c (after the car changes),
 * 0x004632b0 (a check), 0x0046f94c / 0x0046f958 at the end when 0x0049e108 is 0.
 */
#include "pf2_env.h"
#include "pf8_cheats.h"

#define R32 pf2_r32
#define W32 pf2_w32

typedef struct { Pf2Env* e; uint32_t a, d, b, c; } Regs;
static uint32_t call_n(Regs* r, uint32_t fn, int ns, uint32_t s0) {
    uint32_t v[5] = {r->a, r->d, r->b, r->c, s0};
    return r->a = r->e->call(r->e->user, fn, v, 4 + ns);
}
#define CALL(fn) call_n(R, (fn), 0, 0)
#define CNT 0x004e6a80u

static void msg(Regs* R, uint32_t id) { R->a = id; R->d = 0; R->c = 0x63; CALL(0x00436040u); }
static void snd(Regs* R, uint32_t s) {
    Pf2Env* e = R->e;
    uint32_t n = R32(e, CNT);
    R->b = 0;
    W32(e, CNT, n + 1);
    R->d = 0;
    R->a = s;
    call_n(R, 0x0046f8ccu, 1, n);
}
static uint32_t car(Regs* R) { R->a = 0; return CALL(0x0045afbcu); }
static uint32_t toggle(Regs* R, uint32_t g) {
    uint32_t t = R32(R->e, g) == 0;
    R->a = t;                                  /* sete al; and eax, 0xff */
    W32(R->e, g, t);
    return t;
}
/* the car reset of codes 0 and 31: each with the register the original stores (edx / ebx / edi = 0) */
static void car_reset(Regs* R) {
    Pf2Env* e = R->e;
    W32(e, car(R) + 0x1dc, 1);
    W32(e, car(R) + 0x848, 0);
    pf2_w8(e, car(R) + 0x851, 0);
    pf2_w8(e, car(R) + 0x850, 0);
    R->a = 0;
    R->d = 0;
    CALL(0x0045afbcu);
    CALL(0x0043ac5cu);
}
static void off_sound(Regs* R) { R->c = 0x63; snd(R, 0x3f); }   /* 0x0044e8f2 */

uint32_t pf8_0044e1cc(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs RR = {e, a, d, b, a};
    Regs* R = &RR;
    (void)c;
    uint32_t sel = a;
    if (CALL(0x0049e108u) == 0 && R32(e, 0x004e6be4u) == 0) return R->a;
    int on = R32(e, 0x004ffedcu) != 0;
    int dbg = R32(e, 0x004fff58u) != 0;
    int en2 = R32(e, 0x004e6be4u) != 0;
    if (sel <= 0x26) {
        switch (sel) {
        case 0:
            if (CALL(0x004632b0u) & 0xff) {
                msg(R, 0x31);
                snd(R, 0x3f);
                for (uint32_t o = 4; o <= 0x7c; o += 4) W32(e, 0x004ffed8u + o, 0);
                car_reset(R);
            } else if (toggle(R, 0x004ffedcu)) {
                msg(R, 0x2b);
                snd(R, 0x18);
            } else {
                msg(R, 0x2c);
                snd(R, 0x3f);
                for (uint32_t o = 4; o <= 0x7c; o += 4) W32(e, 0x004ffed8u + o, 0);
                car_reset(R);
            }
            break;
        case 1:
            if (!on) break;
            if (toggle(R, 0x004ffee0u)) {
                R->a = 0;
                W32(e, CALL(0x0045afbcu) + 0x848, 1);
                R->a = 0; R->d = 0;
                W32(e, CALL(0x0045afbcu) + 0x1dc, 0);
                R->a = 0x17; R->c = 0x63;
                CALL(0x00436040u);
                snd(R, 0x18);
            } else {
                W32(e, CALL(0x0045afbcu) + 0x848, 0);
                R->a = 0; R->d = 0;
                W32(e, CALL(0x0045afbcu) + 0x1dc, 1);
                R->a = 0x1b; R->c = 0x63;
                CALL(0x00436040u);
                snd(R, 0x3f);
            }
            break;
        case 2:
            if (!on) break;
            if (toggle(R, 0x004ffee4u)) {
                R->a = 0;
                uint32_t p = CALL(0x0045afbcu);
                R->d = 0;
                W32(e, p + 0x1dc, 0);
                R->a = 0x19; R->c = 0x63;
                CALL(0x00436040u);
                snd(R, 0x18);
            } else {
                W32(e, CALL(0x0045afbcu) + 0x1dc, 1);
                R->d = 0; R->a = 0x1d; R->c = 0x63;
                CALL(0x00436040u);
                snd(R, 0x3f);
            }
            break;
        case 3:
            if (!on) break;
            if (toggle(R, 0x004ffee8u)) { msg(R, 0x24); snd(R, 0x18); }
            else { msg(R, 0x25); snd(R, 0x18); }
            break;
        case 4:
            if (!on) break;
            if (toggle(R, 0x004ffeecu)) {
                R->a = 0;
                uint32_t p = CALL(0x0045afbcu);
                R->d = 0;
                W32(e, p + 0x848, 1);
                R->a = 0x1a; R->c = 0x63;
                CALL(0x00436040u);
                snd(R, 0x18);
            } else {
                W32(e, CALL(0x0045afbcu) + 0x848, 0);
                R->d = 0; R->a = 0x1e; R->c = 0x63;
                CALL(0x00436040u);
                snd(R, 0x3f);
            }
            break;
        case 5:
            if (!on) break;
            if (toggle(R, 0x004ffef0u)) { msg(R, 0x26); snd(R, 0x18); R->a = 0; R->d = 1; }
            else { msg(R, 0x27); snd(R, 0x3f); R->a = 0; R->d = 0; }
            CALL(0x0045afbcu);
            CALL(0x0043ac5cu);
            break;
        case 6:
            if (!on) break;
            if (!toggle(R, 0x004ffef4u)) break;
            msg(R, 0x28);
            snd(R, 0x18);
            {
                static const uint32_t off[4] = {0x85a, 0x85e, 0x860, 0x85c};
                for (int k = 0; k < 4; k++) { uint32_t p = car(R) + off[k]; pf2_w16(e, p, (uint16_t)(pf2_r16(e, p) + 10)); }
                uint32_t p = car(R);
                R->d = ((R->d & 0xffff0000u) | pf2_r16(e, p + 0x862)) + 10;
                pf2_w16(e, p + 0x862, (uint16_t)R->d);
                W32(e, 0x004ffef4u, 0);
            }
            break;
        case 7:
            if (!on) break;
            if (toggle(R, 0x004ffef8u)) {
                msg(R, 0x1f);
                snd(R, 0x18);
                pf2_w8(e, car(R) + 0x851, 1);
                uint32_t p = car(R) + 0x860;
                pf2_w16(e, p, (uint16_t)(pf2_r16(e, p) + 10));
            } else {
                R->c = 0x63;
                snd(R, 0x3f);
                pf2_w8(e, car(R) + 0x851, 0);
            }
            break;
        case 8:
            if (!on) break;
            if (toggle(R, 0x004ffefcu)) { msg(R, 0x18); snd(R, 0x18); pf2_w8(e, car(R) + 0x850, 1); }
            else { msg(R, 0x1c); snd(R, 0x3f); pf2_w8(e, car(R) + 0x850, 0); }
            break;
        case 18: case 19:
            if (!on && !dbg) break;
            if (!en2) break;
            if (toggle(R, sel == 18 ? 0x004fff24u : 0x004fff28u)) { R->c = 0x63; snd(R, 0x18); }
            else off_sound(R);
            break;
        case 20:
            if (!on) break;
            if (toggle(R, 0x004fff2cu)) { R->c = 0x63; snd(R, 0x18); }
            else off_sound(R);
            break;
        case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 28: case 29: case 30:
            W32(e, 0x004ffedcu + 4 * sel, 1);
            break;
        case 31:
            if (R32(e, 0x004fff58u) == 1) {
                msg(R, 0x2e);
                snd(R, 0x3f);
                for (uint32_t o = 0x80; o <= 0x9c; o += 4) W32(e, 0x004fff74u + o, 0);
                car_reset(R);
                W32(e, 0x004fff58u, 0);
            } else if (CALL(0x004632b0u) & 0xff) {
                msg(R, 0x2d);
                snd(R, 0x18);
                W32(e, 0x004fff58u, 1);
            }
            break;
        case 32:
            if (!on && !dbg) break;
            if (toggle(R, 0x004fff5cu)) { msg(R, 0x2f); snd(R, 0x18); }
            else { msg(R, 0x30); snd(R, 0x3f); }
            break;
        case 33: case 35: case 36: case 37:
            if (!dbg || !en2) break;
            {
                uint32_t g = sel == 33 ? 0x004fff60u : sel == 35 ? 0x004fff68u : sel == 36 ? 0x004fff6cu : 0x004fff70u;
                if (toggle(R, g)) { R->c = 0x63; snd(R, 0x18); }
                else off_sound(R);
            }
            break;
        case 34:
            if (!dbg || !en2) break;
            if (toggle(R, 0x004fff64u)) {
                W32(e, 0x004fff68u, 1); W32(e, 0x004fff6cu, 1); W32(e, 0x004fff70u, 1);
                R->c = 0x63;
                snd(R, 0x18);
            } else {
                W32(e, 0x004fff68u, 0); W32(e, 0x004fff6cu, 0); W32(e, 0x004fff70u, 0);
                R->c = 0x63;
                snd(R, 0x3f);
            }
            break;
        case 38:
            if (!dbg) break;
            if (toggle(R, 0x004fff74u)) { R->c = 0x63; snd(R, 0x18); }
            else off_sound(R);
            break;
        default:
            break;
        }
    }
    if (CALL(0x0049e108u) == 0) {
        CALL(0x0046f94cu);
        CALL(0x0046f958u);
    }
    return R->a;
}
