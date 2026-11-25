/*
 * en7_l.c: see en7_l.h. Directories: 0x00a39c68 + 0x12c k for kinds 0..5; kind 6 = the game CD
 * (searched as 0x00499884 does, for the file named at 0x00a3a49c), "X:" written into the kind 6
 * slot; other kinds (and no CD: 0x004e307e) an empty directory string (0x004e3079). wsprintfA is
 * cdecl.
 */
#include "en7_l.h"

#define WSP 0x00c90658u

static uint32_t dir_for(En2Env* e, uint32_t kind, uint32_t root, uint32_t path)
{
    int8_t k = (int8_t)kind;
    if (k < 0 || k >= 7) return 0x004e3079u;
    if (k != 6) return 0x00a39c68u + (uint32_t)(int32_t)k * 0x12cu;
    uint32_t c;
    for (c = 0x41; c <= 0x5a; c++) {
        en2_call3(e, WSP, root, 0x004e2fefu, c);
        if (en2_call1(e, 0x00c902c8u, root) != 5) continue;
        en2_call4(e, WSP, path, 0x004e2ff4u, c, 0x00a3a49cu);
        uint32_t v[7] = {path, 0x80000000u, 1, 0, 3, 0x10000080u, 0};
        uint32_t h = e->call(e->user, 0x00c902a8u, v, 7);
        if (h == 0xffffffffu) continue;
        en2_call2(e, 0x00c902d0u, h, 0);
        en2_call1(e, EN2_CloseHandle, h);
        break;
    }
    if ((int32_t)c > 0x5a) c = 0xffffffffu;
    en2_w32(e, 0x00a3a5d0u, c);
    if ((int32_t)c < 0) return 0x004e307eu;
    uint32_t d = 0x00a39c68u + 6 * 0x12cu;
    en2_call3(e, WSP, d, 0x004e307au, c);
    return d;
}

/* 0x00499408 (file_build_path): dest = "<directory of kind (low byte, signed)>\<name>" (format
 * 0x004e3073); returns dest. */
uint32_t en7_file_build_path(En2Env* e, uint32_t dest, uint32_t name, uint32_t kind)
{
    uint32_t d = dir_for(e, kind, e->scratch + 0x130, e->scratch);
    en2_call4(e, WSP, dest, 0x004e3073u, d, name);
    return dest;
}

/* 0x00499564 (file_load_secondary): the same with the name taken from dest itself (copied first,
 * lstrcpyA into a stack buffer; format 0x004e307f). */
uint32_t en7_file_path_in_place(En2Env* e, uint32_t dest, uint32_t kind)
{
    const uint32_t copy = e->scratch + 0x140;
    en2_call2(e, 0x00c9031cu, copy, dest);
    uint32_t d = dir_for(e, kind, e->scratch + 0x130, e->scratch);
    en2_call4(e, WSP, dest, 0x004e307fu, d, copy);
    return dest;
}
