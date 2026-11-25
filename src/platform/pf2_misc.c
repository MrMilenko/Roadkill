/*
 * pf2_misc.c: small helpers of TM2.EXE's platform range (GDI text style setters, texture format
 * getter, number to text, memset front end), ported over Pf2Env (pf2_env.h). Emulator diff:
 * tools/emu/check_pf2_misc.sh. Arguments: the original's registers in order (eax, edx, ebx, ecx).
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32

/* 0x004b3b84 text_set_color(eax style, edx r, ebx g, ecx b): +4 = g, +8 = b, +0 = r; returns style */
uint32_t pf2_text_set_color(Pf2Env* e, uint32_t s, uint32_t r, uint32_t g, uint32_t b) {
    W32(e, s + 4, g);
    W32(e, s + 8, b);
    W32(e, s, r);
    return s;
}

/* 0x004b3bac text_set_shadow(eax style, edx on, ebx offset): +0x38 = offset, +0x34 = on */
uint32_t pf2_text_set_shadow(Pf2Env* e, uint32_t s, uint32_t on, uint32_t off) {
    W32(e, s + 0x38, off);
    W32(e, s + 0x34, on);
    return s;
}

uint32_t pf2_tex_format_bits(Pf2Env* e) { return R32(e, 0x00c8b35cu); }   /* 0x004bfed0 */

/* 0x004b276e crt_utoa(eax value, edx buffer, ebx radix): digits by unsigned division (the digit
   characters from the table at 0x004b2749 in the game image), written most significant first,
   NUL terminated; returns the buffer. Radix 0 faults (divide error) and radix 1 never ends (it
   writes past its 0x24 byte stack buffer) in the original: tested on radix 2..36. */
uint32_t pf2_crt_utoa(Pf2Env* e, uint32_t v, uint32_t buf, uint32_t radix) {
    uint8_t tmp[0x28];
    int n = 0;
    tmp[n++] = 0;
    do {
        uint32_t d = v % radix;
        v /= radix;
        tmp[n++] = pf2_r8(e, 0x004b2749u + d);
    } while (v != 0);
    uint32_t p = buf;
    do {
        n--;
        pf2_w8(e, p++, tmp[n]);
    } while (tmp[n] != 0);
    return buf;
}

/* 0x004b27b8 crt_itoa(eax value, edx buffer, ebx radix): radix 10 and negative: '-' and the
   negated value (INT_MIN stays 0x80000000); crt_utoa; returns the buffer */
uint32_t pf2_crt_itoa(Pf2Env* e, uint32_t v, uint32_t buf, uint32_t radix) {
    uint32_t p = buf;
    if (radix == 10 && (int32_t)v < 0) {
        v = 0u - v;
        pf2_w8(e, p++, '-');
    }
    pf2_crt_utoa(e, v, p, radix);
    return buf;
}

/* 0x004bf830 crt_memset(eax dst, edx byte (dl), ebx count): the byte spread over edx the way the
   original shifts it, then the runtime's fill 0x004cbba0(eax dst, edx pattern, ebx count, ecx
   count); returns dst */
uint32_t pf2_crt_memset(Pf2Env* e, uint32_t dst, uint32_t x, uint32_t n) {
    x = (x & 0xffff00ffu) | ((x & 0xff) << 8);       /* mov dh, dl */
    x <<= 8;
    x = (x & 0xffffff00u) | ((x >> 8) & 0xff);       /* mov dl, dh */
    x <<= 8;
    x = (x & 0xffffff00u) | ((x >> 8) & 0xff);
    pf2_call4(e, 0x004cbba0u, dst, x, n, n);
    return dst;
}
