/*
 * en5_keys.h: the DirectInput keyboard reader and the key sequence (cheat code) matcher of
 * TM2.EXE (fourth wave, en5). Environment: En2Env (en2_env.h); scratch must hold 0xa8 bytes (the
 * DIDEVICEOBJECTDATA buffer). Verified under emulation: tools/emu/check_en5_keys.sh.
 */
#ifndef TM2_EN5_KEYS_H
#define TM2_EN5_KEYS_H
#include "en2_env.h"

uint32_t en5_key_sequence_feed(En2Env* e, uint32_t k);   /* 0x0049dfac */
void en5_keyboard_read(En2Env* e);                       /* 0x0049e638 */

#endif
