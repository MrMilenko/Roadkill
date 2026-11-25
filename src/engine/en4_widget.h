/*
 * en4_widget.h: widget, window class, list view and subsystem glue of TM2.EXE
 * 0x00490000..0x004b0000 (fourth wave). Environment: En2Env (en2_env.h).
 * Verified under emulation: tools/emu/check_en4_widget.sh.
 */
#ifndef TM2_EN4_WIDGET_H
#define TM2_EN4_WIDGET_H
#include "en2_env.h"

uint32_t en4_cs_root_init(En2Env* e);                                   /* 0x00492df4 */
uint32_t en4_view_load_camera(En2Env* e, uint32_t a);                   /* 0x00492ed4 */
uint32_t en4_input_poll(En2Env* e);                                     /* 0x00493748 */
uint32_t en4_button_key(En2Env* e, uint32_t o, uint32_t key, uint32_t ebx, uint32_t ecx); /* 0x004a4504 */
uint32_t en4_widget_style_finalize(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx,
                                   uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4); /* 0x004a4a24 */
uint32_t en4_widget_cycle_input(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx); /* 0x004a4b04 */
uint32_t en4_widget_create_sprite(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                                  uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4); /* 0x004a5f40 */
uint32_t en4_listview_insert(En2Env* e, uint32_t o, uint32_t text, uint32_t item, uint32_t param); /* 0x004a64c4 */
uint32_t en4_window_obj_init(En2Env* e, uint32_t o);                    /* 0x004a6ac8 */
uint32_t en4_register_class(En2Env* e, uint32_t name, uint32_t hinst, uint32_t style, uint32_t brush); /* 0x004a6b80 */
uint32_t en4_window_enable(En2Env* e, uint32_t o, uint32_t on);         /* 0x004a6dd8 */
uint32_t en4_mem_alloc(En2Env* e, uint32_t n);                          /* 0x004a8f48 */
uint32_t en4_dplay_report(En2Env* e, uint32_t o, uint32_t flag);        /* 0x004aaa88 (flag 0), 0x004aaacc (1) */
uint32_t en4_strcpy_dc(En2Env* e, uint32_t o, uint32_t s);              /* 0x004ac044 */
uint32_t en4_slots_each_active(En2Env* e, uint32_t o, uint32_t x);      /* 0x004ace8c */

#endif
