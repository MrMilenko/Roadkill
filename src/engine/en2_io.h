/*
 * en2_io.h: the file loading thread, registry key creation, coordinate system allocation, input
 * resume callbacks and widget creation of TM2.EXE 0x00488000..0x004b0000, ported from the machine
 * code. Verified under emulation: tools/emu/check_en2_io.sh. Runs in an En2Env (en2_env.h); locals
 * the original passes by address live in En2Env.scratch.
 *
 *   0x00498b38 en2_file_open          CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
 *                                     0x10000080, 0); *size_out = invalid ? 0 : GetFileSize(h, 0); h
 *   0x00498bd8 en2_load_thread_start  only when no thread (0x00a39c64 == 0) and idle ([0x004e8428] == 1):
 *                                     request 0x00a39c58 = {h, buffer, size}, idle = 0, thread =
 *                                     CreateThread(0, 0, 0x00498c38, request, 0, &id); a failed create
 *                                     sets idle back. Returns the thread, or h when nothing was started
 *   0x00498c38 en2_load_thread_proc   (the thread, stdcall) ReadFile(req h, buffer, size, &n, 0); idle = 1; 0
 *   0x004a2f60 en2_load_file_async    vtable o+0x30 slot 3 (o, vtable, EBX, buffer or 0x00ad3204);
 *                                     h = file_open(name) -> [0x004e84d0]; invalid: 1; else the thread
 *                                     reads the whole file into the buffer (0x00ad3204 when EBX = 0); 0
 *   0x004ac550 en2_reg_create         RegCreateKeyExA(EDX root, EBX sub or the path 0x00bdfb38, 0, 0, 0,
 *                                     KEY_ALL_ACCESS, 0, EAX out, &disposition)
 *   0x00492d90 en2_cs_alloc csAlloc   n = [0x00a03644]; n >= 0x80: printf 0x004a1f1e("internal error:
 *                                     too many coordinate systems", n), 0; else p = 0x009f7e44 + 0x170 n,
 *                                     csReset 0x00492c60(p), n + 1; returns p
 *   0x004aca90 en2_ui_resume_6c       if o+0x6c: o+0x3c = input mode 0x0049e108, input mode 0 (0x0049e0c0),
 *                                     DrawMenuBar([o]), o+0x40 = 1, (*o+0x6c)(o, EDX, o+0x58); 0
 *   0x004aca28 en2_ui_resume_64       the same unconditionally, the callback o+0x64 (o, EDX, o+0x50) if set
 *   0x004aca1c en2_display_aca1c      0x004b5b40(display, EDX x, EBX y): a tail jump, the caller's
 *                                     EDX / EBX (the window procedure's WM_SIZE / move words) pass through
 *   0x004a53c4 en2_list_add_item      r = LB_ADDSTRING(text); r < 0 (LB_ERR / LB_ERRSPACE): r; else
 *                                     LB_SETITEMDATA(r, EBX)
 *   0x004a4ab0 en2_button_set_style   +0xc0 = style, +0xc8 = 2 + (style & 8 != 0), BM_SETSTYLE through
 *                                     0x004a4440 (style & 0xffff0000 | 0xb, redraw); redraw == 1:
 *                                     InvalidateRect(hwnd, 0, 0); 0
 *   0x004a7118 en2_window_subclass    o+4 = GetWindowLongA(hwnd, GWL_WNDPROC); SetWindowLongA(hwnd,
 *                                     GWL_WNDPROC, o+8); SetPropA(hwnd, "ST Object", o)
 *   0x004a4354 en2_create_button      o+0x68 = style | WS_CHILD; 0x004a4068(o, "Button", EDX, EBX, stack:
 *                                     ECX, o+0x68, x, y, w, h)
 *   0x004a6078 en2_widget_image_create  o = 0x004a3fa0(a, b); vtable 0x004e50fc; its slot 0xc4
 *                                     (= 0x004a60bc, the "Static" create) (o, b, EBX, ECX, style |
 *                                     WS_CHILD, x, y, w, h); returns o
 */
#ifndef TM2_EN2_IO_H
#define TM2_EN2_IO_H

#include "en2_env.h"

uint32_t en2_file_open(En2Env* e, uint32_t name, uint32_t size_out);
uint32_t en2_load_thread_start(En2Env* e, uint32_t h, uint32_t buf, uint32_t size);
uint32_t en2_load_thread_proc(En2Env* e, uint32_t req);
uint32_t en2_load_file_async(En2Env* e, uint32_t o, uint32_t name, uint32_t buf);
uint32_t en2_reg_create(En2Env* e, uint32_t out, uint32_t root, uint32_t sub);
uint32_t en2_cs_alloc(En2Env* e);
uint32_t en2_ui_resume_6c(En2Env* e, uint32_t o, uint32_t edx);
uint32_t en2_ui_resume_64(En2Env* e, uint32_t o, uint32_t edx);
uint32_t en2_display_aca1c(En2Env* e, uint32_t edx, uint32_t ebx);
uint32_t en2_list_add_item(En2Env* e, uint32_t o, uint32_t text, uint32_t data);
uint32_t en2_button_set_style(En2Env* e, uint32_t o, uint32_t style, uint32_t redraw);
uint32_t en2_window_subclass(En2Env* e, uint32_t o);
uint32_t en2_create_button(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t style, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
uint32_t en2_widget_image_create(En2Env* e, uint32_t a, uint32_t b, uint32_t ebx, uint32_t ecx, uint32_t style, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

#endif
