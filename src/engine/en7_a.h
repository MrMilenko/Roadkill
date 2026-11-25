/*
 * en7_a.h: engine / Win32 functions of TM2.EXE 0x00490000..0x004b0000 (fourth wave, en7 batch a).
 * Environment: En2Env (en2_env.h); scratch holds up to 0xd4 bytes of locals.
 * Verified under emulation: tools/emu/check_en7_a.sh.
 */
#ifndef TM2_EN7_A_H
#define TM2_EN7_A_H
#include "en2_env.h"

uint32_t en7_debug_printf(En2Env* e, uint32_t fmt, uint32_t va);      /* 0x004a1f1e (cdecl varargs) */
uint32_t en7_exit_handlers_run(En2Env* e);                             /* 0x004a2dcc */
uint32_t en7_joystick_poll(En2Env* e);                                 /* 0x004a20c4 */
uint32_t en7_widget_draw_item(En2Env* e, uint32_t o, uint32_t dis, uint32_t ebx); /* 0x004a4e00 */
uint32_t en7_draw_focus_frame(En2Env* e, uint32_t o, uint32_t hdc, uint32_t rect, uint32_t on); /* 0x004a5060 */
uint32_t en7_dsound_create(En2Env* e, uint32_t o, uint32_t desc);      /* 0x004aa524 */
uint32_t en7_dplay_enum_sessions(En2Env* e, uint32_t o);               /* 0x004ab43c */
uint32_t en7_render_set_background(En2Env* e, uint32_t o, uint32_t path, uint32_t ebx); /* 0x004a2ea0 */

#endif
