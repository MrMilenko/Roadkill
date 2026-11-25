/*
 * en4_widget.c: widget, window class, list view and subsystem glue of TM2.EXE
 * 0x00490000..0x004b0000 (see en4_widget.h). Verified under emulation: tools/emu/check_en4_widget.sh.
 */
#include "en4_widget.h"

#define EN4_lstrlenA         0x00c90320u
#define EN4_DefWindowProcA   0x00c90484u
#define EN4_LoadIconA        0x00c90510u
#define EN4_LoadCursorA      0x00c9050cu
#define EN4_RegisterClassA   0x00c9053cu
#define EN4_SetWindowLongA   0x00c90578u

/* 0x00492df4 (subsys_input_finalize, really the coordinate system root setup): counter 0x00a03644
 * = 0, csReset(root 0x009f7e44), current 0x009f7e38 = the root, 0x009f7f9c = 9, 0x009f7e34 =
 * 0x009f7e3c = 0, i16 0x009f7fa8 = 0, counter = its value + 1 (read after the reset). */
uint32_t en4_cs_root_init(En2Env* e)
{
    en2_w32(e, 0x00a03644u, 0);
    uint32_t r = en2_call1(e, 0x00492c60u, 0x009f7e44u);
    uint32_t n = en2_r32(e, 0x00a03644u);
    en2_w32(e, 0x009f7e38u, 0x009f7e44u);
    en2_w32(e, 0x009f7f9cu, 9);
    en2_w32(e, 0x009f7e34u, 0);
    en2_w32(e, 0x009f7e3cu, 0);
    en2_w16(e, 0x009f7fa8u, 0);
    en2_w32(e, 0x00a03644u, n + 1);
    return r;
}

/* 0x00492ed4 (Ghidra: particles_update; it loads a view's camera): m = 0x00494e64(a),
 * camera_load_orient(0x00494e84(a)), camera_load_position(m), then the empty 0x0048bc38 three
 * times with the current coordinate system's (0x009f7e38) +4 record: +0x3c, +0x0c, and its
 * first word (edx / ebx = its next two words). */
uint32_t en4_view_load_camera(En2Env* e, uint32_t a)
{
    uint32_t cs = en2_r32(e, 0x009f7e38u);
    uint32_t m = en2_call1(e, 0x00494e64u, a);
    uint32_t o = en2_call1(e, 0x00494e84u, a);
    en2_call1(e, 0x0048b9b4u, o);
    en2_call1(e, 0x0048ba84u, m);
    en2_call1(e, 0x0048bc38u, en2_r32(e, cs + 4) + 0x3c);
    en2_call1(e, 0x0048bc38u, en2_r32(e, cs + 4) + 0x0c);
    uint32_t p = en2_r32(e, cs + 4);
    return en2_call1(e, 0x0048bc38u, en2_r32(e, p));
}

/* 0x00493748 (input_poll): 0x0049e108() == 0 ? 0x004a2060() (key state reset) : 0x004a2054()
 * (empty); then the device pollers 0x0049ee74, 0x004a2058, 0x004a22cc, 0x004a2810, 0x004a20c4,
 * 0x004a247c, 0x004a28d0; if key 0x29 was pressed (0x0049eb30, low byte) the toggle 0x00a03a88
 * = 1 - itself and the key is consumed (0x0049eb88(0x29)). */
uint32_t en4_input_poll(En2Env* e)
{
    if (en2_call0(e, 0x0049e108u) == 0) en2_call0(e, 0x004a2060u);
    else en2_call0(e, 0x004a2054u);
    static const uint32_t f[] = {0x0049ee74u, 0x004a2058u, 0x004a22ccu, 0x004a2810u, 0x004a20c4u,
                                 0x004a247cu, 0x004a28d0u};
    for (unsigned k = 0; k < 7; k++) en2_call0(e, f[k]);
    uint32_t r = en2_call1(e, 0x0049eb30u, 0x29);
    if ((r & 0xff) != 0) {
        en2_w32(e, 0x00a03a88u, 1 - en2_r32(e, 0x00a03a88u));
        r = en2_call1(e, 0x0049eb88u, 0x29);
    }
    return r;
}

/* 0x004a4504: a button's key handler: Enter (0x0d) -> vtable +0xd4 (o, 0) (eax = o, edx = 0, ebx
 * = the vtable, ecx = o), then SendMessageA([o], BM_CLICK, 0, 0), returns 0; space (0x20) -> 0;
 * any other key -> 2. */
uint32_t en4_button_key(En2Env* e, uint32_t o, uint32_t key, uint32_t ebx, uint32_t ecx)
{
    (void)ebx; (void)ecx;
    if (key == 0x0d) {
        uint32_t vt = en2_r32(e, o + 0x34);
        en2_call4(e, en2_r32(e, vt + 0xd4), o, 0, vt, o);
        en2_call4(e, EN2_SendMessageA, en2_r32(e, o), 0xf5, 0, 0);
        return 0;
    }
    return key == 0x20 ? 0 : 2;
}

/* 0x004a4a24: o +0xc0 = style s0, +0xc8 = 2 + (style bit 3 set), then widget_finalize 0x004a4354
 * (o, edx, ebx, ecx; (s0 & 0xffff0000) | 0x0b, s1, s2, s3, s4). Pops its five stack words. */
uint32_t en4_widget_style_finalize(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx,
                                   uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4)
{
    en2_w32(e, o + 0xc0, s0);
    en2_w32(e, o + 0xc8, ((en2_r8(e, o + 0xc0) & 8) != 0) + 2u);
    uint32_t v[9] = {o, edx, ebx, ecx, (s0 & 0xffff0000u) | 0x0bu, s1, s2, s3, s4};
    return e->call(e->user, 0x004a4354u, v, 9);
}

/* 0x004a4b04: with edx = 0 and style bit 2 (+0xc0), the state +0xc4 steps to (+0xc4 + 1) mod
 * +0xc8 (signed idiv remainder) and 0x004a6e2c(o, 0, 0) redraws; then 0x004a4484(o, edx, ebx). */
uint32_t en4_widget_cycle_input(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx)
{
    if (edx == 0 && (en2_r8(e, o + 0xc0) & 4)) {
        int32_t s = (int32_t)(en2_r32(e, o + 0xc4) + 1);
        en2_w32(e, o + 0xc4, (uint32_t)(s % (int32_t)en2_r32(e, o + 0xc8)));
        en2_call3(e, 0x004a6e2cu, o, 0, 0);
    }
    return en2_call3(e, 0x004a4484u, o, edx, ebx);
}

/* 0x004a5f40 (widget_create_sprite): o = widget_alloc 0x004a3fa0(eax, edx); vtable +0x34 =
 * 0x004e5b00; +0x54, +0x60, +0x64, +0x5c, +0x58 = 0; then vtable +0xc4 (o, edx, ebx, ecx;
 * s0 | 0x40000000 (WS_CHILD), s1, s2, s3, s4); returns o. Pops its five stack words. */
uint32_t en4_widget_create_sprite(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                                  uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4)
{
    uint32_t o = en2_call2(e, 0x004a3fa0u, eax, edx);
    en2_w32(e, o + 0x34, 0x004e5b00u);
    en2_w32(e, o + 0x54, 0);
    en2_w32(e, o + 0x60, 0);
    en2_w32(e, o + 0x64, 0);
    en2_w32(e, o + 0x5c, 0);
    en2_w32(e, o + 0x58, 0);
    uint32_t v[9] = {o, edx, ebx, ecx, s0 | 0x40000000u, s1, s2, s3, s4};
    e->call(e->user, en2_r32(e, en2_r32(e, o + 0x34) + 0xc4), v, 9);
    return o;
}

/* 0x004a64c4: LVM_INSERTITEMA with a LVITEM: mask 0x0d (text, param, state), iItem = item,
 * iSubItem 0, state 0, stateMask 0, pszText = text, cchTextMax = lstrlenA(text), lParam = param
 * (iImage left as the stack had it). */
uint32_t en4_listview_insert(En2Env* e, uint32_t o, uint32_t text, uint32_t item, uint32_t param)
{
    uint32_t lv = e->scratch;
    en2_w32(e, lv + 4, item);
    en2_w32(e, lv + 0x14, text);
    en2_w32(e, lv + 0, 0x0d);
    en2_w32(e, lv + 8, 0);
    en2_w32(e, lv + 0x18, en2_call1(e, EN4_lstrlenA, text));
    en2_w32(e, lv + 0x0c, 0);
    en2_w32(e, lv + 0x10, 0);
    en2_w32(e, lv + 0x20, param);
    return en2_call4(e, EN2_SendMessageA, en2_r32(e, o), 0x1007, 0, lv);
}

/* 0x004a6ac8: a top window object: vtable +0x34 = 0x004e602c, +0 = 0, +0x08 = the window
 * procedure 0x004a6a60, +0x0c = +0x10 = 0, widget_set_nav_links(o, 0, 0, 0; 0, 0, 0), then
 * 0x00bdfaec = 0; returns o. */
uint32_t en4_window_obj_init(En2Env* e, uint32_t o)
{
    en2_w32(e, o + 0x34, 0x004e602cu);
    en2_w32(e, o, 0);
    en2_w32(e, o + 8, 0x004a6a60u);
    en2_w32(e, o + 0x0c, 0);
    en2_w32(e, o + 0x10, 0);
    uint32_t v[7] = {o, 0, 0, 0, 0, 0, 0};
    e->call(e->user, 0x004a70acu, v, 7);
    en2_w32(e, 0x00bdfaecu, 0);
    return o;
}

/* 0x004a6b80 (win32_register_class): WNDCLASSA {style, DefWindowProcA (the IAT slot's value),
 * 0, 0, hinst, LoadIconA(hinst, "GAME_ICON"), LoadCursorA(0, IDC_ARROW), brush, menu 0, name};
 * returns RegisterClassA's atom (low 16 bits). */
uint32_t en4_register_class(En2Env* e, uint32_t name, uint32_t hinst, uint32_t style, uint32_t brush)
{
    uint32_t w = e->scratch;
    en2_w32(e, w + 0, style);
    en2_w32(e, w + 0x10, hinst);
    en2_w32(e, w + 4, en2_r32(e, EN4_DefWindowProcA));
    en2_w32(e, w + 8, 0);
    en2_w32(e, w + 0x0c, 0);
    en2_w32(e, w + 0x14, en2_call2(e, EN4_LoadIconA, hinst, 0x004e31a8u /* "GAME_ICON" */));
    en2_w32(e, w + 0x18, en2_call2(e, EN4_LoadCursorA, 0, 0x7f00));
    en2_w32(e, w + 0x1c, brush);
    en2_w32(e, w + 0x20, 0);
    en2_w32(e, w + 0x24, name);
    return en2_call1(e, EN4_RegisterClassA, w) & 0xffff;
}

/* 0x004a6dd8: enable (on != 0: clear WS_DISABLED) or disable a window through its GWL_STYLE. */
uint32_t en4_window_enable(En2Env* e, uint32_t o, uint32_t on)
{
    uint32_t s = en2_call2(e, EN2_GetWindowLongA, en2_r32(e, o), (uint32_t)-16);
    s = on ? (s & 0xf7ffffffu) : (s | 0x08000000u);
    return en2_call3(e, EN4_SetWindowLongA, en2_r32(e, o), (uint32_t)-16, s);
}

/* 0x004a8f48 (widget_mgr_alloc: operator new): n = n ? n : 1; p = malloc 0x004bf680(n) until it
 * succeeds: on failure the handler 0x00c8b9dc (if set) is called with n and a nonzero answer
 * retries; else the handler 0x00c8b9d8 (if set) is called and it retries; with neither, 0. */
uint32_t en4_mem_alloc(En2Env* e, uint32_t n)
{
    if (n == 0) n = 1;
    for (;;) {
        uint32_t p = en2_call1(e, 0x004bf680u, n);
        if (p != 0) return p;
        uint32_t h = en2_r32(e, 0x00c8b9dcu);
        if (h != 0 && en2_call1(e, h, n) != 0) continue;
        uint32_t h2 = en2_r32(e, 0x00c8b9d8u);
        if (h2 == 0) return 0;
        en2_call0(e, h2);
    }
}

/* 0x004aaa88 / 0x004aaacc: a message record on the stack (0x84 bytes) set up by 0x004aab90; when
 * the session is open (+0x24) the DirectPlay object +0x28 method +0x28 (stdcall this, flag) is
 * called and a failure code is named into the record (0x004aac00); then 0x004aabc4(record, 0)
 * (empty in this build). */
uint32_t en4_dplay_report(En2Env* e, uint32_t o, uint32_t flag)
{
    uint32_t rec = e->scratch;
    en2_call1(e, 0x004aab90u, rec);
    if (en2_r32(e, o + 0x24) != 0) {
        uint32_t dp = en2_r32(e, o + 0x28);
        uint32_t r = en2_call2(e, en2_r32(e, en2_r32(e, dp) + 0x28), dp, flag);
        if (r != 0) en2_call2(e, 0x004aac00u, rec, r);
    }
    return en2_call2(e, 0x004aabc4u, rec, 0);
}

/* 0x004ac044: strcpy(o + 0xdc, s); returns o with its low byte cleared (the copy loop's al). */
uint32_t en4_strcpy_dc(En2Env* e, uint32_t o, uint32_t s)
{
    uint32_t d = o + 0xdc;
    uint8_t c;
    do {
        c = en2_r8(e, s++);
        en2_w8(e, d++, c);
    } while (c != 0);
    return o & ~0xffu;
}

/* 0x004ace8c: for i = 1 .. +0x10 (signed): p = 0x004acce0(o, i); if p and p +0x8c == 1:
 * 0x004a53c4(x, p + 4, p). Returns +0x10 (read again at the end). */
uint32_t en4_slots_each_active(En2Env* e, uint32_t o, uint32_t x)
{
    for (int32_t i = 1; i <= (int32_t)en2_r32(e, o + 0x10); i++) {
        uint32_t p = en2_call2(e, 0x004acce0u, o, (uint32_t)i);
        if (p != 0 && en2_r32(e, p + 0x8c) == 1) en2_call3(e, 0x004a53c4u, x, p + 4, p);
    }
    return en2_r32(e, o + 0x10);
}
