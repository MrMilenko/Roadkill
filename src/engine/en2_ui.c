/*
 * en2_ui.c: widget object lifetime, window procedure defaults, key queue and screen helpers of
 * TM2.EXE 0x00488000..0x004b0000 (see en2_ui.h). Verified under emulation: tools/emu/check_en2_ui.sh.
 */
#include "en2_ui.h"
#include "en2_leaf.h"
#include "en2_sys.h"

#define EN2_FreeResource     0x00c902c0u
#define EN2_GetSysColor      0x00c904e4u
#define EN2_SetTextColor     0x00c90210u
#define EN2_SetBkColor       0x00c90208u
#define EN2_SetBkMode        0x00c9020cu
#define EN2_GetAsyncKeyState 0x00c904b8u
#define EN2_CallWindowProcA  0x00c9046cu
#define EN2_DeleteObject     0x00c901dcu

uint32_t en2_widget_delete(En2Env* e, uint32_t o, uint32_t flags)
{
    if (flags & 4) {
        uint32_t p = en2_block_header(e, o, 0x004e5920u);
        en2_call1(e, 0x004aa39cu, p);
        return o;
    }
    uint32_t r = en2_call2(e, 0x004a3ff4u, o, 1);
    if (flags & 2) en2_call1(e, 0x004aa39cu, r);
    return r;
}
uint32_t en2_wndproc_size(En2Env* e, uint32_t o, uint32_t dx, uint32_t bx, uint32_t cx)
{
    uint32_t prev = en2_r32(e, o + 4);
    if (prev == 0) return 0;
    return en2_call5(e, EN2_CallWindowProcA, prev, en2_r32(e, o), 5 /* WM_SIZE */, cx,
                     ((bx & 0xffff) << 16) | (dx & 0xffff));
}
uint32_t en2_wndproc_command(En2Env* e, uint32_t o, uint32_t dx, uint32_t bx, uint32_t cx)
{
    uint32_t prev = en2_r32(e, o + 4);
    if (prev == 0) return 0;
    return en2_call5(e, EN2_CallWindowProcA, prev, en2_r32(e, o), 0x111 /* WM_COMMAND */,
                     ((bx & 0xffff) << 16) | (dx & 0xffff), cx);
}
uint32_t en2_gdi_free(En2Env* e, uint32_t o)
{
    if (en2_r32(e, o) != 0) {
        en2_call1(e, EN2_DeleteObject, en2_r32(e, o));
        en2_w32(e, o, 0);
    }
    uint32_t res = en2_r32(e, o + 0x10);
    if (res != 0) en2_call1(e, EN2_FreeResource, res);
    return o;
}
uint32_t en2_screen_lost_life3(En2Env* e, uint32_t v)
{
    uint32_t r = 0;
    for (int i = 0; i < 3; i++) {
        en2_display_lost_life(e, v);
        r = en2_display_495b14(e);
    }
    return r;
}
uint32_t en2_handle_close_notify(En2Env* e, uint32_t o)
{
    uint32_t r = en2_handle_close(e);
    if (r != 0) {
        en2_close_handle(e, en2_r32(e, 0x004e84d0u));
        en2_call2(e, en2_r32(e, en2_r32(e, o + 0x30)), o, 0x00ad3204u);
    }
    return r;
}
uint32_t en2_widget_base_create(En2Env* e, uint32_t a, uint32_t b)
{
    uint32_t o = en2_call2(e, 0x004a6ac8u, a, b);
    en2_w32(e, o + 0x34, 0x004e5f54u);
    en2_w32(e, o + 0x3c, en2_call1(e, EN2_GetSysColor, 0x12 /* COLOR_BTNTEXT */));
    uint32_t bk = en2_call1(e, EN2_GetSysColor, 0xf /* COLOR_BTNFACE */);
    en2_w32(e, o + 0x44, 0);
    en2_w32(e, o + 0x40, bk);
    en2_call2(e, en2_r32(e, en2_r32(e, o + 0x34) + 0xb8), o, en2_r32(e, o + 0x40));
    en2_w32(e, o + 0x48, 2);
    en2_w32(e, o + 0x4c, 0);
    return o;
}
uint32_t en2_widget_paint_colors(En2Env* e, uint32_t o, uint32_t hdc)
{
    en2_call2(e, EN2_SetTextColor, hdc, en2_r32(e, o + 0x3c));
    en2_call2(e, EN2_SetBkColor, hdc, en2_r32(e, o + 0x40));
    en2_call2(e, EN2_SetBkMode, hdc, en2_r32(e, o + 0x48));
    return en2_r32(e, o + 0x44);
}
uint32_t en2_pages_each(En2Env* e, uint32_t o)
{
    for (int32_t i = 0; i < (int32_t)en2_r32(e, o + 0x50c9c); i++) {
        en2_w32(e, o + 0x2cc, (uint32_t)i);
        en2_w32(e, o, en2_r32(e, o + 0x2dc + 4u * (uint32_t)i));
        en2_call2(e, 0x004a7c94u, o, (uint32_t)i + 1);
    }
    return o;
}
uint32_t en2_f10_key(En2Env* e, uint32_t ebx)
{
    if (ebx == 1) {
        uint16_t s = (uint16_t)en2_call1(e, EN2_GetAsyncKeyState, 0x79 /* VK_F10 */);
        if (s & 0x8001) {
            en2_w32(e, 0x00bdfcccu, 1);
            en2_w8(e, 0x00a5d754u, 1);
            return 2;
        }
    } else if (en2_r32(e, 0x00bdfcccu) != 0) {
        en2_w32(e, 0x00bdfcccu, 0);
        en2_w8(e, 0x00a5d754u, 0);
        return 2;
    }
    en2_w8(e, 0x00a5d748u, 1);
    return 2;
}
void en2_key_queue_push(En2Env* e, uint32_t k)
{
    int32_t head = (int32_t)en2_r32(e, 0x00a5da14u);
    int32_t tail = (int32_t)en2_r32(e, 0x00a5da1cu);
    en2_w8(e, 0x00a5d910u + (uint32_t)tail, (uint8_t)k);
    en2_w32(e, 0x00a5da10u, en2_r32(e, 0x00a5da10u) + 1);
    tail = tail < 0xff ? tail + 1 : 0;
    if (tail == head) {
        head++;
        if (head > 0xff) head = 0;
    }
    en2_w32(e, 0x00a5da1cu, (uint32_t)tail);
    en2_w32(e, 0x00a5da14u, (uint32_t)head);
}
