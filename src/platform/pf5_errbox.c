/*
 * pf5_errbox.c: win32_error_box, TM2.EXE 0x004bc660 (see pf5_errbox.h).
 */
#include "pf5_errbox.h"

#define TABLE 0x004e8840u

uint32_t pf5_error_box(Pf2Env* e, uint32_t index, uint32_t a1, uint32_t a2) {
    uint32_t ent = TABLE + 12u * (uint32_t)(int32_t)(int8_t)index;
    uint32_t buf = e->scratch, args = e->scratch + 0x100;
    pf2_w32(e, args, a1);
    pf2_w32(e, args + 4, a2);
    pf2_call3(e, 0xc90598, buf, pf2_r32(e, ent + 8), args);
    pf2_call1(e, 0xc90580, 1);
    uint32_t hwnd = pf2_call0(e, 0x4a8f34);
    uint32_t brush = pf2_call1(e, 0xc901ec, 4);
    uint32_t old = pf2_call3(e, 0xc90558, hwnd, 0xfffffff6u, brush);
    uint32_t r = pf2_call4(e, 0xc90524, hwnd, buf, pf2_r32(e, ent + 4), pf2_r32(e, ent));
    pf2_call1(e, 0xc90580, 0);
    pf2_call3(e, 0xc90558, hwnd, 0xfffffff6u, old);
    return r;
}
