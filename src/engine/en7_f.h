/*
 * en7_f.h: network session teardown, joining a DirectPlay session and the widget bitmap choice of
 * TM2.EXE (fourth wave, en7 batch f). Environment: En2Env (en2_env.h).
 * Verified under emulation: tools/emu/check_en7_f.sh.
 */
#ifndef TM2_EN7_F_H
#define TM2_EN7_F_H
#include "en2_env.h"

uint32_t en7_net_session_destroy(En2Env* e, uint32_t o);             /* 0x004ab9d0 */
uint32_t en7_dplay_join_session(En2Env* e, uint32_t o, uint32_t sess); /* 0x004aa7dc */
uint32_t en7_widget_draw_bitmap(En2Env* e, uint32_t o, uint32_t hdc, uint32_t rect, uint32_t part,
                                uint32_t pressed, uint32_t focus, uint32_t third);   /* 0x004a4eb4 */

#endif
