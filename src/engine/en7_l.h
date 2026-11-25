/*
 * en7_l.h: path building (file_build_path and its in place variant) of TM2.EXE (fourth wave, en7
 * batch l). Environment: En2Env (en2_env.h); scratch holds 0x264 bytes.
 * Verified under emulation: tools/emu/check_en7_l.sh.
 */
#ifndef TM2_EN7_L_H
#define TM2_EN7_L_H
#include "en2_env.h"

uint32_t en7_file_build_path(En2Env* e, uint32_t dest, uint32_t name, uint32_t kind);   /* 0x00499408 */
uint32_t en7_file_path_in_place(En2Env* e, uint32_t dest, uint32_t kind);               /* 0x00499564 */

#endif
