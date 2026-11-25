/*
 * en5_kbdev.h: the keyboard device record update 0x004a3964 of TM2.EXE (fourth wave, en5).
 * Environment: En2Env (en2_env.h); scratch holds the 0xa8 byte DIDEVICEOBJECTDATA buffer.
 * Verified under emulation: tools/emu/check_en5_kbdev.sh.
 */
#ifndef TM2_EN5_KBDEV_H
#define TM2_EN5_KBDEV_H
#include "en2_env.h"

void en5_kbdev_update(En2Env* e, uint32_t o);   /* 0x004a3964 */

#endif
