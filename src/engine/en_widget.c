/*
 * en_widget.c: small pure accessors of the widget / menu code, ported from TM2.EXE (see
 * en_widget.h for the addresses).
 */
#include "en_widget.h"

#include <string.h>

static uint32_t r32(EnWidgetEnv* e, uint32_t a) { uint32_t v; memcpy(&v, e->mem(e->user, a), 4); return v; }
static void w32(EnWidgetEnv* e, uint32_t a, uint32_t v) { memcpy(e->mem(e->user, a), &v, 4); }
static uint8_t r8(EnWidgetEnv* e, uint32_t a) { return *e->mem(e->user, a); }
static void w8(EnWidgetEnv* e, uint32_t a, uint8_t v) { *e->mem(e->user, a) = v; }
static void clear(EnWidgetEnv* e, uint32_t a, uint32_t n) { for (uint32_t i = 0; i < n; i++) w8(e, a + i, 0); }

uint8_t en_wg_pair_get_a(EnWidgetEnv* e, uint32_t i) { return r8(e, 0xa81e4cu + 2u * (i & 0xff)); }
void en_wg_pair_clear(EnWidgetEnv* e) { clear(e, 0xa81e4cu, 0x100); }
void en_wg_pair_set_b(EnWidgetEnv* e, uint32_t i, uint8_t v, uint32_t ofs) { w8(e, ofs + 2u * (i & 0xff) + 0xa81f4cu, v); }
uint8_t en_wg_pair_get_b(EnWidgetEnv* e, uint32_t i) { return r8(e, 0xa81f4cu + 2u * (i & 0xff)); }
uint32_t en_wg_pair_table(EnWidgetEnv* e, uint32_t size_out) { w32(e, size_out, 0x100); return 0xa81e4cu; }
uint8_t en_wg_t204c_get(EnWidgetEnv* e, uint32_t i) { return r8(e, 0xa8204cu + (i & 0xff)); }
void en_wg_t204c_clear(EnWidgetEnv* e) { clear(e, 0xa8204cu, 0x80); }
void en_wg_t20cc_set(EnWidgetEnv* e, uint32_t i, uint8_t v) { w8(e, 0xa820ccu + (i & 0xff), v); }
void en_wg_t214c_clear(EnWidgetEnv* e) { clear(e, 0xa8214cu, 8); }
uint32_t en_wg_t204c_table(EnWidgetEnv* e, uint32_t size_out) { w32(e, size_out, 0x80); return 0xa8204cu; }
void en_wg_t23fc_set(EnWidgetEnv* e, uint32_t i, uint8_t v) { w8(e, 0xa823fcu + (i & 0xff), v); }
uint8_t en_wg_t23fc_get(EnWidgetEnv* e, uint32_t i) { return r8(e, 0xa823fcu + (i & 0xff)); }
void en_wg_t23fc_clear(EnWidgetEnv* e) { clear(e, 0xa823fcu, 0x80); }
void en_wg_t247c_set(EnWidgetEnv* e, uint32_t i, uint8_t v) { w8(e, 0xa8247cu + (i & 0xff), v); }
uint8_t en_wg_t247c_get(EnWidgetEnv* e, uint32_t i) { return r8(e, 0xa8247cu + (i & 0xff)); }
void en_wg_t24fc_clear(EnWidgetEnv* e) { clear(e, 0xa824fcu, 0x32); }
uint32_t en_wg_t23fc_table(EnWidgetEnv* e, uint32_t size_out) { w32(e, size_out, 0x80); return 0xa823fcu; }

uint8_t en_wg_get_focus(EnWidgetEnv* e, int32_t kind, uint32_t i) {
    uint8_t r = 0;
    if (kind == 1) r = en_wg_pair_get_a(e, i & 0xff);
    if (kind == 2) r = en_wg_t204c_get(e, i & 0xff);
    if (kind == 4) r = en_wg_t23fc_get(e, i & 0xff);
    return r;
}

uint32_t en_wg_field4_addr(uint32_t obj) { return obj + 4; }
uint32_t en_wg_entry(EnWidgetEnv* e, uint32_t obj, int32_t i) { return r32(e, r32(e, obj + 4) + (uint32_t)i * 8u); }
void en_wg_set_c8b9f0(EnWidgetEnv* e) { w32(e, 0xc8b9f0u, 0x4e84acu); }
void en_wg_list_push(EnWidgetEnv* e, uint32_t node) { w32(e, node, r32(e, 0x4e91e8u)); w32(e, 0x4e91e8u, node); }
void en_wg_reset_a(EnWidgetEnv* e, uint32_t obj) {
    for (uint32_t o = 0x14; o <= 0x24; o += 4) w32(e, obj + o, 0);
    w32(e, obj + 0x30, 0x4e58fcu);
}
uint32_t en_wg_zero(void) { return 0; }
uint32_t en_wg_word0(EnWidgetEnv* e, uint32_t obj) { return r32(e, obj); }
uint32_t en_wg_word1(EnWidgetEnv* e, uint32_t obj) { return r32(e, obj + 4); }
uint32_t en_wg_word3(EnWidgetEnv* e, uint32_t obj) { return r32(e, obj + 0xc); }
void en_wg_reset_b(EnWidgetEnv* e, uint32_t obj) {
    w32(e, obj + 0x6a8, 0); w32(e, obj + 0x6ac, 0); w32(e, obj + 0x6b0, 0); w32(e, obj + 0x6b4, 0);
    w32(e, obj + 0x298, 0); w32(e, obj + 0x29c, 6); w32(e, obj + 0x2a0, 0xa); w32(e, obj + 0x6a4, 0);
}
void en_wg_clear_2a4(EnWidgetEnv* e, uint32_t obj) { clear(e, obj + 0x2a4, 0x100); }
uint32_t en_wg_set_3c(EnWidgetEnv* e, uint32_t obj, uint32_t v) { w32(e, obj + 0x3c, v); return 1; }
uint32_t en_wg_set_40(EnWidgetEnv* e, uint32_t obj, uint32_t v) { w32(e, obj + 0x40, v); return 1; }
uint32_t en_wg_set_44(EnWidgetEnv* e, uint32_t obj, uint32_t v) { w32(e, obj + 0x44, v); return 1; }
void en_wg_set_48(EnWidgetEnv* e, uint32_t obj, uint32_t v) { w32(e, obj + 0x48, v); }

static uint32_t set_field_3(EnWidgetEnv* e, uint32_t obj, uint32_t f, uint32_t v, uint32_t g, const uint32_t s[3]) {
    w32(e, obj + f, v);
    w32(e, obj + g, s[0]); w32(e, obj + g + 4, s[1]); w32(e, obj + g + 8, s[2]);
    return s[2];
}
uint32_t en_wg_set_54_5c(EnWidgetEnv* e, uint32_t obj, uint32_t v, const uint32_t s[3]) { return set_field_3(e, obj, 0x54, v, 0x5c, s); }
uint32_t en_wg_set_64_58(EnWidgetEnv* e, uint32_t obj, uint32_t v, const uint32_t s[3]) { return set_field_3(e, obj, 0x64, v, 0x58, s); }
uint32_t en_wg_set_78_6c(EnWidgetEnv* e, uint32_t obj, uint32_t v, const uint32_t s[3]) { return set_field_3(e, obj, 0x78, v, 0x6c, s); }
void en_wg_set_54z_58(EnWidgetEnv* e, uint32_t obj, uint32_t v) { w32(e, obj + 0x54, 0); w32(e, obj + 0x58, v); }
void en_wg_set_64z_54(EnWidgetEnv* e, uint32_t obj, uint32_t v) { w32(e, obj + 0x64, 0); w32(e, obj + 0x54, v); }
void en_wg_set_78z_68(EnWidgetEnv* e, uint32_t obj, uint32_t v) { w32(e, obj + 0x78, 0); w32(e, obj + 0x68, v); }
uint32_t en_wg_get_c4(EnWidgetEnv* e, uint32_t obj) { return r32(e, obj + 0xc4); }
void en_wg_set_colors(EnWidgetEnv* e, uint32_t obj, uint32_t edx, uint32_t ebx) { w32(e, obj + 0xcc, ebx); w32(e, obj + 0xd0, edx); }
void en_wg_store_instance(EnWidgetEnv* e, uint32_t v) { w32(e, 0xbdfae8u, v); }
uint32_t en_wg_flag_2a4(EnWidgetEnv* e, uint32_t obj, int32_t i) {
    return (i >= 0 && i < 0x20 && r8(e, obj + 0x2a4 + (uint32_t)i) != 0) ? 1 : 0;
}
uint8_t en_wg_axis_pos(EnWidgetEnv* e, uint32_t obj, int k) {
    uint32_t a = obj + 0x274 + 4u * (uint32_t)k;
    return (int32_t)r32(e, a) < 0 ? 0 : r8(e, a);
}
uint16_t en_wg_axis_neg(EnWidgetEnv* e, uint32_t obj, int k) {
    uint32_t a = obj + 0x274 + 4u * (uint32_t)k;
    if ((int32_t)r32(e, a) > 0) return 0;   /* only AL = 0 is defined on this path */
    return (uint16_t)(r8(e, a) * 0xffu);
}
uint32_t en_wg_cur_block(EnWidgetEnv* e, uint32_t obj) { return obj + 0x31c + r32(e, obj + 0x2cc) * 0x5098u; }
uint32_t en_wg_cur_word(EnWidgetEnv* e, uint32_t obj, uint32_t ofs) { return r32(e, obj + r32(e, obj + 0x2cc) * 0x5098u + ofs); }
uint32_t en_wg_get_bdfb24(EnWidgetEnv* e) { return r32(e, 0xbdfb24u); }

/* "commit" copies: clear the live table, then copy the edited one over it */
static void commit(EnWidgetEnv* e, uint32_t dst, uint32_t src, uint32_t n) {
    clear(e, dst, n);
    for (uint32_t i = 0; i < n; i++) w8(e, dst + i, r8(e, src + i));
}
void en_wg_pair_commit(EnWidgetEnv* e) { commit(e, 0xa81e4cu, 0xa81f4cu, 0x100); }
void en_wg_t204c_commit(EnWidgetEnv* e) { commit(e, 0xa8204cu, 0xa820ccu, 0x80); }
void en_wg_t23fc_commit(EnWidgetEnv* e) { commit(e, 0xa823fcu, 0xa8247cu, 0x80); }

uint8_t en_wg_pair_flags(EnWidgetEnv* e, uint32_t i) {
    uint8_t r = 0;
    for (uint32_t k = 0; k < 2; k++) {
        uint8_t v = r8(e, 0xa81e4cu + 2u * (i & 0xff) + k);
        if (v) r |= r8(e, 0xa5d810u + v);
    }
    return r;
}
uint8_t en_wg_t204c_map(EnWidgetEnv* e, uint32_t i) {
    uint8_t v = r8(e, 0xa8204cu + (i & 0xff));
    return v ? r8(e, 0xa8214cu + v) : 0;
}
uint8_t en_wg_t23fc_map(EnWidgetEnv* e, uint32_t i) {
    uint8_t v = r8(e, 0xa823fcu + (i & 0xff));
    return v ? r8(e, 0xa824fcu + v) : 0;
}
uint32_t en_wg_image_setup(EnWidgetEnv* e, uint32_t obj) {
    uint32_t src = r32(e, obj + 0x14);
    for (uint32_t i = 0; i < 0x80; i++) w8(e, obj + 0x34 + i, r8(e, src + i));
    uint16_t x0, y0, x1, y1;
    memcpy(&x0, e->mem(e->user, obj + 0x38), 2); memcpy(&y0, e->mem(e->user, obj + 0x3a), 2);
    memcpy(&x1, e->mem(e->user, obj + 0x3c), 2); memcpy(&y1, e->mem(e->user, obj + 0x3e), 2);
    uint32_t w = (uint32_t)x1 - x0 + 1u;
    w32(e, obj, w);
    uint32_t h = (uint32_t)y1 - y0 + 1u;
    uint32_t a = r32(e, obj) * h;
    w32(e, obj + 4, h);
    w32(e, obj + 0xc, a);
    w32(e, obj + 0x10, a + a);
    return 0;
}
uint8_t en_wg_find_80(EnWidgetEnv* e, uint32_t obj) {
    for (uint32_t i = 0; i < 0x100; i++)
        if (r8(e, obj + 0x2a4 + i) == 0x80) return (uint8_t)i;
    return 0;
}
uint32_t en_wg_set_nav_links(EnWidgetEnv* e, uint32_t obj, uint32_t a, uint32_t b, uint32_t c, const uint32_t s[3]) {
    if (!a && !b && !c && !s[0] && !s[1] && !s[2]) { w32(e, obj + 0x18, 0); return obj; }
    w32(e, obj + 0x18, 1);
    w32(e, obj + 0x1c, a); w32(e, obj + 0x20, b); w32(e, obj + 0x24, c);
    w32(e, obj + 0x28, s[0]); w32(e, obj + 0x2c, s[1]); w32(e, obj + 0x30, s[2]);
    return obj;
}
uint32_t en_wg_nav_link(EnWidgetEnv* e, uint32_t obj, int32_t i) {
    return (i >= 0 && i <= 5) ? r32(e, obj + 0x1c + 4u * (uint32_t)i) : 0;
}
void en_wg_set_bdfaec(EnWidgetEnv* e, uint32_t v) { w32(e, 0xbdfaecu, v); }
uint32_t en_wg_slot_294(EnWidgetEnv* e, uint32_t obj, int32_t i, int32_t v) {
    if (i < 0 || i >= 4) return 0;
    uint32_t x = r32(e, obj + 0x294 + 4u * (uint32_t)i);
    if (v == -1) return x;
    return x == (uint32_t)v ? 1 : 0;
}
