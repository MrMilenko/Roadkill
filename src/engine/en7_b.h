/*
 * en7_b.h: joystick / DirectInput setup, view light setup, DIB blit, the text button constructor
 * and the message pump of TM2.EXE 0x00490000..0x004b0000 (fourth wave, en7 batch b).
 * Environment: En2Env (en2_env.h). Verified under emulation: tools/emu/check_en7_b.sh.
 */
#ifndef TM2_EN7_B_H
#define TM2_EN7_B_H
#include "en2_env.h"

uint32_t en7_joy_open_device(En2Env* e, uint32_t o);                 /* 0x004a7be0 */
uint32_t en7_view_lights_setup(En2Env* e);                           /* 0x00495758 */
uint32_t en7_joy_enum_device_cb(En2Env* e, uint32_t ddi, uint32_t o);  /* 0x004a8284 (stdcall) */
uint32_t en7_dib_stretch(En2Env* e, uint32_t o, uint32_t hdc, uint32_t dst, uint32_t src); /* 0x004aa2d8 */
uint32_t en7_widget_create_text_button(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                                       uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4); /* 0x004a490c */
uint32_t en7_game_loop_run(En2Env* e);                               /* 0x004a8e6c */

#endif
