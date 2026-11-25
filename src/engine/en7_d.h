/*
 * en7_d.h: joystick state read, network player list cleanup, sound object teardown, the screen
 * rectangle outline and the session list box sync of TM2.EXE (fourth wave, en7 batch d).
 * Environment: En2Env (en2_env.h); scratch holds the 0x100 byte text buffer of 0x00497d2c.
 * Verified under emulation: tools/emu/check_en7_d.sh.
 */
#ifndef TM2_EN7_D_H
#define TM2_EN7_D_H
#include "en2_env.h"

void en7_joy_state_read(En2Env* e);                                  /* 0x004a247c */
void en7_player_list_purge(En2Env* e, uint32_t o);                   /* 0x004ab2f0 */
uint32_t en7_sound_object_free(En2Env* e, uint32_t o);               /* 0x004aa450 */
void en7_screen_rect_outline(En2Env* e, uint32_t r, uint32_t g, uint32_t b); /* 0x004ad624 */
void en7_session_listbox_sync(En2Env* e, uint32_t o, uint32_t lb);   /* 0x00497d2c */

#endif
