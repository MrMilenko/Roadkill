/*
 * en4_obj.h: input device / DirectInput release, display format, list and object helpers,
 * widget destructors, key bindings and paint of TM2.EXE 0x00490000..0x004b0000 (fourth wave).
 * Environment: En2Env (en2_env.h). Verified under emulation: tools/emu/check_en4_obj.sh.
 */
#ifndef TM2_EN4_OBJ_H
#define TM2_EN4_OBJ_H
#include "en2_env.h"

uint32_t en4_input_set_device(En2Env* e, uint32_t dev, uint32_t mode);  /* 0x004932c4 */
uint32_t en4_display_pixel_format(En2Env* e);                           /* 0x0049628c */
void en4_slot_list_init(En2Env* e, uint32_t o, int32_t n);              /* 0x00496718 */
uint32_t en4_keyed_list_set(En2Env* e, uint32_t o, uint32_t key, uint32_t val); /* 0x00498194 */
uint32_t en4_dinput_release(En2Env* e);                                 /* 0x0049e44c */
uint32_t en4_widget_destroy(En2Env* e, uint32_t o, uint32_t flags);     /* 0x004a3ff4 */
uint32_t en4_window_obj_destroy(En2Env* e, uint32_t o, uint32_t flags); /* 0x004a6b10 */
uint32_t en4_fill_rect_btnface(En2Env* e, uint32_t eax, uint32_t hdc, uint32_t rect); /* 0x004a4204 */
uint32_t en4_widget_activate(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx);  /* 0x004a4484 */
uint32_t en4_widget_set_keys(En2Env* e, uint32_t o, uint32_t k1, uint32_t k2, uint32_t k3);     /* 0x004a4c00 */
uint32_t en4_widget_set_keys_alt(En2Env* e, uint32_t o, uint32_t k1, uint32_t k2, uint32_t k3); /* 0x004a4c78 */
uint32_t en4_widget_set_keys_3(En2Env* e, uint32_t o, uint32_t k1, uint32_t k2, uint32_t k3);   /* 0x004a4d68 */
uint32_t en4_create_listbox(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t style,
                            uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3);               /* 0x004a5344 */
uint32_t en4_button_key_activate(En2Env* e, uint32_t o, uint32_t key, uint32_t ebx);         /* 0x004a6020 */
uint32_t en4_dinput_device_release(En2Env* e, uint32_t o);              /* 0x004a7c94 */
uint32_t en4_dinput_device_poll(En2Env* e, uint32_t o);                 /* 0x004a8018 */
uint32_t en4_dsound_buffer_release(En2Env* e, uint32_t o);              /* 0x004aa638 */
uint32_t en4_window_paint(En2Env* e, uint32_t o);                       /* 0x004ac9c4 */
uint32_t en4_session_find_player(En2Env* e, uint32_t o, uint32_t id);   /* 0x004acce0 */

#endif
