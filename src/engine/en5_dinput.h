/*
 * en5_dinput.h: DirectInput keyboard setup 0x0049e110 of TM2.EXE (fourth wave, en5).
 * Environment: En2Env (en2_env.h). Verified under emulation: tools/emu/check_en5_dinput.sh.
 */
#ifndef TM2_EN5_DINPUT_H
#define TM2_EN5_DINPUT_H
#include "en2_env.h"

uint32_t en5_dinput_keyboard_init(En2Env* e);   /* 0x0049e110 */

#endif
