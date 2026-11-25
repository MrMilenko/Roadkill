/*
 * en4_io.h: input checks, view / texture slot setup, file loading, keyed lists, DirectPlay /
 * DirectSound / thread teardown, dialogs and widget constructors of TM2.EXE
 * 0x00490000..0x004b0000 (fourth wave). Environment: En2Env (en2_env.h); these ports keep stack
 * buffers of up to 0x268 bytes in the scratch area, so En2Env.scratch must have 0x400 bytes.
 * Verified under emulation: tools/emu/check_en4_io.sh.
 */
#ifndef TM2_EN4_IO_H
#define TM2_EN4_IO_H
#include "en2_env.h"

uint32_t en4_input_check(En2Env* e, uint32_t slot, uint32_t k);        /* 0x004936ac */
uint32_t en4_view_select(En2Env* e, uint32_t i);                       /* 0x0049456c */
uint32_t en4_tables_load(En2Env* e);                                   /* 0x00495924 */
uint32_t en4_slot_list_init_fwd(En2Env* e, uint32_t o, int32_t n);     /* 0x00496774 */
uint32_t en4_keyed_list_set_kind(En2Env* e, uint32_t o, uint32_t key, uint32_t kind); /* 0x004981e0 */
uint32_t en4_keyed_list_set_mode(En2Env* e, uint32_t o, uint32_t key, uint32_t v);    /* 0x00498250 */
void en4_records_reset(En2Env* e, uint32_t o, int32_t start);          /* 0x00498380 */
uint32_t en4_dplay_create_player(En2Env* e, uint32_t o, uint32_t dp);  /* 0x004988e8 */
uint32_t en4_file_load(En2Env* e, uint32_t dest, uint32_t path);       /* 0x00498d18 */
uint32_t en4_key_state_snapshot(En2Env* e);                            /* 0x0049e9d8 */
uint32_t en4_key_clear(En2Env* e, uint32_t k);                         /* 0x0049eb88 */
uint32_t en4_pad_last_button(En2Env* e);                               /* 0x004a2828 */
uint32_t en4_widget_base_init(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                              uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4); /* 0x004a42c0 */
uint32_t en4_widget_set_nav_keys(En2Env* e, uint32_t o, uint32_t k1, uint32_t k2, uint32_t k3, uint32_t k4); /* 0x004a4b50 */
uint32_t en4_widget_create_panel(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t style,
                                 uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3);   /* 0x004a5290 */
uint32_t en4_destroy_window(En2Env* e, uint32_t o);                    /* 0x004a6c98 */
uint32_t en4_dialog_show(En2Env* e, uint32_t o, uint32_t parent, uint32_t templ); /* 0x004a9de0 */
uint32_t en4_dsound_sound_free(En2Env* e, uint32_t o);                 /* 0x004aa5b8 */
uint32_t en4_thread_stop(En2Env* e, uint32_t o);                       /* 0x004abd38 */

#endif
