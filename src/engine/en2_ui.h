/*
 * en2_ui.h: widget object lifetime, window procedure defaults, key queue and screen helpers of
 * TM2.EXE 0x00488000..0x004b0000, ported from the machine code. Verified under emulation:
 * tools/emu/check_en2_ui.sh. Runs in an En2Env (en2_env.h).
 *
 *   0x004a6920 en2_widget_delete     Watcom C++ deleting destructor: flags & 4 (array): free
 *                                    (0x004aa39c, through the thunk 0x004aa018) the block header
 *                                    0x004a9ff8(o, 0x004e5920), returns o; else r = destructor
 *                                    0x004a3ff4(o, 1), flags & 2: free(r); returns r
 *   0x004a7924 en2_wndproc_size      o+4 (old window procedure) ? CallWindowProcA(o+4, [o], WM_SIZE,
 *                                    ECX, BX << 16 | DX) : 0
 *   0x004a7958 en2_wndproc_command   the same with WM_COMMAND (BX << 16 | DX, ECX)
 *   0x004aa064 en2_gdi_free          DeleteObject([o]) and [o] = 0 when set; FreeResource(o+0x10) when set; o
 *   0x00495c94 en2_screen_lost_life3 three times 0x00495b48(v) then 0x00495b14() (display object);
 *                                    returns the last result (the loop counter left in ECX is not passed on)
 *   0x004a2fa4 en2_handle_close_notify  r = 0x00498c70(); r: CloseHandle([0x004e84d0]) (0x00498cb0),
 *                                    vtable o+0x30 slot 0 (o, 0x00ad3204); returns r
 *   0x004a3fa0 en2_widget_base_create  o = 0x004a6ac8(a, b); vtable +0x34 = 0x004e5f54; +0x3c =
 *                                    GetSysColor(COLOR_BTNTEXT), +0x40 = GetSysColor(COLOR_BTNFACE),
 *                                    +0x44 = 0, vtable slot 0xb8 (o, +0x40) (= 0x004a4180, brush),
 *                                    +0x48 = 2 (OPAQUE), +0x4c = 0; returns o
 *   0x004a41bc en2_widget_paint_colors  SetTextColor / SetBkColor / SetBkMode(hdc, +0x3c / +0x40 /
 *                                    +0x48); returns the brush +0x44
 *   0x004a7ba4 en2_pages_each        for i < o+0x50c9c (read again each step): +0x2cc = i, [o] =
 *                                    o+0x2dc[i], 0x004a7c94(o, EDX = i + 1); returns o
 *   0x004ac950 en2_f10_key           EBX == 1: GetAsyncKeyState(VK_F10) & 0x8001 -> [0x00bdfccc] = 1,
 *                                    byte 0x00a5d754 = 1; EBX != 1 and [0x00bdfccc]: both = 0;
 *                                    otherwise byte 0x00a5d748 = 1. Returns 2.
 *   0x0049ed48 en2_key_queue_push    ring of 0x100 bytes at 0x00a5d910: [tail] = k, count 0x00a5da10++,
 *                                    tail 0x00a5da1c = tail < 0xff ? tail + 1 : 0, when it meets head
 *                                    0x00a5da14 the head moves on (wrapping above 0xff). Tested with
 *                                    head / tail in -2..0x101 (the store index is the raw tail).
 */
#ifndef TM2_EN2_UI_H
#define TM2_EN2_UI_H

#include "en2_env.h"

uint32_t en2_widget_delete(En2Env* e, uint32_t o, uint32_t flags);
uint32_t en2_wndproc_size(En2Env* e, uint32_t o, uint32_t dx, uint32_t bx, uint32_t cx);
uint32_t en2_wndproc_command(En2Env* e, uint32_t o, uint32_t dx, uint32_t bx, uint32_t cx);
uint32_t en2_gdi_free(En2Env* e, uint32_t o);
uint32_t en2_screen_lost_life3(En2Env* e, uint32_t v);
uint32_t en2_handle_close_notify(En2Env* e, uint32_t o);
uint32_t en2_widget_base_create(En2Env* e, uint32_t a, uint32_t b);
uint32_t en2_widget_paint_colors(En2Env* e, uint32_t o, uint32_t hdc);
uint32_t en2_pages_each(En2Env* e, uint32_t o);
uint32_t en2_f10_key(En2Env* e, uint32_t ebx);
void en2_key_queue_push(En2Env* e, uint32_t k);

#endif
