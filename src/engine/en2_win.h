/*
 * en2_win.h: widget creation, registry and input mode helpers of TM2.EXE 0x00488000..0x004b0000,
 * ported from the machine code. Verified under emulation: tools/emu/check_en2_win.sh.
 *
 *   0x004ac4b0 en2_reg_set_path      name ? lstrcpyA(0x00bdfb38, name) : once ([0x004e8528] == 1,
 *                                    then 0) lstrcpyA(0x00bdfb38, "Software\SingleTrac\default"
 *                                    0x004e35b4); returns o
 *   0x004ac4e8 en2_reg_default_path  v ? (lstrcpyA(0x00bdfb38, v), 0) : (the same "once" default, 1)
 *   0x004ac638 en2_reg_write_string  RegSetValueExA([o], name, 0, REG_SZ, s, lstrlenA(s))
 *   0x004a4264 en2_widget_create_4a4264  o = 0x004a3fa0(a, b); four components at o+0x6c (0x14
 *                                    apart) cleared (0x004aa020); +0x34 = vtable 0x004e5e70;
 *                                    +0x54 +0x58 +0x5c +0x60 +0x64 = 0; returns o
 *   0x004a5394 en2_list_select_current  SendMessage 0x199 with LB_GETCURSEL's result
 *   0x004acd80 en2_node_list_free    frees the nodes from [o] (cursor o+8, next at +0x94, free
 *                                    0x004aa39c); o+4 = o+0 = 0; returns o
 *   0x004a61c8 / 0x004a5b34 / 0x004a6648 / 0x004a5fa8  control creation through 0x004a4068 (EAX o,
 *     EDX class name, EBX = caller EDX, ECX = caller EBX, stack: window name, style, x y w h):
 *     "SysListView32" (style ECX), "ComboBox" (ECX | WS_CHILD | WS_VSCROLL), "msctls_trackbar32"
 *     (ECX | WS_CHILD); 0x004a5fa8 "Edit" and 0x004a60bc "Static": window name ECX, style the first
 *     stack word | WS_CHILD, 5 stack words. The class names are strings in the EXE (0x004e3107 ..).
 *   0x004ac8a8 en2_key_release       mirror of 0x004ac900: mode 1 and render mode 2: 0x00a5d710[w] = 0;
 *                                    render mode 2 and k == 0x10: [0x00a5da20] = 0, 0x0049ed48(byte); 1
 *   0x0049e0c0 en2_input_mode_set    0: [0x00a5d370] = 0, display mode 0, input release, 0x00496190(1);
 *                                    1 / 2: [0x00a5d370] = v, display mode 1, 0x00496190(v == 2);
 *                                    other values: nothing, returns v
 */
#ifndef TM2_EN2_WIN_H
#define TM2_EN2_WIN_H

#include "en2_env.h"

uint32_t en2_reg_set_path(En2Env* e, uint32_t o, uint32_t name);
uint32_t en2_reg_default_path(En2Env* e, uint32_t v);
uint32_t en2_reg_write_string(En2Env* e, uint32_t o, uint32_t name, uint32_t str);
uint32_t en2_widget_create_4a4264(En2Env* e, uint32_t a, uint32_t b);
uint32_t en2_list_select_current(En2Env* e, uint32_t o);
uint32_t en2_node_list_free(En2Env* e, uint32_t o);
uint32_t en2_create_listview(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
uint32_t en2_create_combobox(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
uint32_t en2_create_trackbar(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
uint32_t en2_create_edit(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t style, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
uint32_t en2_key_release(En2Env* e, uint32_t k);
uint32_t en2_input_mode_set(En2Env* e, uint32_t v);
uint32_t en2_create_static(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t style, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

#endif
