/*
 * en7_j.h: the background blit 0x004a2ffc (render_configure) of TM2.EXE (fourth wave, en7 batch j).
 * Environment: En2Env (en2_env.h). Verified under emulation: tools/emu/check_en7_j.sh.
 */
#ifndef TM2_EN7_J_H
#define TM2_EN7_J_H
#include "en2_env.h"

uint32_t en7_background_blit(En2Env* e, uint32_t o, uint32_t x, uint32_t y, uint32_t surf);   /* 0x004a2ffc */

#endif
