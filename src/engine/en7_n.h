/*
 * en7_n.h: the text button's WM_DRAWITEM 0x004a45f4 of TM2.EXE (fourth wave, en7 batch n).
 * Environment: En2Env (en2_env.h). Verified under emulation: tools/emu/check_en7_n.sh.
 */
#ifndef TM2_EN7_N_H
#define TM2_EN7_N_H
#include "en2_env.h"

uint32_t en7_text_button_draw_item(En2Env* e, uint32_t o, uint32_t dis);   /* 0x004a45f4 */

#endif
