/*
 * en_widget.h: the small pure accessors of the menu / widget code in TM2.EXE 0x004a1f8c..
 * 0x004a9018, ported from the machine code. The Win32 wrappers of that range (SendMessage
 * through [0x00c90550], window styles, focus, GetSysColor: widget_handle_action, widget_nav_*,
 * widget_set_label / _focus / _property, widget_alloc, ...) are platform and not here.
 *
 * Everything is addressed in the game's 32 bit address space through env->mem() (some of these
 * store addresses: a list head, a vtable, a table address).
 *
 * Byte tables (meaning not traced; indices are u8):
 *   0x00a81e4c / 0x00a81f4c pairs (stride 2): get 0x004a1f8c / 0x004a2008, set 0x004a1ff8,
 *     clear 0x004a1f9c (0x100 bytes at 0x00a81e4c), "size" 0x004a217c (0x100, table address)
 *   0x00a8204c bytes: get 0x004a2214, clear 0x004a2220 (0x80), size 0x004a25ec (0x80)
 *   0x00a820cc bytes: set 0x004a2278;  0x00a8214c: clear 0x004a22cc (8 bytes)
 *   0x00a823fc bytes: get 0x004a2758, set 0x004a274c, clear 0x004a2764 (0x80), size 0x004a2d9c
 *   0x00a8247c bytes: get 0x004a27c8, set 0x004a27bc;  0x00a824fc: clear 0x004a2810 (0x32)
 *   widget_mgr_get_focus 0x004a8fe8(kind, i): kind 1 -> 0x004a1f8c, 2 -> 0x004a2214,
 *     4 -> 0x004a2758, else 0 (only AL is defined)
 */
#ifndef TM2_EN_WIDGET_H
#define TM2_EN_WIDGET_H

#include <stdint.h>

typedef struct {
    uint8_t* (*mem)(void* user, uint32_t addr);
    void* user;
} EnWidgetEnv;

uint8_t en_wg_pair_get_a(EnWidgetEnv* e, uint32_t i);                       /* 0x004a1f8c */
void en_wg_pair_clear(EnWidgetEnv* e);                                      /* 0x004a1f9c */
void en_wg_pair_set_b(EnWidgetEnv* e, uint32_t i, uint8_t v, uint32_t ofs); /* 0x004a1ff8 (EBX ofs) */
uint8_t en_wg_pair_get_b(EnWidgetEnv* e, uint32_t i);                       /* 0x004a2008 */
uint32_t en_wg_pair_table(EnWidgetEnv* e, uint32_t size_out);               /* 0x004a217c */
uint8_t en_wg_t204c_get(EnWidgetEnv* e, uint32_t i);                        /* 0x004a2214 */
void en_wg_t204c_clear(EnWidgetEnv* e);                                     /* 0x004a2220 */
void en_wg_t20cc_set(EnWidgetEnv* e, uint32_t i, uint8_t v);                /* 0x004a2278 */
void en_wg_t214c_clear(EnWidgetEnv* e);                                     /* 0x004a22cc */
uint32_t en_wg_t204c_table(EnWidgetEnv* e, uint32_t size_out);              /* 0x004a25ec */
void en_wg_t23fc_set(EnWidgetEnv* e, uint32_t i, uint8_t v);                /* 0x004a274c */
uint8_t en_wg_t23fc_get(EnWidgetEnv* e, uint32_t i);                        /* 0x004a2758 widget_get_prev */
void en_wg_t23fc_clear(EnWidgetEnv* e);                                     /* 0x004a2764 */
void en_wg_t247c_set(EnWidgetEnv* e, uint32_t i, uint8_t v);                /* 0x004a27bc */
uint8_t en_wg_t247c_get(EnWidgetEnv* e, uint32_t i);                        /* 0x004a27c8 widget_get_value */
void en_wg_t24fc_clear(EnWidgetEnv* e);                                     /* 0x004a2810 */
uint32_t en_wg_t23fc_table(EnWidgetEnv* e, uint32_t size_out);              /* 0x004a2d9c */
uint8_t en_wg_get_focus(EnWidgetEnv* e, int32_t kind, uint32_t i);          /* 0x004a8fe8 */

/* object accessors (obj = address of the object) */
uint32_t en_wg_field4_addr(uint32_t obj);                                   /* 0x004a2e28: obj + 4 */
uint32_t en_wg_entry(EnWidgetEnv* e, uint32_t obj, int32_t i);              /* 0x004a2e2c: [[obj+4] + 8 i] */
void en_wg_set_c8b9f0(EnWidgetEnv* e);                                      /* 0x004a2e38: [0x00c8b9f0] = 0x004e84ac */
void en_wg_list_push(EnWidgetEnv* e, uint32_t node);                        /* 0x004a2e43: [node] = [0x004e91e8], head = node */
void en_wg_reset_a(EnWidgetEnv* e, uint32_t obj);                           /* 0x004a2e60: +0x14..+0x24 = 0, vtable +0x30 = 0x004e58fc */
uint32_t en_wg_zero(void);                                                  /* 0x004a2ff0 */
uint32_t en_wg_word0(EnWidgetEnv* e, uint32_t obj);                         /* 0x004a2ff4, 0x004a6dcc ddraw_get_hwnd */
uint32_t en_wg_word1(EnWidgetEnv* e, uint32_t obj);                         /* 0x004a2ff8 */
uint32_t en_wg_word3(EnWidgetEnv* e, uint32_t obj);                         /* 0x004a6dd4 ddraw_get_accel */
void en_wg_reset_b(EnWidgetEnv* e, uint32_t obj);                           /* 0x004a3620 */
void en_wg_clear_2a4(EnWidgetEnv* e, uint32_t obj);                         /* 0x004a3d88: 0x100 bytes at +0x2a4 */
uint32_t en_wg_set_3c(EnWidgetEnv* e, uint32_t obj, uint32_t v);            /* 0x004a4168, returns 1 */
uint32_t en_wg_set_40(EnWidgetEnv* e, uint32_t obj, uint32_t v);            /* 0x004a4174, returns 1 */
uint32_t en_wg_set_44(EnWidgetEnv* e, uint32_t obj, uint32_t v);            /* 0x004a41ac, returns 1 */
void en_wg_set_48(EnWidgetEnv* e, uint32_t obj, uint32_t v);                /* 0x004a41b8 */
/* 0x004a4458 / 0x004a593c / 0x004a5968 (EAX obj, EDX v, 3 stack words; ret 0xc; returns the
 * last stack word): field = v, three words after it */
uint32_t en_wg_set_54_5c(EnWidgetEnv* e, uint32_t obj, uint32_t v, const uint32_t s[3]);
uint32_t en_wg_set_64_58(EnWidgetEnv* e, uint32_t obj, uint32_t v, const uint32_t s[3]);
uint32_t en_wg_set_78_6c(EnWidgetEnv* e, uint32_t obj, uint32_t v, const uint32_t s[3]);
void en_wg_set_54z_58(EnWidgetEnv* e, uint32_t obj, uint32_t v);            /* 0x004a4478: +0x54 = 0, +0x58 = v */
void en_wg_set_64z_54(EnWidgetEnv* e, uint32_t obj, uint32_t v);            /* 0x004a595c */
void en_wg_set_78z_68(EnWidgetEnv* e, uint32_t obj, uint32_t v);            /* 0x004a5988 */
uint32_t en_wg_get_c4(EnWidgetEnv* e, uint32_t obj);                        /* 0x004a4a78 */
void en_wg_set_colors(EnWidgetEnv* e, uint32_t obj, uint32_t edx, uint32_t ebx); /* 0x004a4de0 widget_set_colors: +0xcc = EBX, +0xd0 = EDX */
void en_wg_store_instance(EnWidgetEnv* e, uint32_t v);                      /* 0x004a6b78: [0x00bdfae8] = v */
uint32_t en_wg_flag_2a4(EnWidgetEnv* e, uint32_t obj, int32_t i);           /* 0x004a8078: 0 <= i < 32 && byte +0x2a4 + i */
/* 0x004a809c .. 0x004a81a0: the six i32 axes at +0x274 .. +0x288 (k = 0..5): positive part as
 * a byte (AL), or for the negative getters (AX) the low byte times 0xff, i.e. AL = -value */
uint8_t en_wg_axis_pos(EnWidgetEnv* e, uint32_t obj, int k);
uint16_t en_wg_axis_neg(EnWidgetEnv* e, uint32_t obj, int k);
uint32_t en_wg_cur_block(EnWidgetEnv* e, uint32_t obj);                     /* 0x004a81e8: obj + 0x31c + 0x5098 [obj+0x2cc] */
uint32_t en_wg_cur_word(EnWidgetEnv* e, uint32_t obj, uint32_t ofs);        /* 0x004a822c (+0x52c), 0x004a8240 (+0x530), 0x004a825c (+0x53b0) */
uint32_t en_wg_get_bdfb24(EnWidgetEnv* e);                                  /* 0x004a8f40 */

/* 0x004a2018 / 0x004a2290 / 0x004a27d4: clear the live table and copy the edited one over it
 * (0x00a81f4c -> 0x00a81e4c 0x100 bytes, 0x00a820cc -> 0x00a8204c 0x80, 0x00a8247c ->
 * 0x00a823fc 0x80) */
void en_wg_pair_commit(EnWidgetEnv* e);
void en_wg_t204c_commit(EnWidgetEnv* e);
void en_wg_t23fc_commit(EnWidgetEnv* e);
/* 0x004a2084: OR of [0x00a5d810 + v] over the two nonzero bytes v of pair i (AL) */
uint8_t en_wg_pair_flags(EnWidgetEnv* e, uint32_t i);
/* 0x004a244c / 0x004a28ac: v = [0x00a8204c / 0x00a823fc + i]; v ? [0x00a8214c / 0x00a824fc + v] : 0 (AL) */
uint8_t en_wg_t204c_map(EnWidgetEnv* e, uint32_t i);
uint8_t en_wg_t23fc_map(EnWidgetEnv* e, uint32_t i);
/* 0x004a3438: copies 0x80 bytes from [obj + 0x14] to obj + 0x34, then from the u16 rectangle
 * x0 +0x38, y0 +0x3a, x1 +0x3c, y1 +0x3e: +0 = x1 - x0 + 1, +4 = y1 - y0 + 1, +0xc = w h,
 * +0x10 = 2 w h; returns 0 */
uint32_t en_wg_image_setup(EnWidgetEnv* e, uint32_t obj);
/* 0x004a3e24: first i < 0x100 with byte [obj + 0x2a4 + i] == 0x80, else 0 (AL) */
uint8_t en_wg_find_80(EnWidgetEnv* e, uint32_t obj);
/* 0x004a70ac widget_set_nav_links (EAX obj, EDX, EBX, ECX, 3 stack words; ret 0xc): all six 0
 * -> +0x18 = 0; else +0x18 = 1 and +0x1c..+0x30 = the six. 0x004a70fc: link i (0..5) or 0.
 * 0x004a7110: [0x00bdfaec] = EDX. */
uint32_t en_wg_set_nav_links(EnWidgetEnv* e, uint32_t obj, uint32_t a, uint32_t b, uint32_t c, const uint32_t s[3]);
uint32_t en_wg_nav_link(EnWidgetEnv* e, uint32_t obj, int32_t i);
void en_wg_set_bdfaec(EnWidgetEnv* e, uint32_t v);
/* 0x004a81b8 (EAX obj, EDX i, EBX v): 0 unless 0 <= i < 4; v == -1: [obj + 0x294 + 4 i];
 * else that word == v */
uint32_t en_wg_slot_294(EnWidgetEnv* e, uint32_t obj, int32_t i, int32_t v);

#endif /* TM2_EN_WIDGET_H */
