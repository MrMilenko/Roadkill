/*
 * en7_m.h: the PCX image decode onto a DirectDraw surface 0x004a3494 (screenLoadCardImages) of
 * TM2.EXE (fourth wave, en7 batch m). Environment: En2Env (en2_env.h).
 * Verified under emulation: tools/emu/check_en7_m.sh.
 */
#ifndef TM2_EN7_M_H
#define TM2_EN7_M_H
#include "en2_env.h"

uint32_t en7_pcx_to_surface(En2Env* e, uint32_t o);   /* 0x004a3494 */

#endif
