/*
 * en4_small.h: small engine / Win32 helpers of TM2.EXE 0x00490000..0x004b0000 (fourth wave).
 * Environment: En2Env (en2_env.h): memory by game address, every call outside the function by
 * the original's address (imports by IAT slot, stdcall arguments first to last).
 * Verified under emulation: tools/emu/check_en4_small.sh.
 */
#ifndef TM2_EN4_SMALL_H
#define TM2_EN4_SMALL_H
#include "en2_env.h"

uint32_t en4_cs_set_draw_enable(En2Env* e, uint32_t cs, uint32_t v);   /* 0x00492e50 */
uint32_t en4_input_flags_set(En2Env* e, uint32_t v);                   /* 0x0049334c */
uint32_t en4_input_flags_update(En2Env* e);                            /* 0x00493354 */
uint32_t en4_input_reset_all(En2Env* e);                               /* 0x0049364c */
void en4_model_set_position(En2Env* e, int32_t i, uint32_t x, uint32_t y, uint32_t z); /* 0x004952f4 */
uint32_t en4_display_flip_n(En2Env* e, int32_t n);                     /* 0x00495890 */
uint32_t en4_display_lock_whole(En2Env* e);                            /* 0x004961b8 */
uint32_t en4_display_unlock_whole(En2Env* e);                          /* 0x004961d4 */
uint32_t en4_file_read(En2Env* e, uint32_t h, uint32_t buf, uint32_t n); /* 0x00498bb8 */
uint32_t en4_cs_reset_a3c890(En2Env* e, uint32_t v);                   /* 0x0049d494 */
void en4_pad_byte_set(En2Env* e, uint32_t i, uint32_t v);              /* 0x004a1f6c */
void en4_nop_4a2054(En2Env* e);                                        /* 0x004a2054 */
uint32_t en4_thunk_49eab8(En2Env* e);                                  /* 0x004a2060 */
uint32_t en4_input_get_num_players(En2Env* e);                         /* 0x004a2470 */
uint32_t en4_list_get_cursel(En2Env* e, uint32_t o);                   /* 0x004a54cc */
uint32_t en4_list_sm_189(En2Env* e, uint32_t o, uint32_t w, uint32_t l); /* 0x004a559c */
uint32_t en4_listview_get_text(En2Env* e, uint32_t o, uint32_t text, uint32_t item, uint32_t sub); /* 0x004a6564 */
uint32_t en4_widget_set_label(En2Env* e, uint32_t o, uint32_t text);   /* 0x004a6f08 */
uint32_t en4_widget_set_focus(En2Env* e, uint32_t o);                  /* 0x004a6f80 */
void en4_game_longjmp_quit(En2Env* e);                                 /* 0x004a8f24 */
uint32_t en4_widget_mgr_begin(En2Env* e);                              /* 0x004a8f34 */
uint32_t en4_mem_free(En2Env* e, uint32_t p);                          /* 0x004aa39c, 0x004aa018 */
uint32_t en4_get_18(En2Env* e, uint32_t o);                            /* 0x004aaa58 */
uint32_t en4_get_0c(En2Env* e, uint32_t o);                            /* 0x004aaa5c */
uint32_t en4_set_1c_via_30(En2Env* e, uint32_t o, uint32_t v);         /* 0x004aaa74 */
uint32_t en4_addr_5c(En2Env* e, uint32_t o);                           /* 0x004ac040 */
uint32_t en4_addr_dc(En2Env* e, uint32_t o);                           /* 0x004ac06c */
uint32_t en4_get_1dc(En2Env* e, uint32_t o);                           /* 0x004ac0ac */
uint32_t en4_set_58(En2Env* e, uint32_t o, uint32_t v);                /* 0x004ac0b4 */
uint32_t en4_get_58(En2Env* e, uint32_t o);                            /* 0x004ac0b8 */
uint32_t en4_thread_entry(En2Env* e, uint32_t arg);                    /* 0x004ac494 (stdcall) */
uint32_t en4_reg_read_int(En2Env* e, uint32_t o, uint32_t name, uint32_t out); /* 0x004ac5bc */
uint32_t en4_reg_write_int(En2Env* e, uint32_t o, uint32_t name, uint32_t v);  /* 0x004ac614 */
uint32_t en4_set_render_flag_a(En2Env* e, uint32_t v);                 /* 0x004ad21c */
uint32_t en4_cs_reset(En2Env* e, uint32_t cs);                         /* 0x00492c60 */
uint32_t en4_input_mode_reset(En2Env* e);                              /* 0x0049eab8 */
uint32_t en4_dperr_name(En2Env* e, uint32_t o, uint32_t code);         /* 0x004aac00 */

#endif
