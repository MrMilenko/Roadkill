/*
 * en2_leaf.h: small leaves of TM2.EXE 0x00488000..0x004b0000, ported from the machine code.
 * Verified under emulation: tools/emu/check_en2_leaf.sh (the address of each port is in the
 * diff's test names). Runs in an En2Env (en2_env.h). Return values: only the part the original
 * defines is meaningful (AL for the flag / byte getters, AX for frame_get_ticks60).
 *
 *   0x0048bc38 0x004937b8 0x004a1ee0 0x004aabc4  en2_nop (ret)
 *   0x0048e254 [0x004e8288] = v            0x00492c44 csGetRoot [0x009f7e38]
 *   0x00492de4 csSetModel cs+0x15c = model 0x00492c4c csListAdd: cs+0x160 = head 0x009f7e34, head = cs
 *   0x004931d4 frame_get_ticks60 (u16 0x004e8300)   0x004931dc frame_get_fps [0x004e8304]
 *   0x004931f4 frame_parity [0x00a0364c]  0x00493344 [0x00a03a70]  0x00493644 byte 0x00a03a8c
 *   0x00496284 byte 0x00a2dba5  0x00499958 [0x00a3a5d0]  0x0049d4ac [0x004e8430] = v
 *   0x0049e108 [0x00a5d370]  0x0049eda4 [0x00a5da20] = v  0x0049ee90 / 0x0049ee98 dpc_set / get_base
 *   0x004ad82c screen pointer [0x00a04674] = v
 *   0x00493380 / 0x0049338c / 0x00493398  [0x00a03a7c] / [0x00a03a80] / [0x00a03a84] != 0
 *   0x00498b2c subsys_resource_init [0x00a3a5c8] = EAX, [0x00a3a5cc] = EDX
 *   0x0049eb30 / 0x0049eb3c  key table 0x00a5d810[k & 0xff] get / set to 1
 *   0x0049ee74 counters 0x00a5da10 / 14 / 1c = 0;  0x0049ea8c (and thunk 0x004a2058) also clears
 *              the 0x100 byte key table
 *   0x004937e0 [0x00a03b78 + 4 * 37 * [0x00a03ae4 + 4 i]] (i 0..7, table values 0..3 tested)
 *   0x00499960 subsys_unknown_c: dword 0x00a3a610 + 0x228 k = 0 (k = 1..15), [0x004e842c] = -1
 *   0x00496fb4 subsys_resource_mgr_init: 0x6000 bytes at 0x00a30c48 = 0, [0x00a36c48] = [0x00a36c54] = 0
 *   0x004a9ff0 returns 1; 0x004988c8 / 0x004988e0 o + 0xc4 / o + 0x44
 *   0x004aa68c +0x24, 0x004aaa70 +0x1c, 0x004ac714 game_tick +0x38, 0x004ace88 +0x10 getters;
 *   0x004aaa60 +0xc = 1; 0x004ab72c / 0x004ab734 +0x254 / +0x258 = EDX
 *   0x004aa020 widget_alloc_component: +0x10 +0xc +8 +0 = 0
 *   0x004979fc +4 +8 +0xc +0x10 = 0, +0x14 = 1, +0 = 0
 * Forwards (display object 0x00c84840 methods above 0x004b0000, not ported yet):
 *   0x004958d8 screen_get_width -> 0x004b5a14, 0x004958e4 screen_get_height -> 0x004b5a10,
 *   0x00495b14 -> 0x004b5128, 0x00495b20 -> 0x004b52b8(d, v), 0x00496184 win32_input_release ->
 *   0x004b480c, 0x00496190 -> 0x004b4a54(d, v), 0x00495af8 subsys_display_mode -> 0x004b50f8(d, v),
 *   0x00495b60 -> 0x004b4e38(d, 0), 0x00495b48 screenLostALife -> 0x004b4d98(d, v & 0xffff, 0),
 *   0x00496304 subsys_display_width -> 0x004b45a0(d, a, b, c, stack 0), 0x004a2fdc -> 0x004b48b8(d,
 *   o + 0x24) returns 0, 0x00495ad8 subsys_display_setup -> 0x004bf76f([0x00a05678]),
 *   0x004958ac screen_is_split (the hardware 3D device flag) = 0x004b5f60() != 0
 *   0x004899b8 crt_rand_state_ptr = (*[0x004e89c4])() + 0xc
 *   0x0048e9a8 polygon_renderer_wrapper -> 0x0048e9b8 (registers and the stack word passed on)
 *   0x004a4bdc -> 0x004a4b50(EAX, DX, BX, CX, stack word zero extended)
 *   0x004a9dd0: o = 0x004a3fa0(a, b); o+0x34 = 0x004e60e8 (vtable), o+0x54 = b; returns o
 *   0x004a41f0: if o+0x38 == 1, 0x004a6f80(o); returns 2
 *   0x004a2e8c: o+0x30 = vtable 0x004e58fc, calls its slot 3 with o; returns o
 *   0x004acb1c: EDX == 1: 0x004ba39c(all registers), returns 0; else 2
 *   0x00493694 timer_check_timeout = AL of 0x004a2380() | 0x004a28a8() | 0x004a2068()
 *   0x004a2068: r = 0x004932a0(1); AL = (AL of r == 1) ? 0x0049ecd0(r) : 0
 *   0x00493728: 0 <= k < 8: key_set(pair_get_a(i)) (en_wg_pair_get_a 0x004a1f8c)
 *   0x004a1ef9 game_shutdown_final: r = (*[0x004e8484])(code); r = (*[0x004e8488])(r);
 *              if [0x004e8fcc]: (*[0x004e8fcc])(r); exit 0x004bc957(code)
 *   0x004a1ee1 game_shutdown_dispatch: r = (*[0x004e8480])(code); (*[0x004e8484])(r); final(code)
 * Win32 wrappers, hwnd = [o]: SendMessageA with the message in the function name (W = EDX as
 * wParam, L = EDX as lParam, WL = EDX, EBX); 0x004a6ef4 WM_GETTEXT(EBX, EDX); 0x004a6844 0x406
 * (ECX, BX << 16 | DX); 0x004a4440 o+0x68 = EDX then 0xf4 (EDX, EBX); 0x004a4a80 = (BM_GETSTATE &
 * ~3) | o+0xc4; 0x004988b4 / 0x004988d0 lstrcpyA(o + 0xc4 / 0x44, s); 0x00498cb0 CloseHandle;
 * 0x004a6ee4 widget_set_property = ShowWindow; 0x004a6e2c InvalidateRect(hwnd, EDX, EBX);
 * 0x004ac590 reg_flush = RegCloseKey; 0x004ac5e4 reg_read_data = RegQueryValueExA(key, name, 0, 0,
 * data, size); 0x004ac65c reg_read_data2 (it writes: RegSetValueExA(key, name, 0, REG_BINARY,
 * data, size)); 0x004a6e0c = !(GetWindowLongA(hwnd, GWL_STYLE) & WS_DISABLED); 0x004a6f9c =
 * GetFocus() == hwnd; 0x004a4a94: if v != o+0xc4 { o+0xc4 = v; InvalidateRect(hwnd, 0, 0) },
 * returns 0; 0x004a9e70: o+0x54 ? 0 : IsDialogMessageA(hwnd, msg).
 * SendMessage messages: 0xf0..0xf5 button BM_*, 0x143..0x14e combo box CB_*, 0x180..0x1a2 list
 * box LB_*, 0x400.. WM_USER (trackbar / up down), 0x1001..0x1026 list view, 0xc5 EM_LIMITTEXT.
 */
#ifndef TM2_EN2_LEAF_H
#define TM2_EN2_LEAF_H

#include "en2_env.h"

void en2_nop(En2Env* e);
void en2_set_4e8288(En2Env* e, uint32_t v);
uint32_t en2_cs_get_root(En2Env* e);
void en2_cs_set_model(En2Env* e, uint32_t cs, uint32_t model);
uint32_t en2_frame_get_ticks60(En2Env* e);
uint32_t en2_frame_get_fps(En2Env* e);
uint32_t en2_frame_parity(En2Env* e);
uint32_t en2_get_a03a70(En2Env* e);
uint32_t en2_get_a03a8c_byte(En2Env* e);
uint32_t en2_get_a2dba5_byte(En2Env* e);
uint32_t en2_get_a3a5d0(En2Env* e);
void en2_set_4e8430(En2Env* e, uint32_t v);
uint32_t en2_get_a5d370(En2Env* e);
void en2_set_a5da20(En2Env* e, uint32_t v);
void en2_dpc_set_base(En2Env* e, uint32_t v);
uint32_t en2_dpc_get_base(En2Env* e);
void en2_set_screen_ptr(En2Env* e, uint32_t v);
uint32_t en2_flag_a03a7c(En2Env* e);
uint32_t en2_flag_a03a80(En2Env* e);
uint32_t en2_flag_a03a84(En2Env* e);
void en2_resource_init(En2Env* e, uint32_t a, uint32_t b);
uint32_t en2_key_get(En2Env* e, uint32_t k);
void en2_key_set(En2Env* e, uint32_t k);
void en2_key_counters_clear(En2Env* e);
uint32_t en2_lookup_a03b78(En2Env* e, uint32_t i);
void en2_slots_clear(En2Env* e);
void en2_resource_mgr_init(En2Env* e);
void en2_keys_clear(En2Env* e);
uint32_t en2_ret1(En2Env* e);
uint32_t en2_addr_c4(En2Env* e, uint32_t o);
uint32_t en2_addr_44(En2Env* e, uint32_t o);
uint32_t en2_get_24(En2Env* e, uint32_t o);
void en2_set_c_1(En2Env* e, uint32_t o);
uint32_t en2_get_1c(En2Env* e, uint32_t o);
void en2_set_254(En2Env* e, uint32_t o, uint32_t v);
void en2_set_258(En2Env* e, uint32_t o, uint32_t v);
uint32_t en2_game_tick(En2Env* e, uint32_t o);
uint32_t en2_get_10(En2Env* e, uint32_t o);
void en2_component_clear(En2Env* e, uint32_t o);
void en2_rect6_reset(En2Env* e, uint32_t o);
void en2_cs_list_add(En2Env* e, uint32_t cs);
uint32_t en2_screen_get_width(En2Env* e);
uint32_t en2_screen_get_height(En2Env* e);
uint32_t en2_display_setup(En2Env* e);
uint32_t en2_display_495b14(En2Env* e);
uint32_t en2_display_495b20(En2Env* e, uint32_t v);
uint32_t en2_input_release(En2Env* e);
uint32_t en2_display_496190(En2Env* e, uint32_t v);
uint32_t en2_display_mode(En2Env* e, uint32_t v);
uint32_t en2_display_495b60(En2Env* e);
uint32_t en2_display_lost_life(En2Env* e, uint32_t v);
uint32_t en2_display_width(En2Env* e, uint32_t a, uint32_t b, uint32_t c);
uint32_t en2_display_rect_24(En2Env* e, uint32_t o);
uint32_t en2_hw3d_flag(En2Env* e);
uint32_t en2_crt_rand_state_ptr(En2Env* e);
uint32_t en2_polygon_renderer_wrapper(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s);
uint32_t en2_widget_4a4bdc(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t s);
uint32_t en2_widget_4a9dd0(En2Env* e, uint32_t a, uint32_t b);
uint32_t en2_widget_4a41f0(En2Env* e, uint32_t o);
uint32_t en2_widget_4a2e8c(En2Env* e, uint32_t o);
uint32_t en2_4acb1c(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx);
uint32_t en2_timer_check_timeout(En2Env* e);
uint32_t en2_input_4a2068(En2Env* e);
void en2_key_from_pair(En2Env* e, uint32_t i, int32_t k);
uint32_t en2_shutdown_final(En2Env* e, uint32_t code);
uint32_t en2_shutdown_dispatch(En2Env* e, uint32_t code);
uint32_t en2_sm_gettext(En2Env* e, uint32_t o, uint32_t buf, uint32_t len);
uint32_t en2_sm_406(En2Env* e, uint32_t o, uint32_t lo, uint32_t hi, uint32_t w);
uint32_t en2_set_68_sm_f4(En2Env* e, uint32_t o, uint32_t w, uint32_t l);
uint32_t en2_button_state(En2Env* e, uint32_t o);
uint32_t en2_lstrcpy_c4(En2Env* e, uint32_t o, uint32_t s);
uint32_t en2_lstrcpy_44(En2Env* e, uint32_t o, uint32_t s);
uint32_t en2_close_handle(En2Env* e, uint32_t h);
uint32_t en2_show_window(En2Env* e, uint32_t o, uint32_t cmd);
uint32_t en2_invalidate(En2Env* e, uint32_t o, uint32_t rect, uint32_t erase);
uint32_t en2_reg_flush(En2Env* e, uint32_t o);
uint32_t en2_reg_read_data(En2Env* e, uint32_t o, uint32_t name, uint32_t data, uint32_t size);
uint32_t en2_reg_write_data(En2Env* e, uint32_t o, uint32_t name, uint32_t data, uint32_t size);
uint32_t en2_window_enabled(En2Env* e, uint32_t o);
uint32_t en2_has_focus(En2Env* e, uint32_t o);
uint32_t en2_set_c4_redraw(En2Env* e, uint32_t o, uint32_t v);
uint32_t en2_dialog_message(En2Env* e, uint32_t o, uint32_t msg);
uint32_t en2_sm_f1(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0xf1, ...) */
uint32_t en2_sm_f3(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0xf3, ...) */
uint32_t en2_sm_180(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0x180, ...) */
uint32_t en2_sm_182(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0x182, ...) */
uint32_t en2_sm_1a2(En2Env* e, uint32_t o, uint32_t w, uint32_t l);   /* SendMessageA([o], 0x1a2, ...) */
uint32_t en2_sm_199(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0x199, ...) */
uint32_t en2_sm_181(En2Env* e, uint32_t o, uint32_t w, uint32_t l);   /* SendMessageA([o], 0x181, ...) */
uint32_t en2_sm_186(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0x186, ...) */
uint32_t en2_sm_143(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0x143, ...) */
uint32_t en2_sm_14e(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0x14e, ...) */
uint32_t en2_sm_c5(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0xc5, ...) */
uint32_t en2_sm_1013(En2Env* e, uint32_t o, uint32_t w, uint32_t l);   /* SendMessageA([o], 0x1013, ...) */
uint32_t en2_sm_1024(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0x1024, ...) */
uint32_t en2_sm_1026(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0x1026, ...) */
uint32_t en2_sm_1001(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0x1001, ...) */
uint32_t en2_sm_415(En2Env* e, uint32_t o, uint32_t v);   /* SendMessageA([o], 0x415, ...) */
uint32_t en2_sm_405(En2Env* e, uint32_t o, uint32_t w, uint32_t l);   /* SendMessageA([o], 0x405, ...) */
uint32_t en2_sm_414(En2Env* e, uint32_t o, uint32_t w, uint32_t l);   /* SendMessageA([o], 0x414, ...) */
uint32_t en2_sm_f5(En2Env* e, uint32_t o);   /* SendMessageA([o], 0xf5, ...) */
uint32_t en2_sm_f0(En2Env* e, uint32_t o);   /* SendMessageA([o], 0xf0, ...) */
uint32_t en2_sm_f2(En2Env* e, uint32_t o);   /* SendMessageA([o], 0xf2, ...) */
uint32_t en2_sm_18b(En2Env* e, uint32_t o);   /* SendMessageA([o], 0x18b, ...) */
uint32_t en2_sm_184(En2Env* e, uint32_t o);   /* SendMessageA([o], 0x184, ...) */
uint32_t en2_sm_147(En2Env* e, uint32_t o);   /* SendMessageA([o], 0x147, ...) */
uint32_t en2_sm_1004(En2Env* e, uint32_t o);   /* SendMessageA([o], 0x1004, ...) */
uint32_t en2_sm_400(En2Env* e, uint32_t o);   /* SendMessageA([o], 0x400, ...) */

#endif
