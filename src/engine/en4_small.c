/*
 * en4_small.c: small engine / Win32 helpers of TM2.EXE 0x00490000..0x004b0000 (see en4_small.h).
 * Verified under emulation: tools/emu/check_en4_small.sh.
 */
#include "en4_small.h"

#define EN4_SetFocus         0x00c90560u
#define EN4_GetPropA         0x00c904e0u
#define EN4_ReadFile         0x00c902f8u
#define EN4_ST_OBJECT        0x004e3194u   /* "ST Object" in the EXE (window property name) */

/* 0x00492e50 (PS1 csSetDrawEnable): cs +0x08 = the low byte of v. */
uint32_t en4_cs_set_draw_enable(En2Env* e, uint32_t cs, uint32_t v)
{
    en2_w32(e, cs + 8, v & 0xff);
    return v & 0xff;
}

/* 0x00493354: the input device flags 0x00a03a70 from the three "present" flags.
 * bit 0 always set (and 0x00a03a7c set to 1 if it was 0); bit 1 kept if 0x00a03a80 and it was
 * set, else cleared; bit 2 the same with 0x00a03a84. The state bytes 0x00a03a78..7a get
 * 1, then 1 / 0 / 2 (kept / cleared / device absent) for the other two. */
uint32_t en4_input_flags_update(En2Env* e)
{
    uint32_t f = en2_r32(e, 0x00a03a70u) | 1;
    en2_w32(e, 0x00a03a70u, f);
    if (en2_r32(e, 0x00a03a7cu) == 0) en2_w32(e, 0x00a03a7cu, 1);
    uint8_t s1, s2;
    f = en2_r32(e, 0x00a03a70u);
    if (en2_r32(e, 0x00a03a80u) != 0) {
        if (f & 2) s1 = 1; else s1 = 0;      /* (kept as is either way) */
    } else {
        s1 = 2;
        f &= ~2u;
    }
    en2_w32(e, 0x00a03a70u, f);
    uint32_t r = f & ~4u;
    if (en2_r32(e, 0x00a03a84u) != 0) {
        s2 = (f & 4) ? 1 : 0;
    } else {
        s2 = 2;
        f &= ~4u;
    }
    en2_w32(e, 0x00a03a70u, f);
    en2_w8(e, 0x00a03a78u, 1);
    en2_w8(e, 0x00a03a79u, s1);
    en2_w8(e, 0x00a03a7au, s2);
    return r;
}
/* 0x0049334c: stores the flags and falls into 0x00493354. */
uint32_t en4_input_flags_set(En2Env* e, uint32_t v)
{
    en2_w32(e, 0x00a03a70u, v);
    return en4_input_flags_update(e);
}

/* 0x0049364c: clears every 8th byte of 0x00a03670..0x00a03a68 inclusive (the key slot state
 * bytes; the store comes before the end test, so 0x00a03a68 is cleared too), then
 * resets the input subsystems 0x0049ea8c (through its thunk 0x004a2058), 0x004a22cc, 0x004a2810
 * and 0x0049eab8. */
uint32_t en4_input_reset_all(En2Env* e)
{
    uint32_t a = 0;
    do { a += 8; en2_w8(e, 0x00a03668u + a, 0); } while (a != 0x400);
    en2_call0(e, 0x004a2058u);
    en2_call0(e, 0x004a22ccu);
    en2_call0(e, 0x004a2810u);
    return en2_call0(e, 0x0049eab8u);
}

/* 0x004952f4 (PS1 menuCopyChoiceInit pair; Ghidra: model_set_position): for i <= 4 (signed; no
 * lower bound, as in the original) the record i of 27 words at 0x00a041c0 gets x, y, z sign
 * extended from 16 bits in its first three words. */
void en4_model_set_position(En2Env* e, int32_t i, uint32_t x, uint32_t y, uint32_t z)
{
    if (i > 4) return;
    uint32_t a = 0x00a041c0u + (uint32_t)i * 27u * 4u;
    en2_w32(e, a, (uint32_t)(int32_t)(int16_t)x);
    en2_w32(e, a + 4, (uint32_t)(int32_t)(int16_t)y);
    en2_w32(e, a + 8, (uint32_t)(int32_t)(int16_t)z);
}

/* 0x00495890: calls the display wrapper 0x004b50cc n + 1 times (none for n < 0); returns 0. */
uint32_t en4_display_flip_n(En2Env* e, int32_t n)
{
    for (int32_t k = n; k >= 0; k--) en2_call1(e, 0x004b50ccu, EN2_DISPLAY);
    return 0;
}

/* 0x004961b8: locks the whole offscreen surface: 0x004b4418(display, 0, 0, 0, rect 0). */
uint32_t en4_display_lock_whole(En2Env* e)
{
    return en2_call5(e, 0x004b4418u, EN2_DISPLAY, 0, 0, 0, 0);
}

/* 0x004961d4: unlock: 0x004b44e4(display, 0, 0). */
uint32_t en4_display_unlock_whole(En2Env* e)
{
    return en2_call3(e, 0x004b44e4u, EN2_DISPLAY, 0, 0);
}

/* 0x00498bb8: ReadFile(h, buf, n, &got, 0); returns got (whatever ReadFile left there). */
uint32_t en4_file_read(En2Env* e, uint32_t h, uint32_t buf, uint32_t n)
{
    uint32_t got = e->scratch;
    en2_call5(e, EN4_ReadFile, h, buf, n, got, 0);
    return en2_r32(e, got);
}

/* 0x0049d494: 0x00a3c890 = v, byte 0x00a3cf9c = 0, then csReset(0x00a3cfe0) (tail call). */
uint32_t en4_cs_reset_a3c890(En2Env* e, uint32_t v)
{
    en2_w32(e, 0x00a3c890u, v);
    en2_w8(e, 0x00a3cf9cu, 0);
    return en2_call1(e, 0x00492c60u, 0x00a3cfe0u);
}

/* 0x004a1f6c: byte 0x00a81e4c + 2 * (i & 0xff) = v. */
void en4_pad_byte_set(En2Env* e, uint32_t i, uint32_t v)
{
    en2_w8(e, 0x00a81e4cu + 2u * (i & 0xff), (uint8_t)v);
}

/* 0x004a2054: empty. */
void en4_nop_4a2054(En2Env* e) { (void)e; }

/* 0x004a2060: jmp 0x0049eab8. */
uint32_t en4_thunk_49eab8(En2Env* e) { return en2_call0(e, 0x0049eab8u); }

/* 0x004a2470: tail call of the runtime routine 0x004c4168 with 0x00a82164. */
uint32_t en4_input_get_num_players(En2Env* e) { return en2_call1(e, 0x004c4168u, 0x00a82164u); }

/* 0x004a54cc: SendMessageA([o], LB_GETCURSEL, 0, 0). */
uint32_t en4_list_get_cursel(En2Env* e, uint32_t o)
{
    return en2_call4(e, EN2_SendMessageA, en2_r32(e, o), 0x188, 0, 0);
}

/* 0x004a559c (widget_handle_select): SendMessageA([o], 0x189 = LB_GETTEXT, w, l). */
uint32_t en4_list_sm_189(En2Env* e, uint32_t o, uint32_t w, uint32_t l)
{
    return en2_call4(e, EN2_SendMessageA, en2_r32(e, o), 0x189, w, l);
}

/* 0x004a6564: a LVITEM on the stack with iSubItem (+0x08) = sub and pszText (+0x14) = text (the
 * other fields are whatever the stack held), SendMessageA([o], 0x102e = LVM_GETITEMTEXTA, item,
 * &lvitem). */
uint32_t en4_listview_get_text(En2Env* e, uint32_t o, uint32_t text, uint32_t item, uint32_t sub)
{
    uint32_t lv = e->scratch;
    en2_w32(e, lv + 0x14, text);
    en2_w32(e, lv + 8, sub);
    return en2_call4(e, EN2_SendMessageA, en2_r32(e, o), 0x102e, item, lv);
}

/* 0x004a6f08 (widget_set_label): SendMessageA([o], WM_SETTEXT, 0, text). */
uint32_t en4_widget_set_label(En2Env* e, uint32_t o, uint32_t text)
{
    return en2_call4(e, EN2_SendMessageA, en2_r32(e, o), 0xc, 0, text);
}

/* 0x004a6f80 (widget_set_focus): SetFocus([o]); returns GetPropA(previous focus, "ST Object"), the
 * widget object of the window that had the focus. */
uint32_t en4_widget_set_focus(En2Env* e, uint32_t o)
{
    uint32_t prev = en2_call1(e, EN4_SetFocus, en2_r32(e, o));
    return en2_call2(e, EN4_GetPropA, prev, EN4_ST_OBJECT);
}

/* 0x004a8f24: longjmp(0x00bdfaf0, 2) (the runtime's longjmp at 0x004c4827): leaves the game loop. */
void en4_game_longjmp_quit(En2Env* e)
{
    en2_call2(e, 0x004c4827u, 0x00bdfaf0u, 2);
}

/* 0x004a8f34 (widget_mgr_begin): tail call of 0x004a6dcc with the window object 0x00bdfcd0. */
uint32_t en4_widget_mgr_begin(En2Env* e) { return en2_call1(e, 0x004a6dccu, 0x00bdfcd0u); }

/* 0x004aa39c (and its thunk 0x004aa018): free(p) (runtime 0x004bf76f) when p is not 0; returns 0
 * for p = 0, else free's eax. */
uint32_t en4_mem_free(En2Env* e, uint32_t p)
{
    if (p == 0) return 0;
    return en2_call1(e, 0x004bf76fu, p);
}

uint32_t en4_get_18(En2Env* e, uint32_t o) { return en2_r32(e, o + 0x18); }
uint32_t en4_get_0c(En2Env* e, uint32_t o) { return en2_r32(e, o + 0x0c); }

/* 0x004aaa74: 0x004ac0b4([o + 0x30], v) (sets +0x58 of that object), then o +0x1c = v. */
uint32_t en4_set_1c_via_30(En2Env* e, uint32_t o, uint32_t v)
{
    uint32_t r = en2_call2(e, 0x004ac0b4u, en2_r32(e, o + 0x30), v);
    en2_w32(e, o + 0x1c, v);
    return r;
}

uint32_t en4_addr_5c(En2Env* e, uint32_t o) { (void)e; return o + 0x5c; }
uint32_t en4_addr_dc(En2Env* e, uint32_t o) { (void)e; return o + 0xdc; }
uint32_t en4_get_1dc(En2Env* e, uint32_t o) { return en2_r32(e, o + 0x1dc); }
uint32_t en4_set_58(En2Env* e, uint32_t o, uint32_t v) { en2_w32(e, o + 0x58, v); return o; }
uint32_t en4_get_58(En2Env* e, uint32_t o) { return en2_r32(e, o + 0x58); }

/* 0x004ac494: a thread entry (stdcall, one argument): 0x004ac134 with the argument in eax and
 * twice on the stack (that function pops both). */
uint32_t en4_thread_entry(En2Env* e, uint32_t arg)
{
    return en2_call3(e, 0x004ac134u, arg, arg, arg);
}

/* 0x004ac5bc (reg_read_int): size = 4; RegQueryValueExA([o], name, 0, 0, out, &size). */
uint32_t en4_reg_read_int(En2Env* e, uint32_t o, uint32_t name, uint32_t out)
{
    uint32_t sz = e->scratch;
    en2_w32(e, sz, 4);
    return en2_call6(e, EN2_RegQueryValueExA, en2_r32(e, o), name, 0, 0, out, sz);
}

/* 0x004ac614 (reg_write_int): RegSetValueExA([o], name, 0, REG_DWORD, &v, 4). */
uint32_t en4_reg_write_int(En2Env* e, uint32_t o, uint32_t name, uint32_t v)
{
    uint32_t p = e->scratch;
    en2_w32(e, p, v);
    return en2_call6(e, EN2_RegSetValueExA, en2_r32(e, o), name, 0, 4, p, 4);
}

/* 0x004ad21c: 0x004e861c = v. */
uint32_t en4_set_render_flag_a(En2Env* e, uint32_t v) { en2_w32(e, 0x004e861cu, v); return v; }

/* 0x00492c60 (PS1 csReset): a coordinate system record back to identity: the 0x30 byte matrix at
 * 0x004e82d0 (EXE) copied to +0xb0 and +0x20; zero: +0xe0/e2/e4 (i16), +0xe8..f0, +0x14..1c,
 * +0xd4..dc, +0x44..4c, +0x74..7c, +0xa4..ac, +0x15c, +0x00, +0x158, +0x160, +0x164 (i16);
 * +0x0c = 1, +0x08 = 1, +0x10 = 0x009f7e38 (the current root), +0x04 = 0x00495864(0).
 * Returns that call's eax. */
uint32_t en4_cs_reset(En2Env* e, uint32_t cs)
{
    for (uint32_t k = 0; k < 0x30; k += 4) en2_w32(e, cs + 0xb0 + k, en2_r32(e, 0x004e82d0u + k));
    for (uint32_t k = 0; k < 0x30; k += 4) en2_w32(e, cs + 0x20 + k, en2_r32(e, 0x004e82d0u + k));
    en2_w16(e, cs + 0xe0, 0);
    en2_w16(e, cs + 0xe2, 0);
    en2_w16(e, cs + 0xe4, 0);
    static const uint16_t z[] = {0xe8, 0xec, 0xf0, 0x14, 0x18, 0x1c, 0xd4, 0xd8, 0xdc, 0x44, 0x48, 0x4c,
                                 0x74, 0x78, 0x7c, 0xa4, 0xa8, 0xac, 0x15c, 0x00};
    for (unsigned k = 0; k < sizeof z / sizeof z[0]; k++) en2_w32(e, cs + z[k], 0);
    en2_w32(e, cs + 0x0c, 1);
    en2_w32(e, cs + 0x158, 0);
    en2_w32(e, cs + 0x08, 1);
    en2_w32(e, cs + 0x10, en2_r32(e, 0x009f7e38u));
    en2_w32(e, cs + 0x160, 0);
    uint32_t r = en2_call1(e, 0x00495864u, 0);
    en2_w16(e, cs + 0x164, 0);
    en2_w32(e, cs + 4, r);
    return r;
}

/* 0x0049eab8: clears the key state tables by input mode 0x004e8434: 3 -> 0x0049e638() and both
 * 0x00a5d710 / 0x00a5d610 (0x100 bytes each, memset 0x004bf830), 0 -> 0x00a5d610, 1 -> 0x00a5d710,
 * 2 -> game_loop_run 0x004a8e6c() then 0x00a5d710, others none; then the key queue indexes
 * 0x00a5da14, 0x00a5da1c, 0x00a5da10 = 0. The return is the last call's eax (for other modes the
 * caller's eax: not defined). */
uint32_t en4_input_mode_reset(En2Env* e)
{
    uint32_t m = en2_r32(e, 0x004e8434u), r = 0;
    if (m == 3) {
        en2_call0(e, 0x0049e638u);
        en2_call3(e, 0x004bf830u, 0x00a5d710u, 0, 0x100);
        r = en2_call3(e, 0x004bf830u, 0x00a5d610u, 0, 0x100);
    } else if (m == 0) {
        r = en2_call3(e, 0x004bf830u, 0x00a5d610u, 0, 0x100);
    } else if (m == 1 || m == 2) {
        if (m == 2) en2_call0(e, 0x004a8e6cu);
        r = en2_call3(e, 0x004bf830u, 0x00a5d710u, 0, 0x100);
    }
    en2_w32(e, 0x00a5da14u, 0);
    en2_w32(e, 0x00a5da1cu, 0);
    en2_w32(e, 0x00a5da10u, 0);
    return r;
}

/* 0x004aac00: DirectPlay error name: o +0 = code, then for a known code its name (strings in the
 * EXE) is copied to o + 4 with its terminator; unknown codes get "DPERR_UNKNOWN"; code 0 copies
 * nothing (returns 0). The return is eax: the code with its low byte cleared by the copy loop.
 * The EXE's string for 0x88770082 reads "DPERR_INVALIDOBJEC" (sic). */
uint32_t en4_dperr_name(En2Env* e, uint32_t o, uint32_t code)
{
    static const uint32_t tab[][2] = {
        {0x80004001u, 0x004e3382u}, {0x80004005u, 0x004e347eu}, {0x8007000eu, 0x004e33b4u},
        {0x80070057u, 0x004e3444u}, {0x88770005u, 0x004e3501u}, {0x8877000au, 0x004e352eu},
        {0x88770014u, 0x004e351au}, {0x8877001eu, 0x004e34ecu}, {0x88770028u, 0x004e34cdu},
        {0x8877003cu, 0x004e34b6u}, {0x88770050u, 0x004e349cu}, {0x8877005au, 0x004e348cu},
        {0x88770078u, 0x004e346bu}, {0x88770082u, 0x004e3458u}, {0x88770096u, 0x004e3430u},
        {0x887700a0u, 0x004e3423u}, {0x887700aau, 0x004e3410u}, {0x887700beu, 0x004e33ffu},
        {0x887700c8u, 0x004e33e7u}, {0x887700d2u, 0x004e33d7u}, {0x887700dcu, 0x004e33c6u},
        {0x887700f0u, 0x004e33a6u}, {0x887700fau, 0x004e3394u}, {0x8877010eu, 0x004e34e1u},
        {0x88770118u, 0x004e3371u},
    };
    en2_w32(e, o, code);
    if (code == 0) return 0;
    uint32_t s = 0x004e3541u;   /* "DPERR_UNKNOWN" */
    for (unsigned k = 0; k < sizeof tab / sizeof tab[0]; k++)
        if (tab[k][0] == code) s = tab[k][1];
    uint32_t d = o + 4;
    uint8_t c;
    do {
        c = en2_r8(e, s++);
        en2_w8(e, d++, c);
    } while (c != 0);
    return code & ~0xffu;
}
