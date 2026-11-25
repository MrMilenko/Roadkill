/*
 * en2_sys.c: system / input / widget helpers of TM2.EXE 0x00488000..0x004b0000 (see en2_sys.h).
 * Verified under emulation: tools/emu/check_en2_sys.sh.
 */
#include "en2_sys.h"
#include "en2_leaf.h"
#include "en_misc.h"

#include <string.h>

#define EN2_DeleteObject     0x00c901dcu
#define EN2_CreateSolidBrush 0x00c901d4u
#define EN2_EndDialog        0x00c904a4u
#define EN2_RegOpenKeyExA    0x00c90140u

uint32_t en2_hw3d_query_b5fb8(En2Env* e)
{
    uint32_t r = 0;
    if (en2_hw3d_flag(e) == 1) r = en2_call1(e, 0x004b5fb8u, EN2_DISPLAY);
    return r;
}
uint32_t en2_hw3d_query_b5f94(En2Env* e)
{
    if (en2_hw3d_flag(e) == 0) {
        en2_call0(e, 0x004ad848u);
        return 0;
    }
    return en2_call1(e, 0x004b5f94u, EN2_DISPLAY);
}
uint32_t en2_texture_setup(En2Env* e, uint32_t a, uint32_t d)
{
    /* edx (the slot count) is not touched here: it reaches the callee as the caller left it */
    if (en2_hw3d_flag(e) == 0) return en2_call2(e, 0x00496774u, a, d);
    return en2_call2(e, 0x00496718u, a, d);
}
uint32_t en2_key_down(En2Env* e, uint32_t k)
{
    if (en2_r32(e, 0x00a5d5fcu) == 0) return 0;
    return en2_r8(e, 0x00a5d810u + (k & 0xff)) != 0;
}
uint32_t en2_key_take(En2Env* e, uint32_t k)
{
    if (en2_r32(e, 0x00a5d5fcu) == 0) return 0;
    if (en2_r8(e, 0x00a5d810u + (k & 0xff)) == 0) return 0;
    en2_w8(e, 0x00a5d810u + (k & 0xff), 0);
    return 1;
}
void en2_pairs_reset(En2Env* e)
{
    memset(e->mem(e->user, 0x00a81e4cu), 0, 0x100);
    memset(e->mem(e->user, 0x00a81f4cu), 0, 0x100);
    en2_call0(e, 0x0049e110u);
}
void en2_tables_204c_reset(En2Env* e)
{
    memset(e->mem(e->user, 0x00a8204cu), 0, 0x80);
    memset(e->mem(e->user, 0x00a820ccu), 0, 0x80);
    en2_call1(e, 0x004c3e54u, 0x00a82164u);
}
uint32_t en2_files_close_all(En2Env* e)
{
    for (uint32_t off = 0; off != 0x3000; off += 0x18)
        if (en2_r32(e, 0x00a2dc48u + off) != 0) en2_call1(e, 0x004c101cu, 0x00a2dc48u + off);
    return 0;
}
uint32_t en2_handle_close(En2Env* e)
{
    if (en2_r32(e, 0x004e8428u) != 0) {
        en2_call1(e, EN2_CloseHandle, en2_r32(e, 0x00a39c64u));
        en2_w32(e, 0x00a39c64u, 0);
    }
    return en2_r32(e, 0x004e8428u);
}
uint32_t en2_timer_cb_70(En2Env* e, uint32_t o, uint32_t edx)
{
    if (en2_r32(e, o + 0x70) != 0) {
        en2_call1(e, 0x0049e0c0u, en2_r32(e, o + 0x3c));
        en2_w32(e, o + 0x40, 0);
        en2_call3(e, en2_r32(e, o + 0x70), o, edx, en2_r32(e, o + 0x5c));
    }
    return 0;
}
uint32_t en2_timer_cb_68(En2Env* e, uint32_t o, uint32_t edx)
{
    en2_call1(e, 0x0049e0c0u, en2_r32(e, o + 0x3c));
    en2_w32(e, o + 0x40, 0);
    if (en2_r32(e, o + 0x68) != 0) en2_call3(e, en2_r32(e, o + 0x68), o, edx, en2_r32(e, o + 0x54));
    return 0;
}
uint32_t en2_timer_cb_60(En2Env* e, uint32_t o)
{
    if (en2_r32(e, o + 0x60) == 0) return 2;
    return en2_call3(e, en2_r32(e, o + 0x60), o, en2_r32(e, o + 0x4c), o);
}
uint32_t en2_obj_489078(En2Env* e, uint32_t o)
{
    en2_call1(e, 0x00484260u, 0x1d);
    en2_w32(e, o + 0x90, 0x9999);
    en2_w32(e, o + 0x98, 0);
    en2_w32(e, o + 0x94, en2_r32(e, o + 0x98));
    en2_w32(e, o + 0x8c, 0xe666);
    en2_call2(e, 0x00486bb4u, o, 2);
    return en2_call1(e, 0x0048859cu, o);
}
uint32_t en2_obj_free_slots(En2Env* e, uint32_t o)
{
    en2_call2(e, 0x00498380u, o, 0);
    for (uint32_t p = o + 8; ; p += 4) {
        en2_call1(e, 0x004aa39cu, en2_r32(e, p));
        if (p == o + 0x20) break;
    }
    return o;
}
uint32_t en2_brush_free(En2Env* e, uint32_t o)
{
    uint32_t b = en2_r32(e, o + 0x44);
    if (b) en2_call1(e, EN2_DeleteObject, b);
    en2_call1(e, 0x004a6c98u, o);
    return 0;
}
uint32_t en2_brush_set(En2Env* e, uint32_t o, uint32_t color)
{
    uint32_t b = en2_r32(e, o + 0x44);
    if (b) en2_call1(e, EN2_DeleteObject, b);
    en2_w32(e, o + 0x44, en2_call1(e, EN2_CreateSolidBrush, color));
    return 1;
}
uint32_t en2_reg_open(En2Env* e, uint32_t out, uint32_t root, uint32_t sub)
{
    if (sub == 0) return en2_call5(e, EN2_RegOpenKeyExA, root, 0x00bdfb38u, 0, 0x000f003fu, out);
    return en2_call5(e, EN2_RegOpenKeyExA, root, sub, 0, 0x000f003fu, out);
}
uint32_t en2_dialog_end(En2Env* e, uint32_t o, uint32_t result, uint32_t ecx)
{
    if (en2_r32(e, o + 0x54) != 1) return ecx;
    uint32_t r = en2_call2(e, EN2_EndDialog, en2_r32(e, o), result);
    en2_w32(e, o, 0);
    return r;
}
uint32_t en2_block_header(En2Env* e, uint32_t p, uint32_t edx)
{
    if (p == 0) return 0;
    en2_call3(e, 0x004c156cu, p, en2_r32(e, p - 4), edx);
    return p - 4;
}
uint32_t en2_dialog_key(En2Env* e, uint32_t o, uint32_t msg, uint32_t ebx, uint32_t ecx)
{
    if (msg == 0xd) {
        en2_call2(e, en2_r32(e, en2_r32(e, o + 0x34) + 0xd4), o, 1);
        return 0;
    }
    if (msg == 0x20) return 0;
    return en2_call4(e, 0x004a798cu, o, msg, ebx, ecx);
}
uint32_t en2_key_event(En2Env* e, uint32_t edx)
{
    if (en_render_mode_get((const int32_t*)(void*)e->mem(e->user, 0x004e8434u)) != 2) return 1;
    en2_w8(e, 0x00a5d710u + en2_r16(e, 0x004e9240u + 2u * edx), 1);
    if (edx == 0x10) en2_set_a5da20(e, 1);
    en2_call1(e, 0x0049ed48u, en2_r8(e, 0x004e9240u + 2u * edx));
    en2_call1(e, 0x0049dfacu, en2_r8(e, 0x004e9240u + 2u * edx));
    return 1;
}
