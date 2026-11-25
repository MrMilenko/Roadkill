/*
 * en4_io.c: see en4_io.h. Verified under emulation: tools/emu/check_en4_io.sh.
 */
#include "en4_io.h"

#define EN4_lstrcpyA            0x00c9031cu
#define EN4_CreateFileA         0x00c902a8u
#define EN4_GetFileSize         0x00c902d0u
#define EN4_ReadFile            0x00c902f8u
#define EN4_ShowWindow          0x00c90584u
#define EN4_RemovePropA         0x00c90548u
#define EN4_SetWindowLongA      0x00c90578u
#define EN4_GetParent           0x00c904dcu
#define EN4_PostQuitMessage     0x00c90534u
#define EN4_DestroyWindow       0x00c9048cu
#define EN4_DialogBoxParamA     0x00c90490u
#define EN4_CreateDialogParamA  0x00c9047cu
#define EN4_PostThreadMessageA  0x00c90538u
#define EN4_WaitForSingleObject 0x00c9030cu
#define EN4_TerminateThread     0x00c90308u

static uint32_t com1(En2Env* e, uint32_t obj, uint32_t off)
{
    return en2_call1(e, en2_r32(e, en2_r32(e, obj) + off), obj);
}

/* 0x004936ac (PS1 pair padWaitAnyButtonOrTimeout by the matcher; it is an input query): the
 * byte table 0x00a03670 + 8 slot + k. k = 0: slot 7 = any device (0x004a2380 | 0x004a28a8 |
 * 0x004a2068), other slots = 0x004a2084 | 0x004a244c | 0x004a28ac of (slot & 0xff), stored in
 * the table; k 1..7: the stored byte; other k: 0. The answer is the low byte of eax. */
uint32_t en4_input_check(En2Env* e, uint32_t slot, uint32_t k)
{
    uint32_t a = 0x00a03670u + slot * 8 + k;
    if (k == 0) {
        uint32_t r;
        uint8_t v;
        if (slot == 7) {
            r = en2_call0(e, 0x004a2380u); v = (uint8_t)r;
            r = en2_call0(e, 0x004a28a8u); v |= (uint8_t)r;
            r = en2_call0(e, 0x004a2068u); v |= (uint8_t)r;
            return (r & ~0xffu) | v;
        }
        uint32_t b = slot & 0xff;
        r = en2_call1(e, 0x004a2084u, b); v = (uint8_t)r;
        r = en2_call1(e, 0x004a244cu, b); v |= (uint8_t)r;
        r = en2_call1(e, 0x004a28acu, b); v |= (uint8_t)r;
        en2_w8(e, a, v);
        return (r & ~0xffu) | v;
    }
    if ((int32_t)k >= 8 || (int32_t)k <= 0) return slot & ~0xffu;
    return (slot & ~0xffu) | en2_r8(e, a);
}

/* 0x0049456c (level_textures_setup; it selects view i): 0x00a04138 = i; the view record
 * 0x00a03b78 + 0x94 i: viewport_set_size(+0, +4), viewport_set_bounds(+8 / 2, +0x0c / 2) (signed
 * halves), 0x004ad5e0(i); the light matrix 0x004e83bc (EXE data) gets +0x30 at +0, -(+0x38) at
 * +0x14, +0x34 at +0x1c and is copied (0x30 bytes) to 0x00a03a98. Returns +0x34. */
uint32_t en4_view_select(En2Env* e, uint32_t i)
{
    en2_w32(e, 0x00a04138u, i);
    uint32_t r = 0x00a03b78u + i * 0x94u;
    en2_call2(e, 0x0048bbccu, en2_r32(e, r), en2_r32(e, r + 4));
    int32_t hy = (int32_t)en2_r32(e, r + 0x0c) / 2, hx = (int32_t)en2_r32(e, r + 8) / 2;
    en2_call2(e, 0x0048bb10u, (uint32_t)hx, (uint32_t)hy);
    en2_call1(e, 0x004ad5e0u, i);
    uint32_t x = en2_r32(e, r + 0x34);   /* eax at the end: the last value loaded */
    en2_w32(e, 0x004e83bcu, en2_r32(e, r + 0x30));
    en2_w32(e, 0x004e83d0u, en2_r32(e, r + 0x38));
    en2_w32(e, 0x004e83d0u, 0u - en2_r32(e, r + 0x38));
    en2_w32(e, 0x004e83d8u, en2_r32(e, r + 0x34));
    for (uint32_t k = 0; k < 0x30; k += 4) en2_w32(e, 0x00a03a98u + k, en2_r32(e, 0x004e83bcu + k));
    return x;
}

/* 0x00495924 (subsys_sintab_font_init): loads "sintab.dat" into 0x00a04678 (file_build_path
 * 0x00499408, file_load_resource 0x00498d18), runs 0x004ad360, reads the display's pixel masks
 * (0x004b5a20) into the format byte 0x00a2dba5 (0 = 555, 1 = 565, 2 = other), then allocates 0x800
 * bytes (0x004bf680) for the font 0x00a05678 and loads "VGA8X8.FNT" into it. */
uint32_t en4_tables_load(En2Env* e)
{
    uint32_t path1 = e->scratch, path2 = e->scratch + 0x130, m = e->scratch + 0x260;
    en2_call3(e, 0x00499408u, path1, 0x004e2ec0u, 0);
    en2_call2(e, 0x00498d18u, 0x00a04678u, path1);
    en2_call0(e, 0x004ad360u);
    /* red -> m + 4, green -> m + 0, blue -> m + 12, the fourth -> m + 8 */
    uint32_t v[5] = {EN2_DISPLAY, m + 4, m, m + 12, m + 8};
    e->call(e->user, 0x004b5a20u, v, 5);
    uint32_t rm = en2_r32(e, m + 4), gm = en2_r32(e, m), bm = en2_r32(e, m + 12);
    uint8_t f;
    if (rm == 0x7c00 && gm == 0x3e0 && bm == 0x1f) f = 0;
    else if (rm == 0xf800 && gm == 0x7e0 && bm == 0x1f) f = 1;
    else f = 2;
    en2_w8(e, 0x00a2dba5u, f);
    en2_call3(e, 0x00499408u, path2, 0x004e2ecbu, 0);
    uint32_t p = en2_call2(e, 0x004bf680u, 0x800, path2);
    en2_w32(e, 0x00a05678u, p);
    return en2_call2(e, 0x00498d18u, p, path2);
}

/* 0x00496774: the slot table setup of 0x00496718 linked the other way (when the screen is not
 * split; split screen uses 0x00496718): slot k of [o] holds the address of slot k + 1, the last
 * slot 0, and o + 0x4000 + 4 (k + 1) the address of slot k; 0x00a2dbb0 = o. Returns [o] (split:
 * 0x00496718's eax). */
uint32_t en4_slot_list_init_fwd(En2Env* e, uint32_t o, int32_t n)
{
    if ((en2_call0(e, 0x004958acu) & 0xff) != 0) return en2_call2(e, 0x00496718u, o, (uint32_t)n);
    for (int32_t k = 0; k < n - 1; k++) {
        en2_w32(e, en2_r32(e, o) + 4u * (uint32_t)k, en2_r32(e, o) + 4u * (uint32_t)k + 4);
        en2_w32(e, o + 0x4000u + 4u * (uint32_t)(k + 1), en2_r32(e, o) + 4u * (uint32_t)k);
    }
    uint32_t t = en2_r32(e, o);
    en2_w32(e, t + 4u * (uint32_t)n - 4, 0);
    en2_w32(e, 0x00a2dbb0u, o);
    return t;
}

static uint32_t list_find(En2Env* e, uint32_t o, uint32_t key)
{
    uint32_t p = en2_r32(e, o);
    while (p != 0 && en2_r32(e, p) != key) p = en2_r32(e, p + 0x134);
    return p;
}

/* 0x004981e0: keyed list node (see 0x00498194) gets kind +0x118 (new nodes also the key); kinds 2
 * and above 4 also set +0x08 = 1. Returns the node or 0. */
uint32_t en4_keyed_list_set_kind(En2Env* e, uint32_t o, uint32_t key, uint32_t kind)
{
    uint32_t p = list_find(e, o, key);
    if (p == 0) {
        p = en2_call1(e, 0x00497a68u, o);
        if (p == 0) return 0;
        en2_w32(e, p + 0x118, kind);
        en2_w32(e, p, key);
    } else {
        en2_w32(e, p + 0x118, kind);
    }
    if (kind == 2 || (int32_t)kind > 4) en2_w32(e, p + 8, 1);
    return p;
}

/* 0x00498250: keyed list node gets v at +0x0c (new nodes also the key); v = 1 -> +0x118 = 4,
 * +0x08 = 1; else +0x08 = 0, +0x118 = 1. Returns the node or 0. */
uint32_t en4_keyed_list_set_mode(En2Env* e, uint32_t o, uint32_t key, uint32_t v)
{
    uint32_t p = list_find(e, o, key);
    if (p == 0) {
        p = en2_call1(e, 0x00497a68u, o);
        if (p == 0) return 0;
        en2_w32(e, p + 0x0c, v);
        en2_w32(e, p, key);
    } else {
        en2_w32(e, p + 0x0c, v);
    }
    if (v == 1) {
        en2_w32(e, p + 0x118, 4);
        en2_w32(e, p + 8, v);
    } else {
        en2_w32(e, p + 8, 0);
        en2_w32(e, p + 0x118, 1);
    }
    return p;
}

/* 0x00498380: resets the records [o + 4 + 4 i] for i = start .. 7 (signed): +0 = -1, +4 = +8 = 0,
 * the strings +0x14 and +0x94 = "" (lstrcpyA from the EXE's empty strings), +0x114 = -1,
 * +0x118 = 0, +0x11c .. +0x12c = -1; then o +0x28 = 0, +0x2c = 1, +0x24 = 0. */
void en4_records_reset(En2Env* e, uint32_t o, int32_t start)
{
    for (int32_t i = start; i < 8; i++) {
        uint32_t s = o + 4 + 4u * (uint32_t)i;
        en2_w32(e, en2_r32(e, s), 0xffffffffu);
        en2_w32(e, en2_r32(e, s) + 4, 0);
        en2_w32(e, en2_r32(e, s) + 8, 0);
        en2_call2(e, EN4_lstrcpyA, en2_r32(e, s) + 0x14, 0x004e2f6cu);
        en2_call2(e, EN4_lstrcpyA, en2_r32(e, s) + 0x94, 0x004e2f6du);
        en2_w32(e, en2_r32(e, s) + 0x114, 0xffffffffu);
        en2_w32(e, en2_r32(e, s) + 0x118, 0);
        for (uint32_t f = 0x11c; f <= 0x12c; f += 4) en2_w32(e, en2_r32(e, s) + f, 0xffffffffu);
    }
    en2_w32(e, o + 0x28, 0);
    en2_w32(e, o + 0x2c, 1);
    en2_w32(e, o + 0x24, 0);
}

/* 0x004988e8: IDirectPlay2::CreatePlayer (method +0x18) (dp, &id = o + 0x30, &DPNAME {0x10, 0,
 * short o + 0x44, long o + 0xc4}, no event, data o + 0x30, 0x138 bytes, 0); a failure is named
 * into a message record (0x004aab90 / 0x004aac00 / 0x004aabc4). Returns the HRESULT. */
uint32_t en4_dplay_create_player(En2Env* e, uint32_t o, uint32_t dp)
{
    uint32_t rec = e->scratch, nm = e->scratch + 0x84;
    en2_call1(e, 0x004aab90u, rec);
    en2_w32(e, nm + 8, o + 0x44);
    en2_w32(e, nm, 0x10);
    en2_w32(e, nm + 4, 0);
    en2_w32(e, nm + 0x0c, o + 0xc4);
    uint32_t v[7] = {dp, o + 0x30, nm, 0, o + 0x30, 0x138, 0};
    uint32_t r = e->call(e->user, en2_r32(e, en2_r32(e, dp) + 0x18), v, 7);
    if (r != 0) en2_call2(e, 0x004aac00u, rec, r);
    en2_call2(e, 0x004aabc4u, rec, 0);
    return r;
}

/* 0x00498d18 (file_load_resource): reads the whole file path into dest (CreateFileA GENERIC_READ,
 * FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN | NORMAL; GetFileSize; ReadFile;
 * CloseHandle); returns the bytes read, 0 when the file does not open. */
uint32_t en4_file_load(En2Env* e, uint32_t dest, uint32_t path)
{
    uint32_t got = e->scratch + 4;
    uint32_t v[7] = {path, 0x80000000u, 1, 0, 3, 0x10000080u, 0};
    uint32_t h = e->call(e->user, EN4_CreateFileA, v, 7);
    if (h == 0xffffffffu) return 0;
    uint32_t size = en2_call2(e, EN4_GetFileSize, h, 0);
    en2_call5(e, EN4_ReadFile, h, dest, size, got, 0);
    uint32_t n = en2_r32(e, got);
    en2_call1(e, EN2_CloseHandle, h);
    return n;
}

/* 0x0049e9d8: snapshot of the key state table into 0x00a5d810 (0x100 bytes) by input mode
 * 0x004e8434: 3 -> 0x0049e638(), 0x0049e4ac(), copy of 0x00a5d610; 0 -> 0x0049e4ac(), copy of
 * 0x00a5d610; 1 -> 0x0049e638(), copy of 0x00a5d710; 2 -> copy of 0x00a5d710; others nothing.
 * DISASSEMBLY TRAP: the copy is f2 a5 (repne movsd, a full rep copy) which capstone prints as a
 * single movsd. Return not defined. */
uint32_t en4_key_state_snapshot(En2Env* e)
{
    uint32_t m = en2_r32(e, 0x004e8434u), src;
    if (m == 3) { en2_call0(e, 0x0049e638u); en2_call0(e, 0x0049e4acu); src = 0x00a5d610u; }
    else if (m == 0) { en2_call0(e, 0x0049e4acu); src = 0x00a5d610u; }
    else if (m == 1) { en2_call0(e, 0x0049e638u); src = 0x00a5d710u; }
    else if (m == 2) src = 0x00a5d710u;
    else return 0;
    for (uint32_t k = 0; k < 0x100; k += 4) en2_w32(e, 0x00a5d810u + k, en2_r32(e, src + k));
    return 0;
}

/* 0x0049eb88: clears key k (low byte) in the state tables of the input mode 0x004e8434 (3: both
 * 0x00a5d710 and 0x00a5d610, 0: 0x00a5d610, 1 / 2: 0x00a5d710) and stores the mode back. Returns
 * k & 0xff (k for other modes). */
uint32_t en4_key_clear(En2Env* e, uint32_t k)
{
    uint32_t m = en2_r32(e, 0x004e8434u);
    if (m > 3) { en2_w32(e, 0x004e8434u, m); return k; }
    k &= 0xff;
    if (m == 3 || m == 1 || m == 2) en2_w8(e, 0x00a5d710u + k, 0);
    if (m == 3 || m == 0) en2_w8(e, 0x00a5d610u + k, 0);
    en2_w32(e, 0x004e8434u, m);
    return k;
}

/* 0x004a2828: polls (0x004a28d0) and returns the highest pressed button number: buttons 1..0x20
 * and 0x2d..0x30 from the byte table 0x00a824fc (nonzero = pressed); 0x21..0x2c too, except that
 * when the device answer 0x004a825c(0x00a82560) is 0 they count only above 0x4b. 0 if none. */
uint32_t en4_pad_last_button(En2Env* e)
{
    uint8_t best = 0;
    en2_call0(e, 0x004a28d0u);
    for (uint32_t b = 1; b <= 0x20; b++) if (en2_r8(e, 0x00a824fcu + b) != 0) best = (uint8_t)b;
    for (uint32_t b = 0x21; b <= 0x2c; b++) {
        if (en2_call1(e, 0x004a825cu, 0x00a82560u) == 0) {
            if (en2_r8(e, 0x00a824fcu + b) > 0x4b) best = (uint8_t)b;
        } else if (en2_r8(e, 0x00a824fcu + b) != 0) {
            best = (uint8_t)b;
        }
    }
    for (uint32_t b = 0x2d; b < 0x31; b++) if (en2_r8(e, 0x00a824fcu + b) != 0) best = (uint8_t)b;
    return best;
}

/* 0x004a42c0 (widget_base_init): widget_alloc 0x004a3fa0(eax, edx), its four components cleared
 * (widget_alloc_component 0x004aa020, each on the previous one's answer + 0x14); the widget is
 * the last answer - 0xa8: vtable +0x34 = 0x004e5e70, +0x54 .. +0x64 = 0, +0x68 = style s0 |
 * WS_CHILD; then vtable +0xc4 (w, edx, ebx, ecx; +0x68, s1, s2, s3, s4). Returns w. */
uint32_t en4_widget_base_init(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                              uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4)
{
    uint32_t c = en2_call2(e, 0x004a3fa0u, eax, edx);
    c = en2_call1(e, 0x004aa020u, c + 0x6c);
    c = en2_call1(e, 0x004aa020u, c + 0x14);
    c = en2_call1(e, 0x004aa020u, c + 0x14);
    c = en2_call1(e, 0x004aa020u, c + 0x14);
    uint32_t w = c - 0xa8;
    en2_w32(e, w + 0x34, 0x004e5e70u);
    en2_w32(e, w + 0x54, 0);
    en2_w32(e, w + 0x58, 0);
    en2_w32(e, w + 0x5c, 0);
    en2_w32(e, w + 0x60, 0);
    en2_w32(e, w + 0x64, 0);
    en2_w32(e, w + 0x68, s0 | 0x40000000u);
    uint32_t v[9] = {w, edx, ebx, ecx, en2_r32(e, w + 0x68), s1, s2, s3, s4};
    e->call(e->user, en2_r32(e, en2_r32(e, w + 0x34) + 0xc4), v, 9);
    return w;
}

/* 0x004a4b50 (widget_set_nav_keys): four key bindings (0x004aa098(binding, 0, key, ecx = k3)):
 * k1 -> +0xdc (fail: +0xbc |= 1), k2 -> +0x154 (+0xbc |= 0x80), k3 -> +0x190 (+0xbd |= 4),
 * k4 (stack) -> +0x118 (+0xbc |= 8). Returns 0; pops one stack word. */
uint32_t en4_widget_set_nav_keys(En2Env* e, uint32_t o, uint32_t k1, uint32_t k2, uint32_t k3, uint32_t k4)
{
    if (k1 != 0 && en2_call4(e, 0x004aa098u, o + 0xdc, 0, k1, k3) == 0) en2_w8(e, o + 0xbc, en2_r8(e, o + 0xbc) | 1);
    if (k2 != 0 && en2_call4(e, 0x004aa098u, o + 0x154, 0, k2, k3) == 0) en2_w8(e, o + 0xbc, en2_r8(e, o + 0xbc) | 0x80);
    if (k3 != 0 && en2_call4(e, 0x004aa098u, o + 0x190, 0, k3, k3) == 0) en2_w8(e, o + 0xbd, en2_r8(e, o + 0xbd) | 4);
    if (k4 != 0 && en2_call4(e, 0x004aa098u, o + 0x118, 0, k4, k3) == 0) en2_w8(e, o + 0xbc, en2_r8(e, o + 0xbc) | 8);
    return 0;
}

/* 0x004a5290 (widget_create_panel): a list panel: widget_alloc(eax, edx), vtable 0x004e5ca8,
 * +0x54 .. +0x78 = 0, then vtable +0xc4 (the list box create, 0x004a5344) (o, edx, ebx, style |
 * 0x40100001; s0..s3). ORIGINAL BUG as in 0x004a5344: (style | 0x200) is tested, so the
 * WS_VSCROLL branch 0x004a531e is dead. Returns o; pops four stack words. */
uint32_t en4_widget_create_panel(En2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t style,
                                 uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    uint32_t o = en2_call2(e, 0x004a3fa0u, eax, edx);
    en2_w32(e, o + 0x34, 0x004e5ca8u);
    static const uint8_t z[] = {0x64, 0x54, 0x58, 0x5c, 0x60, 0x78, 0x68, 0x6c, 0x70, 0x74};
    for (unsigned k = 0; k < sizeof z; k++) en2_w32(e, o + z[k], 0);
    uint32_t vt = en2_r32(e, o + 0x34);
    uint32_t v[8] = {o, edx, ebx, style | 0x00100000u | 0x40000001u, s0, s1, s2, s3};
    e->call(e->user, en2_r32(e, vt + 0xc4), v, 8);
    return o;
}

/* 0x004a6c98 (win32_destroy_window): hides the window, removes its "ST Object" property and (when
 * that worked and a previous procedure +4 is kept) restores GWL_WNDPROC; a top level window
 * (no parent) posts WM_QUIT (PostQuitMessage(0)); DestroyWindow; +0 = 0. */
uint32_t en4_destroy_window(En2Env* e, uint32_t o)
{
    uint32_t w = en2_r32(e, o);
    if (w == 0) return o;
    en2_call2(e, EN4_ShowWindow, w, 0);
    if (en2_call2(e, EN4_RemovePropA, en2_r32(e, o), 0x004e31d0u) != 0) {
        uint32_t prev = en2_r32(e, o + 4);
        if (prev != 0) en2_call3(e, EN4_SetWindowLongA, en2_r32(e, o), (uint32_t)-4, prev);
    }
    if (en2_call1(e, EN4_GetParent, en2_r32(e, o)) == 0) en2_call1(e, EN4_PostQuitMessage, 0);
    uint32_t r = en2_call1(e, EN4_DestroyWindow, en2_r32(e, o));
    en2_w32(e, o, 0);
    return r;
}

/* 0x004a9de0: shows a dialog object: +4 = 0; modal (+0x54) -> DialogBoxParamA(hinst 0x00bdfae8,
 * templ, parent, proc +8, o)'s answer; an open modeless one gets the focus (0x004a6f80) -> 1;
 * else CreateDialogParamA(...) into +0 -> 1 if created. */
uint32_t en4_dialog_show(En2Env* e, uint32_t o, uint32_t parent, uint32_t templ)
{
    en2_w32(e, o + 4, 0);
    if (en2_r32(e, o + 0x54) != 0)
        return en2_call5(e, EN4_DialogBoxParamA, en2_r32(e, 0x00bdfae8u), templ, parent, en2_r32(e, o + 8), o);
    if (en2_r32(e, o) != 0) {
        en2_call1(e, 0x004a6f80u, o);
        return 1;
    }
    uint32_t h = en2_call5(e, EN4_CreateDialogParamA, en2_r32(e, 0x00bdfae8u), templ, parent, en2_r32(e, o + 8), o);
    en2_w32(e, o, h);
    return h != 0;
}

/* 0x004aa5b8: frees a sound: with a buffer (+0x24): method +0x24 (buffer, [[+0x34] + 0x30]),
 * method +0x10, Release; +0x24 = +0x18 = +0x10 = 0; the sample +0x38 freed (0x00497a28(s, 0),
 * 0x004aa39c) and cleared; the second buffer +0x28 released and cleared. Return not defined. */
uint32_t en4_dsound_sound_free(En2Env* e, uint32_t o)
{
    uint32_t b = en2_r32(e, o + 0x24);
    if (b == 0) return 0;
    en2_call2(e, en2_r32(e, en2_r32(e, b) + 0x24), b, en2_r32(e, en2_r32(e, o + 0x34) + 0x30));
    com1(e, en2_r32(e, o + 0x24), 0x10);
    com1(e, en2_r32(e, o + 0x24), 0x08);
    en2_w32(e, o + 0x24, 0);
    en2_w32(e, o + 0x18, 0);
    uint32_t s = en2_r32(e, o + 0x38);
    en2_w32(e, o + 0x10, 0);
    if (s != 0) {
        en2_call1(e, 0x004aa39cu, en2_call2(e, 0x00497a28u, s, 0));
        en2_w32(e, o + 0x38, 0);
    }
    uint32_t d = en2_r32(e, o + 0x28);
    if (d != 0) {
        com1(e, d, 0x08);
        en2_w32(e, o + 0x28, 0);
    }
    return 0;
}

/* 0x004abd38: stops the network thread +0x1f0: WM_QUIT to thread id +0x200, +0x20c = 0, waits 1 s,
 * TerminateThread if it did not end, else CloseHandle; +0x1f0 = 0; then +0 = +0x224 = 0. */
uint32_t en4_thread_stop(En2Env* e, uint32_t o)
{
    if (en2_r32(e, o + 0x1f0) != 0) {
        en2_call4(e, EN4_PostThreadMessageA, en2_r32(e, o + 0x200), 0x12, 0, 0);
        uint32_t t = en2_r32(e, o + 0x1f0);
        en2_w32(e, o + 0x20c, 0);
        if (en2_call2(e, EN4_WaitForSingleObject, t, 1000) != 0) en2_call2(e, EN4_TerminateThread, en2_r32(e, o + 0x1f0), 0);
        else en2_call1(e, EN2_CloseHandle, en2_r32(e, o + 0x1f0));
        en2_w32(e, o + 0x1f0, 0);
    }
    en2_w32(e, o, 0);
    en2_w32(e, o + 0x224, 0);
    return 0;
}
