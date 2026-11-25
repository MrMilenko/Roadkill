/*
 * en7_e.h: the key sequence trie builder, the immediate keyboard read and the widget arrow / tab
 * navigation of TM2.EXE (fourth wave, en7 batch e). Environment: En2Env (en2_env.h).
 * Verified under emulation: tools/emu/check_en7_e.sh.
 */
#ifndef TM2_EN7_E_H
#define TM2_EN7_E_H
#include "en2_env.h"

uint32_t en7_key_sequences_build(En2Env* e, uint32_t s, uint32_t len, uint32_t cb);  /* 0x0049de80 */
void en7_keyboard_read_state(En2Env* e);                                            /* 0x0049e4ac */
uint32_t en7_widget_navigate(En2Env* e, uint32_t o, uint32_t key);                  /* 0x004a798c */

#endif
