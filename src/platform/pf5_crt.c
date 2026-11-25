/*
 * pf5_crt.c: Watcom C runtime routines of TM2.EXE (see pf5_crt.h).
 */
#include "pf5_crt.h"

static uint32_t len(Pf2Env* e, uint32_t s) {
    uint32_t n = 0;
    while (pf2_r8(e, s + n)) n++;
    return n;
}

uint32_t pf5_strstr(Pf2Env* e, uint32_t hay, uint32_t needle) {
    uint8_t c0 = pf2_r8(e, needle);
    if (c0 == 0) return hay;
    if (pf2_r8(e, needle + 1) == 0) {
        for (uint32_t p = hay;; p++) {
            uint8_t c = pf2_r8(e, p);
            if (c == c0) return p;
            if (c == 0) return 0;
        }
    }
    uint32_t end = hay + len(e, hay);
    uint32_t nl = len(e, needle);
    uint32_t p = hay;
    for (;;) {
        uint32_t rem = end - p;
        if (rem < nl) return 0;
        uint32_t hit = 0;
        for (uint32_t k = 0; k < rem; k++)
            if (pf2_r8(e, p + k) == c0) { hit = p + k; break; }
        if (hit == 0) return 0;
        uint32_t k = 0;
        while (k < nl && pf2_r8(e, hit + k) == pf2_r8(e, needle + k)) k++;
        if (k == nl) return hit;
        p = hit + 1;
    }
}

#define MH_FIRST 0x004e8e34u
#define MH_ROVER 0x004e8e38u
#define MH_LARGEST 0x004e8e3cu
#define MH_LAST 0x00c8b34cu
#define MH_BUSY 0x00c8bfacu

uint32_t pf5_malloc(Pf2Env* e, uint32_t size, uint32_t ds) {
    if (size == 0 || size > 0xffffffd4u) return 0;
    uint32_t want = (size + 3) & ~3u;
    int grown = 0;
    uint32_t r = 0;
    pf2_call0(e, 0x4e89e0);
    for (;;) {
        uint32_t n = want < 0xc ? 0xc : want;
        uint32_t h;
        if (n > pf2_r32(e, MH_LARGEST) && pf2_r32(e, MH_ROVER) != 0) {
            h = pf2_r32(e, MH_ROVER);
        } else {
            pf2_w32(e, MH_LARGEST, 0);
            h = pf2_r32(e, MH_FIRST);
        }
        for (; h; h = pf2_r32(e, h + 8)) {
            pf2_w32(e, MH_ROVER, h);
            r = pf2_call3(e, 0x4cb87c, size, ds & 0xffff, h);
            if (r) goto done;
            if (pf2_r32(e, h + 0x14) > pf2_r32(e, MH_LARGEST)) pf2_w32(e, MH_LARGEST, pf2_r32(e, h + 0x14));
        }
        if (!grown && pf2_call1(e, 0x4cbb33, size) != 0) {
            grown = 1;
            continue;
        }
        if (pf2_call1(e, 0x4cbb8e, size) == 0) break;
        grown = 0;
    }
done:
    pf2_w8(e, MH_BUSY, 0);
    pf2_call0(e, 0x4e89e8);
    return r;
}

void pf5_free(Pf2Env* e, uint32_t p, uint32_t ds) {
    if (p == 0) return;
    pf2_call0(e, 0x4e89e0);
    uint32_t h = pf2_r32(e, MH_LAST);
    if (!(h && h <= p && p < pf2_r32(e, h + 8))) {
        h = pf2_r32(e, MH_FIRST);
        for (uint32_t nx; (nx = pf2_r32(e, h + 8)) != 0; h = nx)
            if (h <= p && p < nx) break;
    }
    pf2_call3(e, 0x4cb924, p, ds & 0xffff, h);
    pf2_w32(e, MH_LAST, h);
    if (h < pf2_r32(e, MH_ROVER) && pf2_r32(e, h + 0x14) > pf2_r32(e, MH_LARGEST))
        pf2_w32(e, MH_LARGEST, pf2_r32(e, h + 0x14));
    pf2_w8(e, MH_BUSY, 0);
    pf2_call0(e, 0x4e89e8);
}

#define T_EXIT 0x004d18deu      /* ExitProcess thunk */
#define T_GETENV 0x004d18d8u    /* GetEnvironmentStrings */
#define T_MODNAME 0x004d18d2u   /* GetModuleFileNameA */
#define T_CMDLINE 0x004d18ccu   /* GetCommandLineA */
#define T_VERSION 0x004d18c6u   /* GetVersion */
#define CTYPE 0x004e633cu

static int blank(Pf2Env* e, uint32_t p) { return (pf2_r8(e, CTYPE + (uint8_t)(pf2_r8(e, p) + 1)) & 2) != 0; }

uint32_t pf5_crt_init_rtns(Pf2Env* e, uint32_t is_dll, uint32_t edx, uint32_t module) {
    pf2_w32(e, 0xc85bbc, is_dll);
    uint32_t td = pf2_call1(e, 0x4caf36, edx);
    pf2_w32(e, 0xc85bc0, td);
    if (td == 0) {
        if (is_dll) return 0;
        pf2_call1(e, T_EXIT, 1);
        return 0;
    }
    pf2_call0(e, 0x4cad92);
    pf2_w32(e, 0x4e8e79, pf2_call0(e, T_GETENV));
    uint32_t buf = e->scratch;
    pf2_call3(e, T_MODNAME, 0, buf, 0x104);
    pf2_w32(e, 0x4e8e4c, pf2_call1(e, 0x4cb209, buf));
    uint32_t p = pf2_call1(e, 0x4cb209, pf2_call0(e, T_CMDLINE));
    if (pf2_r8(e, p) == '"') {
        do p++; while (pf2_r8(e, p) != '"' && pf2_r8(e, p) != 0);
        if (pf2_r8(e, p) != 0) p++;
    } else {
        while (!blank(e, p) && pf2_r8(e, p) != 0) p++;
    }
    while (blank(e, p)) p++;
    pf2_w32(e, 0x4e8e48, p);
    if (is_dll) {
        pf2_call3(e, T_MODNAME, module, buf, 0x104);
        pf2_w32(e, 0x4e8e50, pf2_call1(e, 0x4cb209, buf));
    }
    uint32_t v = pf2_call0(e, T_VERSION);
    pf2_w16(e, 0x4e8e81, (uint16_t)v);
    pf2_w8(e, 0x4e8e7f, (uint8_t)(v >> 24));
    pf2_w8(e, 0x4e8e80, (uint8_t)((v >> 16) & 0xf));
    return 1;
}
