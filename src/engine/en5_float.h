/*
 * en5_float.h: x87 helpers of TM2.EXE (fourth wave, en5): float_round 0x004979a2, the texture
 * sort comparator 0x004977cc, the microsecond clocks 0x00493038 / 0x004931fc. Environment: En2Env.
 * Build with -ffp-contract=off; the game runs the x87 at control word 0x127f (53 bit, nearest),
 * where these operations equal IEEE double ones. Verified: tools/emu/check_en5_float.sh.
 */
#ifndef TM2_EN5_FLOAT_H
#define TM2_EN5_FLOAT_H
#include "en2_env.h"

double en5_float_round(double x);                       /* 0x004979a2 (ST0 in, ST0 out) */
uint32_t en5_texture_compare(En2Env* e, uint32_t a, uint32_t b);   /* 0x004977cc */
uint32_t en5_clock_us(En2Env* e);                       /* 0x00493038 */
uint32_t en5_clock_us_mark(En2Env* e);                  /* 0x004931fc */

#endif
