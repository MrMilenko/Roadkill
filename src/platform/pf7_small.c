/*
 * pf7_small.c: small functions of TM2.EXE that Ghidra never split into functions ("unlisted",
 * tools/rt/unlisted.tsv) and that the game reaches (vtable slots, static constructors, callbacks).
 * Ported over Pf2Env (pf2_env.h); emulator diff: tools/emu/check_pf7_small.sh.
 *
 * Every function takes the original's registers (eax a, edx d, ebx b, ecx c). Calls to other code
 * pass the registers the original has at that point; a tail jump passes all four. Where the
 * original reuses edx after a call (the static constructors' "flag = edx"), the port uses the value
 * edx had before the call (Watcom callees keep edx unless it is an argument they change; here it is
 * the "1" the original loaded, and the runtime's ports keep it).
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define C4(fn, a, d, b, c) pf2_call4(e, (fn), (a), (d), (b), (c))
#define ARGS Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c
#define UNUSED (void)e; (void)a; (void)d; (void)b; (void)c

/* ---- constant / empty vtable slots ---- */
#define KONST(addr, val) uint32_t pf7_##addr(ARGS) { UNUSED; return (val); }
KONST(00462f50, 0x00462d80u)   /* returns a function address (a callback table entry) */
KONST(00462f60, 0x00462d90u)
KONST(00462ff0, 2) KONST(00463000, 0) KONST(00463010, 2) KONST(00463020, 2) KONST(00463030, 2)
KONST(00463040, 2) KONST(00463050, 2) KONST(00463060, 2) KONST(00463070, 2) KONST(00463080, 2)
KONST(00463090, 2) KONST(004630a0, 2) KONST(004630b0, 2) KONST(004630c0, 2) KONST(004630d0, 2)
KONST(004630e0, 2) KONST(004630f0, 2) KONST(00463100, 2) KONST(00463110, 2) KONST(00463120, 2)
KONST(00463130, 2) KONST(00463140, 2) KONST(00463150, 2) KONST(00463160, 2) KONST(00463170, 2)
KONST(00463180, 2) KONST(00463190, 2) KONST(004631a0, 2) KONST(004631b0, 2) KONST(004631c0, 2)
KONST(004631d0, 2) KONST(004631e0, 2) KONST(004631f0, 1) KONST(00463200, 0)
KONST(00489980, 0x00488260u) KONST(00489990, 0x00489078u) KONST(004899a0, 0x00488fd8u)
KONST(004899b0, 2) KONST(004a69f0, 1)
/* "ret": eax unchanged */
#define EMPTY(addr) uint32_t pf7_##addr(ARGS) { UNUSED; return a; }
EMPTY(00462f80) EMPTY(0046b320) EMPTY(004b3010) EMPTY(004b7f90) EMPTY(004b9380)

/* ---- tail jumps with edx = 1 (destructors: "delete this") ---- */
uint32_t pf7_00462f90(ARGS) { (void)d; return C4(0x004a2e8cu, a, 1, b, c); }
uint32_t pf7_00477620(ARGS) { (void)d; return C4(0x004bb1e4u, a, 1, b, c); }

/* ---- constructors: base constructor, then the vtable field ---- */
static uint32_t ctor(ARGS, uint32_t base, uint32_t off, uint32_t vt) {
    uint32_t r = C4(base, a, d, b, c);
    W32(e, r + off, vt);
    return r;
}
uint32_t pf7_00462fa0(ARGS) { return ctor(e, a, d, b, c, 0x004a2e60u, 0x30, 0x004e50d8u); }
uint32_t pf7_00462fd0(ARGS) { return ctor(e, a, d, b, c, 0x004a3fa0u, 0x34, 0x004e50fcu); }
uint32_t pf7_00477630(ARGS) { return ctor(e, a, d, b, c, 0x004bb1b0u, 0x1c, 0x004e5700u); }
/* 0x004a6150 / 0x004a65d0: base 0x004a3fa0, vtable, then 0x004d190e(this) */
static uint32_t ctor2(ARGS, uint32_t vt) {
    uint32_t r = C4(0x004a3fa0u, a, d, b, c);
    W32(e, r + 0x34, vt);
    C4(0x004d190eu, r, d, r, c);
    return r;
}
uint32_t pf7_004a6150(ARGS) { return ctor2(e, a, d, b, c, 0x004e5a38u); }
uint32_t pf7_004a65d0(ARGS) { return ctor2(e, a, d, b, c, 0x004e5970u); }
/* 0x004a5f0c: base 0x004a3fa0, vtable 0x004e5b00, +0x54 / +0x5c / +0x60 / +0x64 / +0x58 = 0 */
uint32_t pf7_004a5f0c(ARGS) {
    uint32_t r = C4(0x004a3fa0u, a, d, b, c);
    W32(e, r + 0x34, 0x004e5b00u);
    W32(e, r + 0x54, 0);
    W32(e, r + 0x5c, 0);
    W32(e, r + 0x60, 0);
    W32(e, r + 0x64, 0);
    W32(e, r + 0x58, 0);
    return r;
}

/* ---- static constructors of global objects: register (0x004a2e43), construct, flag = 1 ---- */
static uint32_t sinit(ARGS, uint32_t reg, uint32_t obj, uint32_t fn, uint32_t flag) { (void)a;
    C4(0x004a2e43u, reg, d, b, c);
    uint32_t r = C4(fn, obj, 1, b, c);
    W32(e, flag, 1);
    return r;
}
uint32_t pf7_0046b330(ARGS) { return sinit(e, a, d, b, c, 0x004e7ba0u, 0x00510dd4u, 0x004b2f80u, 0x004e7ba8u); }
uint32_t pf7_00473da0(ARGS) { return sinit(e, a, d, b, c, 0x004e7ffcu, 0x00624edcu, 0x00485ebcu, 0x004e8004u); }
uint32_t pf7_004a21b0(ARGS) { return sinit(e, a, d, b, c, 0x004e848cu, 0x00a82164u, 0x004c3d90u, 0x004e8494u); }
uint32_t pf7_004a2620(ARGS) { return sinit(e, a, d, b, c, 0x004e849cu, 0x00a82560u, 0x004a7b4cu, 0x004e84a4u); }
uint32_t pf7_004a8ae0(ARGS) { return sinit(e, a, d, b, c, 0x004e84d4u, 0x00bdfb28u, 0x004c4530u, 0x004e84dcu); }
uint32_t pf7_004ac680(ARGS) { return sinit(e, a, d, b, c, 0x004e852cu, 0x00bdfcd0u, 0x004ac6a4u, 0x004e8534u); }
uint32_t pf7_004b8550(ARGS) { return sinit(e, a, d, b, c, 0x004e86b4u, 0x00c850bcu, 0x004b8574u, 0x004e86bcu); }
uint32_t pf7_004b9390(ARGS) { return sinit(e, a, d, b, c, 0x004e86c0u, 0x00c850ecu, 0x004b93b4u, 0x004e86c8u); }
/* 0x00477640 / 0x00485e90: the same with the constructor 0x004a2e60 and its vtable +0x30 */
static uint32_t sinit_vt(ARGS, uint32_t reg, uint32_t obj, uint32_t flag) { (void)a;
    C4(0x004a2e43u, reg, d, b, c);
    uint32_t r = C4(0x004a2e60u, obj, 1, b, c);
    W32(e, r + 0x30, 0x004e50d8u);
    W32(e, flag, 1);
    return r;
}
uint32_t pf7_00477640(ARGS) { return sinit_vt(e, a, d, b, c, 0x004e8014u, 0x009f6c20u, 0x004e801cu); }
uint32_t pf7_00485e90(ARGS) { return sinit_vt(e, a, d, b, c, 0x004e827cu, 0x009f6cd8u, 0x004e8284u); }
/* 0x00463240: register, 0x004aa3c0(0x0050f9a8, 0x004e5654), flag 0x004e7acc = 1 */
uint32_t pf7_00463240(ARGS) { (void)a;
    C4(0x004a2e43u, 0x004e7ac4u, d, b, c);
    uint32_t r = C4(0x004aa3c0u, 0x0050f9a8u, 0x004e5654u, b, c);
    W32(e, 0x004e7accu, 1);
    return r;
}
/* 0x004a6a39: once (0x004e91e4): 0x004c4510(0x004e3174, 1); returns its result, else eax */
uint32_t pf7_004a6a39(ARGS) { (void)d;
    if (pf2_r16(e, 0x004e91e4u) != 0) return a;
    pf2_w16(e, 0x004e91e4u, 1);
    return C4(0x004c4510u, 0x004e3174u, 1, b, c);
}
uint32_t pf7_004aa3a8(ARGS) { (void)a; (void)d; return C4(0x004c4510u, 0x004e3344u, 1, b, c); }
uint32_t pf7_00497990(ARGS) { (void)d; (void)b; return C4(0x004c156cu, a, 0x200, 0x004e58a4u, c); }

/* ---- small field functions ---- */
uint32_t pf7_00467594(ARGS) { UNUSED; W32(e, 0x0050fa14u, 1); W32(e, 0x0050fa10u, 1); return a; }
uint32_t pf7_004675a8(ARGS) { UNUSED; W32(e, 0x0050fa14u, 0); W32(e, 0x0050fa10u, 1); return a; }
uint32_t pf7_0046761c(ARGS) { (void)a; (void)d;
    C4(0x00466f44u, R32(e, 0x0050fa3cu), 0x004e16d5u, b, c);
    return C4(0x004a6f08u, R32(e, 0x0050fa3cu), 0x004e16d5u, b, c);   /* edx as the first call left it */
}
/* 0x00485440: x = 0x00469cdc(+0x10) & 0xff; +0x14's +0x54 = the +0x18 callback(x) */
uint32_t pf7_00485440(ARGS) { (void)d;
    uint32_t x = C4(0x00469cdcu, R32(e, a + 0x10), a, b, c) & 0xff;
    uint32_t r = C4(R32(e, a + 0x18), x, a, b, c);
    W32(e, R32(e, a + 0x14) + 0x54, r);
    return r;
}
uint32_t pf7_00485530(ARGS) { (void)b;
    W32(e, a + 8, 0x004e5740u);
    C4(0x00452d48u, pf2_r8(e, a + 0x50), d, a, c);
    return a;
}
uint32_t pf7_00485610(ARGS) { UNUSED; uint8_t v = pf2_r8(e, a + 0x14); pf2_w8(e, R32(e, a + 0x10), v); return (a & 0xffffff00u) | v; }
uint32_t pf7_00485900(ARGS) { (void)d; return C4(0x0046a0d8u, R32(e, a + 0xc), R32(e, a + 0x10), b, c); }
uint32_t pf7_00485a50(ARGS) { UNUSED; uint32_t p = R32(e, a + 0xc); pf2_w16(e, p + 0xc, 0); return p; }
uint32_t pf7_00485a80(ARGS) { UNUSED; uint32_t p = R32(e, a + 0xc); pf2_w16(e, p + 0xc, 1); return p; }
uint32_t pf7_00485dc0(ARGS) { UNUSED; W32(e, a + 4, d); return a; }
/* 0x00498534 table entry: index 0..7 (signed) -> [eax + 4 + 4 i], else 0 */
uint32_t pf7_00498534(ARGS) { UNUSED; return ((int32_t)d < 0 || (int32_t)d >= 8) ? 0 : R32(e, a + 4 + 4 * d); }
uint32_t pf7_004a2e33(ARGS) { UNUSED; pf2_w8(e, a + 8, 0); return a; }
/* list view messages: SendMessageA([eax], msg, edx, lparam) */
uint32_t pf7_004a6244(ARGS) { (void)b; (void)c; return pf2_call4(e, 0x00c90550u, R32(e, a), 0x1008, d, 0); }
uint32_t pf7_004a6404(ARGS) { (void)b; (void)c; return pf2_call4(e, 0x00c90550u, R32(e, a), 0x102c, d, 0xf0f); }
/* 0x004a6540: a 0x24 byte structure on the stack with +0x0c = ecx, +0x10 = ebx (the rest unset) */
uint32_t pf7_004a6540(ARGS) {
    uint32_t s = e->scratch;
    W32(e, s + 0x10, b);
    W32(e, s + 0xc, c);
    return pf2_call4(e, 0x00c90550u, R32(e, a), 0x102b, d, s);
}
/* 0x004b27d3 crt_sprintf's character writer: *(*p)++ = dl; p[4]++ */
uint32_t pf7_004b27d3(ARGS) {
    UNUSED;
    uint32_t q = R32(e, a);
    W32(e, a, q + 1);
    pf2_w8(e, q, (uint8_t)d);
    W32(e, a + 0x10, R32(e, a + 0x10) + 1);
    return a;
}
uint32_t pf7_004b8574(ARGS) { UNUSED; W32(e, a + 0x1c, 1); W32(e, a + 0x20, 0); W32(e, a + 0x18, 0); return a; }
uint32_t pf7_004b93b4(ARGS) { UNUSED; W32(e, a + 0x10, 0xffffffffu); W32(e, a + 0x1c, 0); W32(e, a + 0x14, 1); return a; }
uint32_t pf7_004ba49c(ARGS) { UNUSED; W32(e, a + 0x40, 0); W32(e, a, 0xffffffffu); return a; }   /* sample init */
uint32_t pf7_004baa68(ARGS) { UNUSED; W32(e, a + 0x14, 0); W32(e, a + 0x10, 0); return a; }        /* voice init */
uint32_t pf7_004bae1c(ARGS) { UNUSED; W32(e, a + 4, 0); W32(e, a + 8, 0); W32(e, a, 0); return a; } /* group init */
uint32_t pf7_004ba4ac(ARGS) { (void)b; (void)d;   /* sample release: dsbuf_release 0x004b98cc of +0x40 */
    if (R32(e, a + 0x40)) C4(0x004b98ccu, 0x00c850ecu, R32(e, a + 0x40), a, c);
    return a;
}
uint32_t pf7_004bae34(ARGS) { (void)b; (void)d;   /* group release: free +8 */
    if (R32(e, a + 8)) { C4(0x004aa018u, R32(e, a + 8), R32(e, a + 8), a, c); W32(e, a + 8, 0); }
    return a;
}
uint32_t pf7_004acd40(ARGS) {
    UNUSED;
    W32(e, a + 0xc, 0); W32(e, a + 0x14, 0);
    W32(e, a + 0x18, 0xffffffffu); W32(e, a + 0x1c, 0xffffffffu); W32(e, a + 0x20, 0xffffffffu);
    W32(e, a, 0); W32(e, a + 8, 0); W32(e, a + 4, 0); W32(e, a + 0x10, 0);
    return a;
}
uint32_t pf7_004bba30(ARGS) {
    UNUSED;
    for (uint32_t o = 4; o <= 0xc; o += 4) W32(e, a + o, 0);
    for (uint32_t o = 0x1c; o <= 0x24; o += 4) W32(e, a + o, 0);
    W32(e, a, 0);
    return a;
}
uint32_t pf7_004bfed8(ARGS) { UNUSED; W32(e, a + 8, 0); W32(e, a + 0x10, 0); W32(e, a, 0); W32(e, a + 0x14, 0); W32(e, a + 0xc, 0); return a; }
uint32_t pf7_004bfefc(ARGS) { UNUSED; W32(e, a, 0); return a; }
