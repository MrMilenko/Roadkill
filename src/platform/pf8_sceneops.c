/*
 * pf8_sceneops.c: four small unlisted TM2.EXE functions reached through data pointers (scene / screen object
 * vtables in DGROUP). Ported over Pf2Env (pf2_env.h); emulator diff: tools/emu/check_pf8_sceneops.sh.
 * Calls get the four registers as the original has them (leftovers included).
 *
 *   0x00485480 (ptr 0x004e573c)  this +0x14 . 0x00469cc4(the byte 0x00469cdc returns for this +0x10)
 *   0x004854c0 (ptr 0x004e5744)  when this +0x0c (word) is set: 0x0045222c (+0x58 set) or 0x00452204 with
 *                                eax +0x50 (byte), edx +0x5a, ebx +0x5c (words), ecx 0x00c8bfc8,
 *                                stack 0x00c8bfd4, +0x54, the word edx points at
 *   0x004858d0 (ptr 0x004e576c)  screen (this +0x0c) +0x48 = (+0x48 + 1) mod +0xb8; returns the quotient
 *   0x004a5234 (ptr 0x004e5921)  constructor: 0x004a3fa0, then vtable +0x34 = 0x004e5ca8 and zeroes
 *                                +0x54..+0x60, +0x64..+0x78
 */
#include "pf2_env.h"
#include "pf8_sceneops.h"

#define R32 pf2_r32
#define W32 pf2_w32

static uint32_t c4(Pf2Env* e, uint32_t fn, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    uint32_t v[4] = {a, d, b, c};
    return e->call(e->user, fn, v, 4);
}

uint32_t pf8_00485480(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)d; (void)b;
    uint32_t r = c4(e, 0x00469cdcu, R32(e, a + 0x10), a, 0, c) & 0xff;
    return c4(e, 0x00469cc4u, R32(e, a + 0x14), r, r, c);
}

uint32_t pf8_004854c0(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    if (pf2_r16(e, a + 0xc) == 0) return a;
    uint32_t fn = pf2_r8(e, a + 0x58) ? 0x0045222cu : 0x00452204u;
    uint32_t v[7] = {pf2_r8(e, a + 0x50), pf2_r16(e, a + 0x5a), pf2_r16(e, a + 0x5c), 0x00c8bfc8u,
                     0x00c8bfd4u, R32(e, a + 0x54), R32(e, d)};
    return e->call(e->user, fn, v, 7);
}

uint32_t pf8_004858d0(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)d; (void)b; (void)c;
    uint32_t o = R32(e, a + 0xc);
    int32_t n = (int32_t)(R32(e, o + 0x48) + 1u);
    int32_t m = (int32_t)R32(e, o + 0xb8);   /* the original faults on 0 (and INT_MIN / -1) */
    W32(e, o + 0x48, (uint32_t)(n % m));
    return (uint32_t)(n / m);
}

uint32_t pf8_004a5234(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    uint32_t o = c4(e, 0x004a3fa0u, a, d, b, c);
    W32(e, o + 0x34, 0x004e5ca8u);
    static const uint8_t z[] = {0x64, 0x54, 0x58, 0x5c, 0x60, 0x78, 0x68, 0x6c, 0x70, 0x74};
    for (unsigned i = 0; i < sizeof z; i++) W32(e, o + z[i], 0);
    return o;
}
