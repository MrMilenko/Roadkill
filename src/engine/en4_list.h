/*
 * en4_list.h: session / player lists, DirectPlay callbacks, joystick state, window and control
 * creation, palettes and window activation of TM2.EXE 0x00490000..0x004b0000 (fourth wave).
 * Environment: En2Env (en2_env.h). Verified under emulation: tools/emu/check_en4_list.sh.
 */
#ifndef TM2_EN4_LIST_H
#define TM2_EN4_LIST_H
#include "en2_env.h"

uint32_t en4_keyed_list_add(En2Env* e, uint32_t o);                     /* 0x00497a68 */
uint32_t en4_session_enum(En2Env* e, uint32_t o, uint32_t dp, uint32_t ebx, uint32_t mode); /* 0x00497c7c */
uint32_t en4_enum_players_cb(En2Env* e, uint32_t id, uint32_t type, uint32_t name, uint32_t flags, uint32_t o); /* 0x004982d4 */
uint32_t en4_joy_direction(En2Env* e);                                  /* 0x004a22e4 */
uint32_t en4_joy_open(En2Env* e);                                       /* 0x004a2644 */
uint32_t en4_control_create(En2Env* e, uint32_t o, uint32_t cls, uint32_t parent, uint32_t id,
                            uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5); /* 0x004a4068 */
uint32_t en4_widget_fire_a(En2Env* e, uint32_t o, uint32_t which, uint32_t ebx);  /* 0x004a59b0 */
uint32_t en4_widget_fire_b(En2Env* e, uint32_t o, uint32_t which, uint32_t ebx);  /* 0x004a5e6c */
uint32_t en4_create_main_window(En2Env* e, uint32_t o, uint32_t exstyle, uint32_t cls, uint32_t name,
                                uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5,
                                uint32_t menu, uint32_t hinst);          /* 0x004a6be8 */
uint32_t en4_palette_create(En2Env* e, uint32_t o);                     /* 0x004aa1bc */
uint32_t en4_window_activate(En2Env* e, uint32_t o, uint32_t active);   /* 0x004ac718 */

#endif
