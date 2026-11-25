/*
 * en2_leaf.c: small leaves of TM2.EXE 0x00488000..0x004b0000 (see en2_leaf.h for the list and
 * what each one touches). Verified under emulation: tools/emu/check_en2_leaf.sh.
 */
#include "en2_leaf.h"
#include "en_widget.h"

#include <string.h>

/* ---- empty functions ---- */
void en2_nop(En2Env* e) { (void)e; }

/* ---- global getters / setters ---- */
void en2_set_4e8288(En2Env* e, uint32_t v) { en2_w32(e, 0x004e8288u, v); }
uint32_t en2_cs_get_root(En2Env* e) { return en2_r32(e, 0x009f7e38u); }
void en2_cs_set_model(En2Env* e, uint32_t cs, uint32_t model) { en2_w32(e, cs + 0x15c, model); }
uint32_t en2_frame_get_ticks60(En2Env* e) { return en2_r16(e, 0x004e8300u); }
uint32_t en2_frame_get_fps(En2Env* e) { return en2_r32(e, 0x004e8304u); }
uint32_t en2_frame_parity(En2Env* e) { return en2_r32(e, 0x00a0364cu); }
uint32_t en2_get_a03a70(En2Env* e) { return en2_r32(e, 0x00a03a70u); }
uint32_t en2_get_a03a8c_byte(En2Env* e) { return en2_r8(e, 0x00a03a8cu); }
uint32_t en2_get_a2dba5_byte(En2Env* e) { return en2_r8(e, 0x00a2dba5u); }
uint32_t en2_get_a3a5d0(En2Env* e) { return en2_r32(e, 0x00a3a5d0u); }
void en2_set_4e8430(En2Env* e, uint32_t v) { en2_w32(e, 0x004e8430u, v); }
uint32_t en2_get_a5d370(En2Env* e) { return en2_r32(e, 0x00a5d370u); }
void en2_set_a5da20(En2Env* e, uint32_t v) { en2_w32(e, 0x00a5da20u, v); }
void en2_dpc_set_base(En2Env* e, uint32_t v) { en2_w32(e, 0x00a5eb60u, v); }
uint32_t en2_dpc_get_base(En2Env* e) { return en2_r32(e, 0x00a5eb60u); }
void en2_set_screen_ptr(En2Env* e, uint32_t v) { en2_w32(e, 0x00a04674u, v); }
uint32_t en2_flag_a03a7c(En2Env* e) { return en2_r32(e, 0x00a03a7cu) != 0; }
uint32_t en2_flag_a03a80(En2Env* e) { return en2_r32(e, 0x00a03a80u) != 0; }
uint32_t en2_flag_a03a84(En2Env* e) { return en2_r32(e, 0x00a03a84u) != 0; }
void en2_resource_init(En2Env* e, uint32_t a, uint32_t b)
{
    en2_w32(e, 0x00a3a5ccu, b);
    en2_w32(e, 0x00a3a5c8u, a);
}
uint32_t en2_key_get(En2Env* e, uint32_t k) { return en2_r8(e, 0x00a5d810u + (k & 0xff)); }
void en2_key_set(En2Env* e, uint32_t k) { en2_w8(e, 0x00a5d810u + (k & 0xff), 1); }
void en2_key_counters_clear(En2Env* e)
{
    en2_w32(e, 0x00a5da14u, 0);
    en2_w32(e, 0x00a5da1cu, 0);
    en2_w32(e, 0x00a5da10u, 0);
}
uint32_t en2_lookup_a03b78(En2Env* e, uint32_t i)
{
    uint32_t k = en2_r32(e, 0x00a03ae4u + 4u * i);
    return en2_r32(e, 0x00a03b78u + 4u * (37u * k));
}
void en2_slots_clear(En2Env* e)
{
    for (uint32_t off = 0x228; ; off += 0x228) {
        en2_w32(e, 0x00a3a610u + off, 0);
        if (off == 0x2058) break;
    }
    en2_w32(e, 0x004e842cu, 0xffffffffu);
}
void en2_resource_mgr_init(En2Env* e)
{
    memset(e->mem(e->user, 0x00a30c48u), 0, 0x6000);
    en2_w32(e, 0x00a36c48u, 0);
    en2_w32(e, 0x00a36c54u, 0);
}
void en2_keys_clear(En2Env* e)
{
    memset(e->mem(e->user, 0x00a5d810u), 0, 0x100);
    en2_w32(e, 0x00a5da10u, 0);
    en2_w32(e, 0x00a5da14u, 0);
    en2_w32(e, 0x00a5da1cu, 0);
}

/* ---- object accessors ---- */
uint32_t en2_ret1(En2Env* e) { (void)e; return 1; }
uint32_t en2_addr_c4(En2Env* e, uint32_t o) { (void)e; return o + 0xc4; }
uint32_t en2_addr_44(En2Env* e, uint32_t o) { (void)e; return o + 0x44; }
uint32_t en2_get_24(En2Env* e, uint32_t o) { return en2_r32(e, o + 0x24); }
void en2_set_c_1(En2Env* e, uint32_t o) { en2_w32(e, o + 0xc, 1); }
uint32_t en2_get_1c(En2Env* e, uint32_t o) { return en2_r32(e, o + 0x1c); }
void en2_set_254(En2Env* e, uint32_t o, uint32_t v) { en2_w32(e, o + 0x254, v); }
void en2_set_258(En2Env* e, uint32_t o, uint32_t v) { en2_w32(e, o + 0x258, v); }
uint32_t en2_game_tick(En2Env* e, uint32_t o) { return en2_r32(e, o + 0x38); }
uint32_t en2_get_10(En2Env* e, uint32_t o) { return en2_r32(e, o + 0x10); }
void en2_component_clear(En2Env* e, uint32_t o)
{
    en2_w32(e, o + 0x10, 0);
    en2_w32(e, o + 0xc, 0);
    en2_w32(e, o + 8, 0);
    en2_w32(e, o, 0);
}
void en2_rect6_reset(En2Env* e, uint32_t o)
{
    en2_w32(e, o + 4, 0);
    en2_w32(e, o + 8, 0);
    en2_w32(e, o + 0xc, 0);
    en2_w32(e, o + 0x10, 0);
    en2_w32(e, o + 0x14, 1);
    en2_w32(e, o, 0);
}
void en2_cs_list_add(En2Env* e, uint32_t cs)
{
    uint32_t head = en2_r32(e, 0x009f7e34u);
    en2_w32(e, 0x009f7e34u, cs);
    en2_w32(e, cs + 0x160, head);
}

/* ---- forwards to the display object 0x00c84840 and other unported code ---- */
uint32_t en2_screen_get_width(En2Env* e) { return en2_call1(e, 0x004b5a14u, EN2_DISPLAY); }
uint32_t en2_screen_get_height(En2Env* e) { return en2_call1(e, 0x004b5a10u, EN2_DISPLAY); }
uint32_t en2_display_setup(En2Env* e) { return en2_call1(e, 0x004bf76fu, en2_r32(e, 0x00a05678u)); }
uint32_t en2_display_495b14(En2Env* e) { return en2_call1(e, 0x004b5128u, EN2_DISPLAY); }
uint32_t en2_display_495b20(En2Env* e, uint32_t v) { return en2_call2(e, 0x004b52b8u, EN2_DISPLAY, v); }
uint32_t en2_input_release(En2Env* e) { return en2_call1(e, 0x004b480cu, EN2_DISPLAY); }
uint32_t en2_display_496190(En2Env* e, uint32_t v) { return en2_call2(e, 0x004b4a54u, EN2_DISPLAY, v); }
uint32_t en2_display_mode(En2Env* e, uint32_t v) { return en2_call2(e, 0x004b50f8u, EN2_DISPLAY, v); }
uint32_t en2_display_495b60(En2Env* e) { return en2_call2(e, 0x004b4e38u, EN2_DISPLAY, 0); }
uint32_t en2_display_lost_life(En2Env* e, uint32_t v)
{
    return en2_call3(e, 0x004b4d98u, EN2_DISPLAY, v & 0xffff, 0);
}
uint32_t en2_display_width(En2Env* e, uint32_t a, uint32_t b, uint32_t c)
{
    return en2_call5(e, 0x004b45a0u, EN2_DISPLAY, a, b, c, 0);
}
uint32_t en2_display_rect_24(En2Env* e, uint32_t o)
{
    en2_call2(e, 0x004b48b8u, EN2_DISPLAY, o + 0x24);
    return 0;
}
uint32_t en2_hw3d_flag(En2Env* e) { return en2_call0(e, 0x004b5f60u) != 0; }
uint32_t en2_crt_rand_state_ptr(En2Env* e) { return en2_call0(e, en2_r32(e, 0x004e89c4u)) + 0xc; }
uint32_t en2_polygon_renderer_wrapper(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s)
{
    uint32_t v[5] = {eax, edx, ebx, ecx, s};
    return e->call(e->user, 0x0048e9b8u, v, 5);
}
uint32_t en2_widget_4a4bdc(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s)
{
    return en2_call5(e, 0x004a4b50u, eax, edx & 0xffff, ebx & 0xffff, ecx & 0xffff, s & 0xffff);
}
uint32_t en2_widget_4a9dd0(En2Env* e, uint32_t a, uint32_t b)
{
    uint32_t o = en2_call2(e, 0x004a3fa0u, a, b);
    en2_w32(e, o + 0x34, 0x004e60e8u);
    en2_w32(e, o + 0x54, b);
    return o;
}
uint32_t en2_widget_4a41f0(En2Env* e, uint32_t o)
{
    if (en2_r32(e, o + 0x38) == 1) en2_call1(e, 0x004a6f80u, o);
    return 2;
}
uint32_t en2_widget_4a2e8c(En2Env* e, uint32_t o)
{
    en2_w32(e, o + 0x30, 0x004e58fcu);
    en2_call1(e, en2_r32(e, 0x004e58fcu + 0xc), o);
    return o;
}
uint32_t en2_4acb1c(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx)
{
    if (edx == 1) {
        en2_call4(e, 0x004ba39cu, eax, edx, ebx, ecx);
        return 0;
    }
    return 2;
}
uint32_t en2_timer_check_timeout(En2Env* e)
{
    uint8_t d = (uint8_t)en2_call0(e, 0x004a2380u);
    d |= (uint8_t)en2_call0(e, 0x004a28a8u);
    return (uint8_t)(en2_call0(e, 0x004a2068u) | d);
}
uint32_t en2_input_4a2068(En2Env* e)
{
    uint8_t d = 0;
    uint32_t r = en2_call1(e, 0x004932a0u, 1);
    if ((uint8_t)r == 1) d = (uint8_t)en2_call1(e, 0x0049ecd0u, r);
    return d;
}
void en2_key_from_pair(En2Env* e, uint32_t i, int32_t k)
{
    if (k < 0 || k >= 8) return;
    EnWidgetEnv w = {e->mem, e->user};
    en2_key_set(e, en_wg_pair_get_a(&w, i & 0xff));
}
uint32_t en2_shutdown_final(En2Env* e, uint32_t code)
{
    uint32_t r = en2_call1(e, en2_r32(e, 0x004e8484u), code);
    r = en2_call1(e, en2_r32(e, 0x004e8488u), r);
    uint32_t f = en2_r32(e, 0x004e8fccu);
    if (f) en2_call1(e, f, r);
    return en2_call1(e, 0x004bc957u, code);
}
uint32_t en2_shutdown_dispatch(En2Env* e, uint32_t code)
{
    uint32_t r = en2_call1(e, en2_r32(e, 0x004e8480u), code);
    en2_call1(e, en2_r32(e, 0x004e8484u), r);
    return en2_shutdown_final(e, code);
}

/* ---- Win32 wrappers (hwnd = [obj]) ---- */
static uint32_t sm(En2Env* e, uint32_t o, uint32_t msg, uint32_t w, uint32_t l)
{
    return en2_call4(e, EN2_SendMessageA, en2_r32(e, o), msg, w, l);
}
#define SM_W(name, msg) uint32_t name(En2Env* e, uint32_t o, uint32_t v) { return sm(e, o, msg, v, 0); }
#define SM_L(name, msg) uint32_t name(En2Env* e, uint32_t o, uint32_t v) { return sm(e, o, msg, 0, v); }
#define SM_WL(name, msg) uint32_t name(En2Env* e, uint32_t o, uint32_t w, uint32_t l) { return sm(e, o, msg, w, l); }
#define SM_0(name, msg) uint32_t name(En2Env* e, uint32_t o) { return sm(e, o, msg, 0, 0); }
SM_W(en2_sm_f1, 0xf1)
SM_W(en2_sm_f3, 0xf3)
SM_L(en2_sm_180, 0x180)
SM_W(en2_sm_182, 0x182)
SM_WL(en2_sm_1a2, 0x1a2)
SM_W(en2_sm_199, 0x199)
SM_WL(en2_sm_181, 0x181)
SM_W(en2_sm_186, 0x186)
SM_L(en2_sm_143, 0x143)
SM_W(en2_sm_14e, 0x14e)
SM_L(en2_sm_c5, 0xc5)
SM_WL(en2_sm_1013, 0x1013)
SM_L(en2_sm_1024, 0x1024)
SM_L(en2_sm_1026, 0x1026)
SM_L(en2_sm_1001, 0x1001)
SM_L(en2_sm_415, 0x415)
SM_WL(en2_sm_405, 0x405)
SM_WL(en2_sm_414, 0x414)
SM_0(en2_sm_f5, 0xf5)
SM_0(en2_sm_f0, 0xf0)
SM_0(en2_sm_f2, 0xf2)
SM_0(en2_sm_18b, 0x18b)
SM_0(en2_sm_184, 0x184)
SM_0(en2_sm_147, 0x147)
SM_0(en2_sm_1004, 0x1004)
SM_0(en2_sm_400, 0x400)
uint32_t en2_sm_gettext(En2Env* e, uint32_t o, uint32_t buf, uint32_t len) { return sm(e, o, 0xd, len, buf); }
uint32_t en2_sm_406(En2Env* e, uint32_t o, uint32_t lo, uint32_t hi, uint32_t w)
{
    return sm(e, o, 0x406, w, ((hi & 0xffff) << 16) | (lo & 0xffff));
}
uint32_t en2_set_68_sm_f4(En2Env* e, uint32_t o, uint32_t w, uint32_t l)
{
    en2_w32(e, o + 0x68, w);
    return sm(e, o, 0xf4, w, l);
}
uint32_t en2_button_state(En2Env* e, uint32_t o)
{
    return (en2_sm_f2(e, o) & 0xfffffffcu) | en2_r32(e, o + 0xc4);
}
uint32_t en2_lstrcpy_c4(En2Env* e, uint32_t o, uint32_t s) { return en2_call2(e, EN2_lstrcpyA, o + 0xc4, s); }
uint32_t en2_lstrcpy_44(En2Env* e, uint32_t o, uint32_t s) { return en2_call2(e, EN2_lstrcpyA, o + 0x44, s); }
uint32_t en2_close_handle(En2Env* e, uint32_t h) { return en2_call1(e, EN2_CloseHandle, h); }
uint32_t en2_show_window(En2Env* e, uint32_t o, uint32_t cmd) { return en2_call2(e, EN2_ShowWindow, en2_r32(e, o), cmd); }
uint32_t en2_invalidate(En2Env* e, uint32_t o, uint32_t rect, uint32_t erase)
{
    return en2_call3(e, EN2_InvalidateRect, en2_r32(e, o), rect, erase);
}
uint32_t en2_reg_flush(En2Env* e, uint32_t o) { return en2_call1(e, EN2_RegCloseKey, en2_r32(e, o)); }
uint32_t en2_reg_read_data(En2Env* e, uint32_t o, uint32_t name, uint32_t data, uint32_t size)
{
    return en2_call6(e, EN2_RegQueryValueExA, en2_r32(e, o), name, 0, 0, data, size);
}
uint32_t en2_reg_write_data(En2Env* e, uint32_t o, uint32_t name, uint32_t data, uint32_t size)
{
    return en2_call6(e, EN2_RegSetValueExA, en2_r32(e, o), name, 0, 3, data, size);
}
uint32_t en2_window_enabled(En2Env* e, uint32_t o)
{
    return (en2_call2(e, EN2_GetWindowLongA, en2_r32(e, o), (uint32_t)-16) & 0x08000000u) == 0;
}
uint32_t en2_has_focus(En2Env* e, uint32_t o) { return en2_call0(e, EN2_GetFocus) == en2_r32(e, o); }
uint32_t en2_set_c4_redraw(En2Env* e, uint32_t o, uint32_t v)
{
    if (v != en2_r32(e, o + 0xc4)) {
        en2_w32(e, o + 0xc4, v);
        en2_invalidate(e, o, 0, 0);
    }
    return 0;
}
uint32_t en2_dialog_message(En2Env* e, uint32_t o, uint32_t msg)
{
    if (en2_r32(e, o + 0x54) != 0) return 0;
    return en2_call2(e, 0x00c904fcu /* IsDialogMessageA */, en2_r32(e, o), msg);
}
