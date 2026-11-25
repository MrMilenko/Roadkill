/*
 * pf8_netscreen.c: the last reached unlisted TM2.EXE functions: the network screen step and four list view /
 * control wrappers the network dialog uses. Ported over Pf2Env (pf2_env.h); emulator diff:
 * tools/emu/check_pf8_netscreen.sh. Calls get the four registers as the original has them.
 *
 *   0x0047a308  the network screen (stored by screen_network_sub_a / _b): loads the background 0x004e28f2
 *               (0x00499408 into a 0x12c byte buffer, 0x004a2ea0 / 0x004a2ffc on the screen 0x009f6c20,
 *               0x00495b14), runs the dialog (0x00465f60, see pf8_netrun.c) and by its result (jump table
 *               0x0047a2f0) picks the next screen; the shared tail at 0x0047a164 hands it to 0x004a2fdc and
 *               returns it
 *   0x004a616c  a list view control constructor (base 0x004a3fa0, vtable 0x004e5a38, 0x004d190e, then its
 *               create slot +0xc4 with ecx | 0x40000001 and the four stack words; ret 0x10)
 *   0x004a6390  the item data (LVM_GETITEMA 0x1005, LVIF_PARAM) of row edx
 *   0x004a641c  the text of row edx, column ebx into ecx (LVM_GETITEMTEXTA 0x102d, stack: its size; ret 4)
 *   0x004a6480  inserts column s (LVM_INSERTCOLUMNA 0x101b: mask 0xf, fmt edx, cx ebx, text ecx; ret 4)
 */
#include "pf2_env.h"
#include "pf8_netscreen.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define SENDMSG 0x00c90550u

typedef struct { Pf2Env* e; uint32_t a, d, b, c; } Regs;
static uint32_t call4(Regs* r, uint32_t fn) {
    uint32_t v[4] = {r->a, r->d, r->b, r->c};
    return r->a = r->e->call(r->e->user, fn, v, 4);
}
#define CALL(fn) call4(&R, (fn))

uint32_t pf8_0047a308(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    static const uint32_t next[6] = {0x0047a18cu, 0x004780fcu, 0x0047a1f8u, 0x0047a274u, 0x004780fcu, 0x0047baa0u};
    Regs R = {e, a, d, b, c};
    uint32_t buf = e->scratch;
    R.b = 1;
    R.d = 0x004e28f2u;
    R.a = buf;
    CALL(0x00499408u);
    R.d = buf;
    R.a = 0x009f6c20u;
    R.b = 0;
    R.c = 0;
    CALL(0x004a2ea0u);
    R.a = 0x009f6c20u;
    R.b = 0;
    R.d = 0;
    CALL(0x004a2ffcu);
    CALL(0x00495b14u);
    uint32_t r = CALL(0x00465f60u);
    R.d = r <= 5 ? next[r] : 0x004780fcu;
    R.a = 0x009f6c20u;
    CALL(0x004a2fdcu);
    return R.d;
}

uint32_t pf8_004a616c(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c,
                      uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3) {
    uint32_t v[8] = {a, d, b, c};
    uint32_t o = v[0] = e->call(e->user, 0x004a3fa0u, v, 4);
    W32(e, o + 0x34, 0x004e5a38u);
    e->call(e->user, 0x004d190eu, v, 4);
    uint32_t vt = R32(e, o + 0x34);
    uint32_t w[8] = {o, d, b, c | 0x40000001u, s0, s1, s2, s3};
    e->call(e->user, R32(e, vt + 0xc4), w, 8);
    return o;
}

uint32_t pf8_004a6390(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    uint32_t it = e->scratch;
    W32(e, it + 4, d);
    W32(e, it, 4);
    static const uint8_t z[6] = {0x08, 0x14, 0x18, 0x0c, 0x10, 0x20};
    for (int k = 0; k < 6; k++) W32(e, it + z[k], 0);
    pf2_call4(e, SENDMSG, R32(e, a), 0x1005, 0, it);
    return R32(e, it + 0x20);
}

uint32_t pf8_004a641c(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c, uint32_t s) {
    uint32_t it = e->scratch;
    W32(e, it + 4, d);
    W32(e, it + 8, b);
    W32(e, it + 0x14, c);
    W32(e, it + 0x18, s);
    W32(e, it, 1);
    W32(e, it + 0xc, 0);
    W32(e, it + 0x10, 0);
    W32(e, it + 0x20, 0);
    return pf2_call4(e, SENDMSG, R32(e, a), 0x102d, d, it);
}

uint32_t pf8_004a6480(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c, uint32_t s) {
    uint32_t col = e->scratch;
    W32(e, col + 4, d);
    W32(e, col + 0x14, s);
    W32(e, col + 8, b);
    W32(e, col + 0xc, c);
    W32(e, col, 0xf);
    return pf2_call4(e, SENDMSG, R32(e, a), 0x101b, s, col);
}
