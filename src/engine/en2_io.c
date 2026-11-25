/*
 * en2_io.c: file loading thread, registry key, coordinate system allocation, input dialogs and
 * widget creation of TM2.EXE 0x00488000..0x004b0000 (see en2_io.h). Verified under emulation:
 * tools/emu/check_en2_io.sh.
 */
#include "en2_io.h"
#include "en2_leaf.h"
#include "en2_win.h"
#include "en2_ui.h"

#define EN2_CreateFileA      0x00c902a8u
#define EN2_GetFileSize      0x00c902d0u
#define EN2_CreateThread     0x00c902b4u
#define EN2_ReadFile         0x00c902f8u
#define EN2_SetWindowLongA   0x00c90578u
#define EN2_SetPropA         0x00c90570u
#define EN2_RegCreateKeyExA  0x00c90138u

#define LOAD_REQ   0x00a39c58u   /* {handle, buffer, size} handed to the loading thread */
#define LOAD_THR   0x00a39c64u   /* the thread handle */
#define LOAD_IDLE  0x004e8428u   /* 1 = no load running */

uint32_t en2_file_open(En2Env* e, uint32_t name, uint32_t size_out)
{
    uint32_t a[7] = {name, 0x80000000u /* GENERIC_READ */, 1 /* FILE_SHARE_READ */, 0, 3 /* OPEN_EXISTING */,
                     0x10000080u /* NORMAL | RANDOM_ACCESS */, 0};
    uint32_t h = e->call(e->user, EN2_CreateFileA, a, 7);
    if (h == 0xffffffffu) en2_w32(e, size_out, 0);
    else en2_w32(e, size_out, en2_call2(e, EN2_GetFileSize, h, 0));
    return h;
}
uint32_t en2_load_thread_start(En2Env* e, uint32_t h, uint32_t buf, uint32_t size)
{
    if (en2_r32(e, LOAD_THR) != 0) return h;
    uint32_t idle = en2_r32(e, LOAD_IDLE);
    if (idle != 1) return h;
    en2_w32(e, LOAD_REQ, h);
    en2_w32(e, LOAD_REQ + 4, buf);
    en2_w32(e, LOAD_REQ + 8, size);
    en2_w32(e, LOAD_IDLE, 0);
    uint32_t t = en2_call6(e, EN2_CreateThread, 0, 0, 0x00498c38u, LOAD_REQ, 0, e->scratch);
    en2_w32(e, LOAD_THR, t);
    if (t == 0) en2_w32(e, LOAD_IDLE, idle);
    return t;
}
uint32_t en2_load_thread_proc(En2Env* e, uint32_t req)
{
    en2_call5(e, EN2_ReadFile, en2_r32(e, req), en2_r32(e, req + 4), en2_r32(e, req + 8), e->scratch, 0);
    en2_w32(e, LOAD_IDLE, 1);
    return 0;
}
uint32_t en2_load_file_async(En2Env* e, uint32_t o, uint32_t name, uint32_t buf)
{
    uint32_t dst = buf ? buf : 0x00ad3204u;
    uint32_t v[4] = {o, en2_r32(e, o + 0x30), buf, dst};
    e->call(e->user, en2_r32(e, en2_r32(e, o + 0x30) + 0xc), v, 4);
    uint32_t h = en2_file_open(e, name, e->scratch + 0x10);
    en2_w32(e, 0x004e84d0u, h);
    if (h == 0xffffffffu) return 1;
    en2_load_thread_start(e, h, dst, en2_r32(e, e->scratch + 0x10));
    return 0;
}
uint32_t en2_reg_create(En2Env* e, uint32_t out, uint32_t root, uint32_t sub)
{
    uint32_t v[9] = {root, sub ? sub : 0x00bdfb38u, 0, 0, 0, 0x000f003fu, 0, out, e->scratch};
    return e->call(e->user, EN2_RegCreateKeyExA, v, 9);
}
uint32_t en2_cs_alloc(En2Env* e)
{
    int32_t n = (int32_t)en2_r32(e, 0x00a03644u);
    if (n >= 0x80) {
        en2_call2(e, 0x004a1f1eu, 0x004e2e14u, (uint32_t)n);  /* printf("internal error: too many coordinate systems") */
        return 0;
    }
    uint32_t p = 0x009f7e44u + 0x170u * (uint32_t)n;
    en2_call1(e, 0x00492c60u, p);
    en2_w32(e, 0x00a03644u, en2_r32(e, 0x00a03644u) + 1);
    return p;
}
uint32_t en2_ui_resume_6c(En2Env* e, uint32_t o, uint32_t edx)
{
    if (en2_r32(e, o + 0x6c) == 0) return 0;
    en2_w32(e, o + 0x3c, en2_get_a5d370(e));
    en2_input_mode_set(e, 0);
    en2_call1(e, EN2_DrawMenuBar, en2_r32(e, o));
    en2_w32(e, o + 0x40, 1);
    en2_call3(e, en2_r32(e, o + 0x6c), o, edx, en2_r32(e, o + 0x58));
    return 0;
}
uint32_t en2_ui_resume_64(En2Env* e, uint32_t o, uint32_t edx)
{
    en2_w32(e, o + 0x3c, en2_get_a5d370(e));
    en2_input_mode_set(e, 0);
    en2_call1(e, EN2_DrawMenuBar, en2_r32(e, o));
    en2_w32(e, o + 0x40, 1);
    if (en2_r32(e, o + 0x64) != 0) en2_call3(e, en2_r32(e, o + 0x64), o, edx, en2_r32(e, o + 0x50));
    return 0;
}
uint32_t en2_display_aca1c(En2Env* e, uint32_t edx, uint32_t ebx) { return en2_call3(e, 0x004b5b40u, EN2_DISPLAY, edx, ebx); }
uint32_t en2_list_add_item(En2Env* e, uint32_t o, uint32_t text, uint32_t data)
{
    uint32_t r = en2_call4(e, EN2_SendMessageA, en2_r32(e, o), 0x180 /* LB_ADDSTRING */, 0, text);
    if (r == 0xffffffffu || r == 0xfffffffeu) return r;
    return en2_call4(e, EN2_SendMessageA, en2_r32(e, o), 0x19a /* LB_SETITEMDATA */, r, data);
}
uint32_t en2_button_set_style(En2Env* e, uint32_t o, uint32_t style, uint32_t redraw)
{
    en2_w32(e, o + 0xc0, style);
    en2_w32(e, o + 0xc8, ((en2_r8(e, o + 0xc0) & 8) != 0) + 2);
    en2_set_68_sm_f4(e, o, (style & 0xffff0000u) | 0xb, redraw);
    if (redraw == 1) en2_invalidate(e, o, 0, 0);
    return 0;
}
uint32_t en2_window_subclass(En2Env* e, uint32_t o)
{
    en2_w32(e, o + 4, en2_call2(e, EN2_GetWindowLongA, en2_r32(e, o), (uint32_t)-4 /* GWL_WNDPROC */));
    en2_call3(e, EN2_SetWindowLongA, en2_r32(e, o), (uint32_t)-4, en2_r32(e, o + 8));
    return en2_call3(e, EN2_SetPropA, en2_r32(e, o), 0x004e31b2u /* "ST Object" */, o);
}
uint32_t en2_create_button(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t style,
                           uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    en2_w32(e, o + 0x68, style | 0x40000000u);
    uint32_t v[10] = {o, 0x004e30f6u /* "Button" */, edx, ebx, ecx, en2_r32(e, o + 0x68), x, y, w, h};
    return e->call(e->user, 0x004a4068u, v, 10);
}
uint32_t en2_widget_image_create(En2Env* e, uint32_t a, uint32_t b, uint32_t ebx, uint32_t ecx, uint32_t style,
                                 uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    uint32_t o = en2_widget_base_create(e, a, b);
    en2_w32(e, o + 0x34, 0x004e50fcu);
    uint32_t v[9] = {o, b, ebx, ecx, style | 0x40000000u, x, y, w, h};
    e->call(e->user, en2_r32(e, 0x004e50fcu + 0xc4), v, 9);
    return o;
}
