/*
 * pf8_boot.c: unlisted functions of TM2.EXE (not in Ghidra's function list, tools/rt/unlisted.tsv) that the
 * static constructors run before WinMain (the XI table at 0x004ea5fc) reach. Ported over Pf2Env
 * (pf2_env.h); emulator diff: tools/emu/check_pf8_boot.sh.
 *
 * Each takes the original's registers (eax a, edx d, ebx b, ecx c); calls get the four registers as the
 * original has them at that point (values left from earlier instructions included: the stubbed callees
 * keep their registers, and so do the game's Watcom callees; a Win32 call may change ecx / edx in the
 * real program, which the recorded values do not show).
 */
#include "pf2_env.h"
#include "pf8_boot.h"

#define R32 pf2_r32
#define W32 pf2_w32

typedef struct { Pf2Env* e; uint32_t a, d, b, c; } Regs;

static uint32_t call_n(Regs* r, uint32_t fn, int ns, const uint32_t* st) {
    uint32_t v[12] = {r->a, r->d, r->b, r->c};
    for (int k = 0; k < ns; k++) v[4 + k] = st[k];
    r->a = r->e->call(r->e->user, fn, v, 4 + ns);
    return r->a;
}
#define CALL(fn) call_n(&R, (fn), 0, NULL)
#define API(fn, ...) do { uint32_t st_[] = {__VA_ARGS__}; R.a = e->call(e->user, (fn), st_, (int)(sizeof st_ / 4)); } while (0)

#define F_NEW 0x004a8f48u
#define I_CreateMutexA 0x00c902acu       /* the 0x00c902ac import, 3 words */
#define I_lstrcpyA 0x00c9031cu
#define I_WaitForSingleObject 0x00c9030cu
#define I_ReleaseMutex 0x00c902fcu

/* 0x004aa3c0 session_ctor(eax this, edx): +0x24 / +4 = 0, +0 = edx; +0x2c = a 0x24 byte list (new +
   0x004acd40), +0x30 = a 0x25c byte object (new + 0x004ab73c(it, +0)), +0x34 = a 0x168 byte object (new +
   0x00498498), +0x38 = 0; +0x20 = 0x004acdb8(+0x2c) (the service provider count); +0x0c..+0x1c = 0 */
uint32_t pf8_004aa3c0(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t self = a;
    R.b = self;
    W32(e, self + 0x24, 0);
    W32(e, self + 4, 0);
    W32(e, self, d);
    R.a = 0x24;
    CALL(F_NEW);
    if (R.a) CALL(0x004acd40u);
    W32(e, self + 0x2c, R.a);
    R.a = 0x25c;
    CALL(F_NEW);
    if (R.a) { R.d = R32(e, self); CALL(0x004ab73cu); }
    W32(e, self + 0x30, R.a);
    R.a = 0x168;
    CALL(F_NEW);
    if (R.a) CALL(0x00498498u);
    W32(e, self + 0x34, R.a);
    R.a = R32(e, self + 0x2c);
    W32(e, self + 0x38, 0);
    CALL(0x004acdb8u);
    W32(e, self + 0xc, 0);
    W32(e, self + 0x14, 0);
    W32(e, self + 0x18, 0);
    W32(e, self + 0x1c, 0);
    W32(e, self + 0x10, 0);
    W32(e, self + 0x20, R.a);
    return self;
}

/* 0x004ab73c (eax this, edx owner): fields cleared, +4 = owner, three mutexes (0x004e3550 / 0x004e355d /
   0x004e356e names) -> +0x1f4 / +0x1f8 / +0x1fc, three empty strings (lstrcpyA from 0x004e357e..) at +0x5c /
   +0xdc / +0x15c, the second mutex taken and released once */
uint32_t pf8_004ab73c(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t s = a;
    W32(e, s, 0);
    W32(e, s + 0x1e0, 0); W32(e, s + 0x1e4, 0); W32(e, s + 0x1e8, 0); W32(e, s + 0x1ec, 0);
    W32(e, s + 0x214, 0); W32(e, s + 0x218, 0); W32(e, s + 0x21c, 0); W32(e, s + 0x220, 0);
    W32(e, s + 0x1f0, 0);
    W32(e, s + 4, d);
    API(I_CreateMutexA, 0, 0, 0x004e3550u);
    W32(e, s + 0x1f4, R.a);
    API(I_CreateMutexA, 0, 0, 0x004e355du);
    W32(e, s + 0x1f8, R.a);
    API(I_CreateMutexA, 0, 0, 0x004e356eu);
    W32(e, s + 0x200, 0);
    W32(e, s + 0x20c, 1);
    W32(e, s + 0x58, 0);
    W32(e, s + 0x1fc, R.a);
    W32(e, s + 0x1dc, 0);
    W32(e, s + 0x210, 1);
    API(I_lstrcpyA, s + 0x5c, 0x004e357eu);
    API(I_lstrcpyA, s + 0xdc, 0x004e357fu);
    API(I_lstrcpyA, s + 0x15c, 0x004e3580u);
    W32(e, s + 0x254, 0);
    W32(e, s + 0x258, 0);
    API(I_WaitForSingleObject, R32(e, s + 0x1f8), 0xffffffffu);
    W32(e, s + 0x208, 0);
    API(I_ReleaseMutex, R32(e, s + 0x1f8));
    return s;
}

/* 0x004acdb8 (eax list): frees the entries (chain +0x94, free 0x004aa39c), empties the list, enumerates
   the service providers (thunk 0x004d1902 = DirectPlayEnumerate(0x004acf14, list)), probes them
   (0x004acb70); returns the count +0x10 */
uint32_t pf8_004acdb8(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t l = a;
    (void)d;
    R.d = l;
    R.a = R32(e, l);
    W32(e, l + 8, R.a);
    if (R.a) {
        do {
            R.a = R32(e, l + 8);
            R.b = R32(e, R.a + 0x94);
            CALL(0x004aa39cu);
            W32(e, l + 8, R.b);
        } while (R.b);
    }
    W32(e, l, 0);
    W32(e, l + 4, 0);
    { uint32_t st[2] = {0x004acf14u, l}; call_n(&R, 0x004d1902u, 2, st); }
    R.a = l;
    CALL(0x004acb70u);
    return R32(e, l + 0x10);
}

/* 0x00498498 (eax this): +0x28 = 0, +0x2c = 1, +0 / +0x24 = 0, +4 = this + 0x30, +8..+0x20 = seven new(0x138) */
uint32_t pf8_00498498(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t s = a;
    R.c = s;
    W32(e, s + 0x28, 0);
    W32(e, s + 0x2c, 1);
    W32(e, s, 0);
    R.d = s + 4;
    W32(e, s + 0x24, 0);
    R.b = s + 0x20;
    W32(e, s + 4, s + 0x30);
    do {
        R.a = 0x138;
        R.d += 4;
        CALL(F_NEW);
        W32(e, R.d, R.a);
    } while (R.d != R.b);
    return s;
}

/* 0x004acb70 (eax list): with entries (+0x14 and +0x10 > 0): for each service provider, DirectPlayCreate
   (thunk 0x004d1908(guid, &dp, NULL)): it worked: +0x8c = 1, Close (slot 0x10) and Release; failed:
   0x004aac00(local, error), +0x8c = 0. 0x004aab90 / 0x004aabc4 open / close a local block (0x84 bytes).
   Returns 0x004aabc4's result. ORIGINAL BUG: a list shorter than its count reads [0 + 0x94]. */
uint32_t pf8_004acb70(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t l = a, loc = e->scratch, pp = e->scratch + 0x84;
    R.b = l;
    R.d = 0;
    R.a = loc;
    W32(e, pp, 0);
    CALL(0x004aab90u);
    if (R32(e, l + 0x14) != 0 && (int32_t)R32(e, l + 0x10) > 0) {
        W32(e, l + 8, R32(e, l));
        uint32_t i = 0;
        for (;;) {
            if (R32(e, l + 8) != 0) {
                uint32_t st[3] = {R32(e, R32(e, l + 8)), pp, 0};
                R.a = R32(e, l + 8);
                R.d = R32(e, R.a);
                call_n(&R, 0x004d1908u, 3, st);
                if (R.a != 0) {
                    R.d = R.a;
                    R.a = loc;
                    CALL(0x004aac00u);
                    W32(e, R32(e, l + 8) + 0x8c, 0);
                } else {
                    W32(e, R32(e, l + 8) + 0x8c, 1);
                }
                R.c = R32(e, pp);
                if (R.c) {
                    R.a = PF2_C0(e, PF2_IUNK, 0x10, R.c);
                    R.a = PF2_C0(e, PF2_IUNK, 8, R32(e, pp));
                    R.a = 0;
                    W32(e, pp, 0);
                }
            }
            i++;
            W32(e, l + 8, R32(e, R32(e, l + 8) + 0x94));
            if ((int32_t)i >= (int32_t)R32(e, l + 0x10)) break;
        }
    }
    R.a = loc;
    R.d = 0;
    return CALL(0x004aabc4u);
}

/* 0x004acc54 (eax list): a new 0x98 byte entry appended (chain +0x94; head +0, tail +4, cursor +8,
   count +0x10) while there are fewer than 20; returns it, else 0 */
uint32_t pf8_004acc54(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t l = a;
    (void)d; (void)b;
    R.d = l;
    R.c = R32(e, l + 0x10);
    R.b = 0;
    if ((int32_t)R.c >= 0x14) return 0;
    R.a = 0x98;
    CALL(F_NEW);
    uint32_t s = R32(e, l), n = R.a;
    if (s == 0) {
        W32(e, l, n);
        W32(e, n + 0x94, 0);
        W32(e, l + 4, n);
        W32(e, l + 8, n);
        W32(e, l + 0x10, 0);
    } else {
        W32(e, R32(e, l + 4) + 0x94, n);
        W32(e, l + 4, n);
        W32(e, n + 0x94, 0);
    }
    W32(e, l + 0x10, R32(e, l + 0x10) + 1);
    return n;
}

/* 0x004acf14 the DirectPlayEnumerate callback (stdcall: guid, name, major, minor, list; ret 0x14): a new
   entry (0x004acc54; none: returns 0) gets the name (+4), guid pointer (+0), versions (+0x84 / +0x88), its
   index (+0x90); names containing 0x004e35d0 / 0x004e35d6 / 0x004e35da mark the list (+0x14 bits 1 / 2 / 4,
   +0x20 / +0x18 / +0x1c = the index). Returns 1. */
uint32_t pf8_004acf14(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c, uint32_t guid, uint32_t name,
                      uint32_t major, uint32_t minor, uint32_t list) {
    Regs R = {e, a, d, b, c};
    R.b = list;
    R.a = list;
    CALL(0x004acc54u);
    uint32_t s = R.a;
    if (!s) return 0;
    API(I_lstrcpyA, s + 4, name);
    W32(e, s + 0x8c, 0);
    W32(e, s, guid);
    W32(e, s + 0x84, major);
    W32(e, s + 0x88, minor);
    W32(e, s + 0x90, R32(e, list + 0x10));
    R.d = 0x004e35d0u;
    R.a = name;
    if (CALL(0x004bc710u)) {
        W32(e, list + 0x20, R32(e, list + 0x10) - 1);
        pf2_w8(e, list + 0x14, pf2_r8(e, list + 0x14) | 1);
        return 1;
    }
    R.d = 0x004e35d6u;
    R.a = name;
    if (CALL(0x004bc710u)) {
        W32(e, list + 0x18, R32(e, list + 0x10) - 1);
        pf2_w8(e, list + 0x14, pf2_r8(e, list + 0x14) | 2);
        return 1;
    }
    R.d = 0x004e35dau;
    R.a = name;
    if (CALL(0x004bc710u)) {
        pf2_w8(e, list + 0x14, pf2_r8(e, list + 0x14) | 4);
        W32(e, list + 0x1c, R32(e, list + 0x10) - 1);
    }
    return 1;
}

/* 0x00496b60 static constructor: register (0x004a2e43), the text style 0x00a2dbb4 (0x004b2f80, ebx
   0x004e58a4), flag 0x004e8400 = 1, the 0x200 entry array 0x00a2dc48 (0x004bf848), flag = 2 */
uint32_t pf8_00496b60(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    R.a = 0x004e83f8u;
    CALL(0x004a2e43u);
    R.a = 0x00a2dbb4u;
    R.d = 1;
    R.b = 0x004e58a4u;
    CALL(0x004b2f80u);
    R.a = 0x00a2dc48u;
    W32(e, 0x004e8400u, R.d);
    R.d = 0x200;
    CALL(0x004bf848u);
    W32(e, 0x004e8400u, 2);
    return R.a;
}

/* 0x004a7b4c (eax this): +0, +0x2d8..+0x318, +0x2c8, +0x50c9c, +0x2cc, +0x2c4 = 0 */
uint32_t pf8_004a7b4c(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)d; (void)b; (void)c;
    W32(e, a, 0);
    for (uint32_t o = 0; o <= 0x40; o += 4) W32(e, a + 0x2d8 + o, 0);
    W32(e, a + 0x2c8, 0);
    W32(e, a + 0x50c9c, 0);
    W32(e, a + 0x2cc, 0);
    W32(e, a + 0x2c4, 0);
    return a;
}

/* 0x004ac6a4 (eax this): base constructor 0x004a6ac8, then +0x4c..+0x70 = 0, vtable +0x34 = 0x004e61d8,
   +0x38 = 1, +0x40 / +0x44 / +0x48 = 0 */
uint32_t pf8_004ac6a4(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t r = CALL(0x004a6ac8u);
    for (uint32_t o = 0x4c; o <= 0x70; o += 4) W32(e, r + o, 0);
    W32(e, r + 0x34, 0x004e61d8u);
    W32(e, r + 0x38, 1);
    W32(e, r + 0x40, 0);
    W32(e, r + 0x44, 0);
    W32(e, r + 0x48, 0);
    return r;
}

/* 0x004a5a50 (eax this): base 0x004a3fa0, vtable 0x004e5bd0, +0x54..+0x60, +0x64..+0x78 = 0 */
uint32_t pf8_004a5a50(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t r = CALL(0x004a3fa0u);
    W32(e, r + 0x34, 0x004e5bd0u);
    for (uint32_t o = 0x54; o <= 0x78; o += 4) W32(e, r + o, 0);
    return r;
}

/* 0x004b2f80 text style ctor (eax this): colors 0xff / 0xff / 0, shadow 2, background mode 1, height
   0x10, the face name from 0x004e3670 (lstrcpyA to +0x44); returns this */
uint32_t pf8_004b2f80(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t s = a;
    static const uint32_t v[][2] = {{0, 0xff}, {0x24, 1}, {0x28, 0x10}, {0xc, 0}, {0x10, 0}, {0x14, 0}, {4, 0xff},
                                    {8, 0xff}, {0x18, 0}, {0x1c, 0}, {0x20, 0}, {0x2c, 0}, {0x30, 0}, {0x34, 0},
                                    {0x38, 2}, {0x3c, 0}, {0x40, 0}};
    for (unsigned k = 0; k < sizeof v / sizeof v[0]; k++) W32(e, s + v[k][0], v[k][1]);
    API(I_lstrcpyA, s + 0x44, 0x004e3670u);
    return s;
}

/* 0x004b3ccc display_ctor (eax this): fields cleared, page flipping wanted (+0x6c) and full screen (+0x70)
   on, last ShowCursor 1, the mode list 0x00c848e0 (0x4cc bytes) and caps 0x00c84dac (0x1c8) cleared by
   memset 0x004bf830, the loading thread flag 0x00c85000 = 0; 0x004b3cc0 = the same on 0x00c84840 */
uint32_t pf8_004b3ccc(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t s = a;
    R.c = s;
    W32(e, s + 0x68, 0);
    W32(e, s + 0x6c, 1);
    static const uint32_t z[] = {8, 0xc, 0x10, 0x14, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x44, 0x48};
    for (unsigned k = 0; k < sizeof z / sizeof z[0]; k++) W32(e, s + z[k], 0);
    R.b = 0x4cc;
    R.d = 0;
    R.a = 0x00c848e0u;
    CALL(0x004bf830u);
    R.b = 0x1c8;
    R.a = 0x00c84dacu;
    R.d = 0;
    CALL(0x004bf830u);
    W32(e, s + 0x8c, 0);
    W32(e, s + 0x18, 0);
    W32(e, s + 0x90, 1);
    W32(e, s + 0x70, 1);
    W32(e, 0x00c85000u, 0);
    return s;
}
uint32_t pf8_004b3cc0(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)a;
    return pf8_004b3ccc(e, 0x00c84840u, d, b, c);
}

/* 0x004ba410 the voice list 0x00c85124: 0x34 slots built (0x004bf848, element ctor 0x004e62dc), then the
   list header fields (+0x8f4 = 1, +0x8f8 / +0x9c4 = -1, +0x9c8 = 0, +0x824 / +0x828 = 1, +0x82c..+0x8f0 = 0,
   +0xa94 = 24 voices, +0 = 0) relative to the result - 4; returns that */
uint32_t pf8_004ba410(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    R.b = 0x004e62dcu;
    R.d = 0x34;
    R.a = 0x00c85128u;
    CALL(0x004bf848u);
    uint32_t h = R.a - 4;
    W32(e, h + 0x8f4, 1);
    W32(e, h + 0x8f8, 0xffffffffu);
    W32(e, h + 0x9c4, 0xffffffffu);
    W32(e, h + 0x9c8, 0);
    W32(e, h + 0x824, 1);
    W32(e, h + 0x828, 1);
    for (uint32_t o = 0xc; o <= 0xd0; o += 4) W32(e, h + o + 0x820, 0);
    W32(e, h + 0xa94, 0x18);
    W32(e, h, 0);
    return h;
}

/* 0x004a4838 (eax this): base 0x004a4264, member constructors 0x004aa020 (ten from +0xdc by 0x14, two from
   +0x1a4; each on the previous one's returned eax), vtable 0x004e5d80, colors (+0xcc 0x808000, +0xd0
   0xffff00, +0xd4 0xffffff, +0xd8 0x7f7f7f), +0xbc / +0xc4 = 0, then its vtable slot 0xc0(this, 1) */
uint32_t pf8_004a4838(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    R.b = a;
    CALL(0x004a4264u);
    R.a += 0xdc;
    CALL(0x004aa020u);
    for (int k = 0; k < 9; k++) { R.a += 0x14; CALL(0x004aa020u); }
    R.b = R.a - 0x190;
    R.a = R.b + 0x1a4;
    CALL(0x004aa020u);
    R.a += 0x14;
    CALL(0x004aa020u);
    uint32_t s = R.b = R.a - 0x1b8;
    W32(e, s + 0x34, 0x004e5d80u);
    W32(e, s + 0xc4, 0);
    W32(e, s + 0xbc, 0);
    W32(e, s + 0xd0, 0xffff00);
    W32(e, s + 0xcc, 0x808000);
    W32(e, s + 0xd8, 0x7f7f7f);
    R.d = 1;
    R.c = R32(e, s + 0x34);
    R.a = s;
    W32(e, s + 0xd4, 0xffffff);
    CALL(R32(e, R.c + 0xc0));
    return s;
}
