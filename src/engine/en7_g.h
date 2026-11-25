/*
 * en7_g.h: the network thread and its session enumeration callback of TM2.EXE (fourth wave, en7
 * batch g). Environment: En2Env (en2_env.h). Verified under emulation: tools/emu/check_en7_g.sh.
 */
#ifndef TM2_EN7_G_H
#define TM2_EN7_G_H
#include "en2_env.h"

uint32_t en7_net_thread(En2Env* e, uint32_t o, uint32_t arg1);                       /* 0x004ac134 (stdcall) */
uint32_t en7_enum_sessions_cb(En2Env* e, uint32_t sd, uint32_t timeout, uint32_t flags, uint32_t o); /* 0x004ac2f0 */

#endif
