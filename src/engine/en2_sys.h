/*
 * en2_sys.h: system, input and widget helpers of TM2.EXE 0x00488000..0x004b0000, ported from the
 * machine code. Verified under emulation: tools/emu/check_en2_sys.sh. Runs in an En2Env (en2_env.h).
 *
 *   0x0049635c en2_hw3d_query_b5fb8  hardware 3D on (0x004958ac) ? 0x004b5fb8(display) : 0
 *   0x0049632c en2_hw3d_query_b5f94  off: 0x004ad848(), returns 0; on: 0x004b5f94(display)
 *   0x004966d8 en2_texture_setup     off: 0x00496774(a, d); on: 0x00496718(a, d) (subsys_texture_init)
 *   0x0049ecf4 en2_key_down          AL = [0x00a5d5fc] && key table 0x00a5d810[k & 0xff]
 *   0x0049eb5c en2_key_take          the same, and clears the key when it was set
 *   0x004a1f40 en2_pairs_reset       0x100 bytes at 0x00a81e4c and at 0x00a81f4c = 0, then 0x0049e110()
 *   0x004a21d4 en2_tables_204c_reset 0x80 bytes at 0x00a8204c and 0x00a820cc = 0, then 0x004c3e54(0x00a82164)
 *   0x00497828 en2_files_close_all   512 records of 0x18 bytes at 0x00a2dc48: when the first word is
 *                                    set, 0x004c101c(record) (Watcom runtime); returns 0
 *   0x00498c70 en2_handle_close      if [0x004e8428]: CloseHandle([0x00a39c64]), [0x00a39c64] = 0;
 *                                    returns [0x004e8428]
 *   0x004acad0 en2_timer_cb_70       if o+0x70: 0x0049e0c0(o+0x3c), o+0x40 = 0, (*o+0x70)(o, EDX, o+0x5c); 0
 *   0x004aca68 en2_timer_cb_68       0x0049e0c0(o+0x3c), o+0x40 = 0, if o+0x68: (*o+0x68)(o, EDX, o+0x54); 0
 *   0x004acaf8 en2_timer_cb_60       o+0x60 ? (*o+0x60)(o, o+0x4c, o) : 2
 *   0x00489078 en2_obj_489078        0x00484260(0x1d); +0x90 = 0x9999, +0x98 = 0, +0x94 = +0x98,
 *                                    +0x8c = 0xe666; 0x00486bb4(o, 2); returns 0x0048859c(o)
 *   0x004984e0 en2_obj_free_slots    0x00498380(o, 0); free 0x004aa39c of the words o+8 .. o+0x20; returns o
 *   0x004a4104 en2_brush_free        DeleteObject(o+0x44) if set; 0x004a6c98(o); returns 0
 *   0x004a4180 en2_brush_set         DeleteObject(o+0x44) if set; o+0x44 = CreateSolidBrush(color); 1
 *   0x004ac528 en2_reg_open          RegOpenKeyExA(EDX root, EBX sub or "" at 0x00bdfb38 when 0, 0,
 *                                    KEY_ALL_ACCESS, EAX out)
 *   0x004a9e4c en2_dialog_end        o+0x54 == 1: EndDialog([o], EDX), [o] = 0, returns its result;
 *                                    else returns ECX unchanged (ORIGINAL: no defined result)
 *   0x004a9ff8 en2_block_header      p ? (0x004c156c(p, [p - 4], EDX), p - 4) : 0 (runtime heap helper)
 *   0x004a44d0 en2_dialog_key        msg 0xd: vtable o+0x34 slot 0xd4 (o, 1), 0; 0x20: 0; else
 *                                    0x004a798c(o, msg, EBX, ECX)
 *   0x004ac900 en2_key_event         render mode 2 only: k = EDX, w = u16 0x004e9240[k]; 0x00a5d710[w]
 *                                    = 1; k == 0x10: [0x00a5da20] = 1; 0x0049ed48(low byte),
 *                                    0x0049dfac(low byte); always returns 1
 */
#ifndef TM2_EN2_SYS_H
#define TM2_EN2_SYS_H

#include "en2_env.h"

uint32_t en2_hw3d_query_b5fb8(En2Env* e);
uint32_t en2_hw3d_query_b5f94(En2Env* e);
uint32_t en2_texture_setup(En2Env* e, uint32_t a, uint32_t d);   /* edx: slot count, passed through */
uint32_t en2_key_down(En2Env* e, uint32_t k);
uint32_t en2_key_take(En2Env* e, uint32_t k);
void en2_pairs_reset(En2Env* e);
void en2_tables_204c_reset(En2Env* e);
uint32_t en2_files_close_all(En2Env* e);
uint32_t en2_handle_close(En2Env* e);
uint32_t en2_timer_cb_70(En2Env* e, uint32_t o, uint32_t edx);
uint32_t en2_timer_cb_68(En2Env* e, uint32_t o, uint32_t edx);
uint32_t en2_timer_cb_60(En2Env* e, uint32_t o);
uint32_t en2_obj_489078(En2Env* e, uint32_t o);
uint32_t en2_obj_free_slots(En2Env* e, uint32_t o);
uint32_t en2_brush_free(En2Env* e, uint32_t o);
uint32_t en2_brush_set(En2Env* e, uint32_t o, uint32_t color);
uint32_t en2_reg_open(En2Env* e, uint32_t out, uint32_t root, uint32_t sub);
uint32_t en2_dialog_end(En2Env* e, uint32_t o, uint32_t result, uint32_t ecx);
uint32_t en2_block_header(En2Env* e, uint32_t p, uint32_t edx);
uint32_t en2_dialog_key(En2Env* e, uint32_t o, uint32_t msg, uint32_t ebx, uint32_t ecx);
uint32_t en2_key_event(En2Env* e, uint32_t edx);

#endif
