/*
 * en7_c.h: strcmp, window size metrics, bitmap resources, button text, the CD drive search and the
 * timer setup of TM2.EXE 0x00490000..0x004b0000 (fourth wave, en7 batch c). Environment: En2Env;
 * scratch holds up to 0x240 bytes. Build with -ffp-contract=off (0x00492f30 uses the x87 at 0x127f).
 * Verified under emulation: tools/emu/check_en7_c.sh.
 */
#ifndef TM2_EN7_C_H
#define TM2_EN7_C_H
#include "en2_env.h"

uint32_t en7_strcmp(En2Env* e, uint32_t a, uint32_t b);                    /* 0x004a91b0 */
uint32_t en7_window_frame_sizes(En2Env* e, uint32_t o, uint32_t out);      /* 0x004ac7f4 */
uint32_t en7_bitmap_load(En2Env* e, uint32_t o, uint32_t hinst, uint32_t name); /* 0x004aa098 */
uint32_t en7_button_text_draw(En2Env* e, uint32_t o, uint32_t hdc, uint32_t rect, uint32_t text,
                              uint32_t pressed, uint32_t shadow);          /* 0x004a5110 */
uint32_t en7_find_cd_drive(En2Env* e, uint32_t name);                      /* 0x00499884 */
void en7_timer_init(En2Env* e);                                            /* 0x00492f30 */

#endif
