/*
 * en7_k.h: the light defaults, the input configuration save and the input setup of TM2.EXE
 * (fourth wave, en7 batch k). Environment: En2Env (en2_env.h); scratch holds 0x19c bytes.
 * Verified under emulation: tools/emu/check_en7_k.sh.
 */
#ifndef TM2_EN7_K_H
#define TM2_EN7_K_H
#include "en2_env.h"

void en7_lights_init(En2Env* e);           /* 0x00495328 */
uint32_t en7_input_config_save(En2Env* e); /* 0x004a9c38 */
uint32_t en7_input_setup(En2Env* e);       /* 0x00493444 */

#endif
