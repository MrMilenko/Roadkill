/*
 * en7_h.h: network session start, hosting a DirectPlay session and the dialog procedure dispatch
 * of TM2.EXE (fourth wave, en7 batch h). Environment: En2Env (en2_env.h).
 * Verified under emulation: tools/emu/check_en7_h.sh.
 */
#ifndef TM2_EN7_H_H
#define TM2_EN7_H_H
#include "en2_env.h"

uint32_t en7_net_session_start(En2Env* e, uint32_t o, uint32_t dp, uint32_t lb, uint32_t x);   /* 0x004abb68 */
uint32_t en7_dplay_host_session(En2Env* e, uint32_t o);                                        /* 0x004aa690 */
uint32_t en7_dialog_proc(En2Env* e, uint32_t o, uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp); /* 0x004a9e88 */

#endif
