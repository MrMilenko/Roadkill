/*
 * en4_obj.c: see en4_obj.h. Verified under emulation: tools/emu/check_en4_obj.sh.
 * COM calls are kept as calls through the object's vtable (stdcall: this first).
 */
#include "en4_obj.h"

#define EN4_DeleteObject      0x00c901dcu
#define EN4_GetSysColor       0x00c904e4u
#define EN4_CreateSolidBrush  0x00c901d4u
#define EN4_SelectObject      0x00c90200u
#define EN4_BitBlt            0x00c901c4u
#define EN4_DestroyAccelTable 0x00c90488u
#define EN4_DestroyWindow     0x00c9048cu
#define EN4_BeginPaint        0x00c90468u
#define EN4_EndPaint          0x00c904a8u

static uint32_t com1(En2Env* e, uint32_t obj, uint32_t off)
{
    return en2_call1(e, en2_r32(e, en2_r32(e, obj) + off), obj);
}

/* 0x004932c4: set an input device's mode (dev 1 / 2 / 4; mode 0 = off, 1 = on). Other modes
 * return 2 at once. dev 1 keeps its state byte 0x00a03a78; dev 2 / 4 set or clear bit 1 / 2 of
 * the flags 0x00a03a70 and store the mode in 0x00a03a79 / 0x00a03a7a; other devices answer 2.
 * Each then reapplies the flags (0x00493354) and returns the state (low byte of eax; the rest
 * is the flags call's eax). */
uint32_t en4_input_set_device(En2Env* e, uint32_t dev, uint32_t mode)
{
    uint8_t m = (uint8_t)mode;
    if (m != 1 && m != 0) return (dev & ~0xffu) | 2;
    if (dev == 1) {
        m = en2_r8(e, 0x00a03a78u);
    } else if (dev == 2 || dev == 4) {
        uint8_t bit = dev == 2 ? 2 : 4;
        if (m == 1) en2_w8(e, 0x00a03a70u, en2_r8(e, 0x00a03a70u) | bit);
        if (m == 0) en2_w8(e, 0x00a03a70u, en2_r8(e, 0x00a03a70u) & (uint8_t)~bit);
        en2_w8(e, dev == 2 ? 0x00a03a79u : 0x00a03a7au, m);
    } else {
        m = 2;
    }
    return (en2_call0(e, 0x00493354u) & ~0xffu) | m;
}

/* 0x0049628c: asks the display (0x004b5a20) for its red / green / blue masks: 0x7c00 / 0x3e0 /
 * 0x1f -> 0 (555), 0xf800 / 0x7e0 / 0x1f -> 1 (565), else 2; stored in byte 0x00a2dba5 and
 * returned in the low byte. */
uint32_t en4_display_pixel_format(En2Env* e)
{
    uint32_t l = e->scratch;
    uint32_t v[5] = {EN2_DISPLAY, l, l + 4, l + 8, l + 12};
    uint32_t r = e->call(e->user, 0x004b5a20u, v, 5);
    uint32_t rm = en2_r32(e, l), gm = en2_r32(e, l + 4), bm = en2_r32(e, l + 8);
    uint8_t f;
    if (rm == 0x7c00 && gm == 0x3e0 && bm == 0x1f) f = 0;
    else if (rm == 0xf800 && gm == 0x7e0 && bm == 0x1f) f = 1;
    else f = 2;
    en2_w8(e, 0x00a2dba5u, f);
    return (r & ~0xffu) | f;
}

/* 0x00496718 (subsys_texture_init): links the n slots of the table [o] into a free list (slot i
 * holds the address of slot i - 1, slot 0 holds 0) and fills o + 0x4004 + 4 i with the address of
 * slot i; then 0x00a2dbb0 = o. [o] is read again for every store, as in the original. */
void en4_slot_list_init(En2Env* e, uint32_t o, int32_t n)
{
    for (int32_t i = n - 1; i > 0; i--) {
        en2_w32(e, en2_r32(e, o) + 4u * (uint32_t)i, en2_r32(e, o) + 4u * (uint32_t)(i - 1));
        en2_w32(e, o + 0x4004u + 4u * (uint32_t)i, en2_r32(e, o) + 4u * (uint32_t)i);
    }
    en2_w32(e, en2_r32(e, o), 0);
    en2_w32(e, o + 0x4004u, en2_r32(e, o));
    en2_w32(e, 0x00a2dbb0u, o);
}

/* 0x00498194: the keyed list [o] (next at +0x134, key at +0, value at +8): the node with key gets
 * the value; without one a node is added by 0x00497a68(o) and gets key and value. Returns the
 * node (0 if the add failed). */
uint32_t en4_keyed_list_set(En2Env* e, uint32_t o, uint32_t key, uint32_t val)
{
    uint32_t p = en2_r32(e, o);
    while (p != 0) {
        if (en2_r32(e, p) == key) {
            en2_w32(e, p + 8, val);
            return p;
        }
        p = en2_r32(e, p + 0x134);
    }
    p = en2_call1(e, 0x00497a68u, o);
    if (p != 0) {
        en2_w32(e, p + 8, val);
        en2_w32(e, p, key);
    }
    return p;
}

/* 0x0049e44c (subsys_palette_init; it releases DirectInput): if acquired (0x00a5d5f0) the device
 * 0x00a5d378 is unacquired (method +0x20) and the flag cleared; the device and then the
 * DirectInput object 0x00a5d374 are released (+0x08) and cleared. Return not defined. */
uint32_t en4_dinput_release(En2Env* e)
{
    uint32_t r = 0;
    if (en2_r32(e, 0x00a5d5f0u) != 0) {
        uint32_t d = en2_r32(e, 0x00a5d378u);
        if (d != 0) com1(e, d, 0x20);
        en2_w32(e, 0x00a5d5f0u, 0);
    }
    uint32_t d = en2_r32(e, 0x00a5d378u);
    if (d != 0) {
        com1(e, d, 0x08);
        en2_w32(e, 0x00a5d378u, 0);
    }
    uint32_t di = en2_r32(e, 0x00a5d374u);
    if (di != 0) {
        com1(e, di, 0x08);
        en2_w32(e, 0x00a5d374u, 0);
    }
    return r;
}

/* 0x004a3ff4: the widget destructor. flags bit 2: array delete (0x004a9ff8(o, 0x004a3ff4's
 * table 0x004e595c), then free (0x004aa018) of what it returns), returns o. Else: vtable +0x34 =
 * 0x004e5f54; +0 = 0 if +0x4c (a borrowed window); the GDI object +0x44 deleted; window destroy
 * 0x004a6c98(o, 1); base destructor 0x004a6b10(o, 1); with bit 1 the memory is freed
 * (0x004aa39c). Returns the base destructor's answer. */
uint32_t en4_widget_destroy(En2Env* e, uint32_t o, uint32_t flags)
{
    if (flags & 4) {
        uint32_t r = en2_call2(e, 0x004a9ff8u, o, 0x004e595cu);
        en2_call1(e, 0x004aa018u, r);
        return o;
    }
    en2_w32(e, o + 0x34, 0x004e5f54u);
    if (en2_r32(e, o + 0x4c) != 0) en2_w32(e, o, 0);
    uint32_t g = en2_r32(e, o + 0x44);
    if (g != 0) en2_call1(e, EN4_DeleteObject, g);
    en2_call2(e, 0x004a6c98u, o, 1);
    uint32_t r = en2_call2(e, 0x004a6b10u, o, 1);
    if (flags & 2) en2_call1(e, 0x004aa39cu, r);
    return r;
}

/* 0x004a6b10: the window object destructor. flags bit 2: array delete as above (table
 * 0x004e6018). Else vtable +0x34 = 0x004e602c, the accelerator table +0x0c destroyed and
 * cleared, the window +0 destroyed (not cleared); bit 1 frees o. Returns o. */
uint32_t en4_window_obj_destroy(En2Env* e, uint32_t o, uint32_t flags)
{
    if (flags & 4) {
        uint32_t r = en2_call2(e, 0x004a9ff8u, o, 0x004e6018u);
        en2_call1(e, 0x004aa018u, r);
        return o;
    }
    en2_w32(e, o + 0x34, 0x004e602cu);
    uint32_t a = en2_r32(e, o + 0x0c);
    if (a != 0) {
        en2_call1(e, EN4_DestroyAccelTable, a);
        en2_w32(e, o + 0x0c, 0);
    }
    uint32_t w = en2_r32(e, o);
    if (w != 0) en2_call1(e, EN4_DestroyWindow, w);
    if (flags & 2) en2_call1(e, 0x004aa39cu, o);
    return o;
}

/* 0x004a4204: fills the RECT at rect on hdc with the button face color (GetSysColor(15),
 * a solid brush, BitBlt PATCOPY), then restores the DC's brush and deletes its own. */
uint32_t en4_fill_rect_btnface(En2Env* e, uint32_t eax, uint32_t hdc, uint32_t rect)
{
    (void)eax;
    uint32_t br = en2_call1(e, EN4_CreateSolidBrush, en2_call1(e, EN4_GetSysColor, 15));
    uint32_t old = en2_call2(e, EN4_SelectObject, hdc, br);
    uint32_t l = en2_r32(e, rect), t = en2_r32(e, rect + 4);
    uint32_t w = en2_r32(e, rect + 8) - l, h = en2_r32(e, rect + 12) - t;
    uint32_t v[9] = {hdc, l, t, w, h, 0, 0, 0, 0x00a000c9u};
    e->call(e->user, EN4_BitBlt, v, 9);
    en2_call2(e, EN4_SelectObject, hdc, old);
    return en2_call1(e, EN4_DeleteObject, br);
}

/* The widget's action: a member function pointer {+0x5c resolver, +0x60 this adjust} on the
 * target +0x54: g = resolver(target + adjust) (eax = this, edx = o + 0x5c, ebx = this, ecx = o),
 * then g(this, o) (ebx = this, ecx = o). */
static void member_call(En2Env* e, uint32_t o)
{
    uint32_t t = en2_r32(e, o + 0x54) + en2_r32(e, o + 0x60);
    uint32_t g = en2_call4(e, en2_r32(e, o + 0x5c), t, o + 0x5c, t, o);
    en2_call4(e, g, t, o, t, o);
}

/* 0x004a4484: a widget's activate: only for edx = 0; the member action (+0x54 and +0x5c set) or
 * else the plain callback +0x58 (eax = o, edx = 0, ebx as passed, ecx = o); 1 if one ran, else 0. */
uint32_t en4_widget_activate(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx)
{
    if (edx != 0) return 0;
    if (en2_r32(e, o + 0x54) != 0 && en2_r32(e, o + 0x5c) != 0) {
        member_call(e, o);
        return 1;
    }
    uint32_t cb = en2_r32(e, o + 0x58);
    if (cb == 0) return 0;
    en2_call4(e, cb, o, 0, ebx, o);
    return 1;
}

/* The key binding setters: up to three keys (low 16 bits; 0 = none) each bound by 0x004aa098
 * (binding, 0, key, ecx = the third key); a failed binding sets a bit in byte bits. Return 0. */
static uint32_t set_keys(En2Env* e, uint32_t o, uint32_t k1, uint32_t k2, uint32_t k3, uint32_t b1, uint32_t b2,
                         uint32_t b3, uint32_t byte, uint8_t m1, uint8_t m2, uint8_t m3)
{
    k1 &= 0xffff; k2 &= 0xffff; k3 &= 0xffff;
    if (k1 != 0 && en2_call4(e, 0x004aa098u, o + b1, 0, k1, k3) == 0) en2_w8(e, o + byte, en2_r8(e, o + byte) | m1);
    if (k2 != 0 && en2_call4(e, 0x004aa098u, o + b2, 0, k2, k3) == 0) en2_w8(e, o + byte, en2_r8(e, o + byte) | m2);
    if (k3 != 0 && en2_call4(e, 0x004aa098u, o + b3, 0, k3, k3) == 0) en2_w8(e, o + byte, en2_r8(e, o + byte) | m3);
    return 0;
}
uint32_t en4_widget_set_keys(En2Env* e, uint32_t o, uint32_t k1, uint32_t k2, uint32_t k3)
{ return set_keys(e, o, k1, k2, k3, 0xdc, 0xf0, 0x104, 0xbc, 1, 2, 4); }
uint32_t en4_widget_set_keys_alt(En2Env* e, uint32_t o, uint32_t k1, uint32_t k2, uint32_t k3)
{ return set_keys(e, o, k1, k2, k3, 0x118, 0x12c, 0x140, 0xbc, 8, 0x10, 0x20); }
uint32_t en4_widget_set_keys_3(En2Env* e, uint32_t o, uint32_t k1, uint32_t k2, uint32_t k3)
{ return set_keys(e, o, k1, k2, k3, 0x190, 0x1a4, 0x1b8, 0xbd, 4, 2, 8); }

/* 0x004a5344: a list box child: 0x004a4068(o, "ListBox", edx, ebx; "" , style | 0x40100001
 * (WS_CHILD | WS_HSCROLL | LBS_NOTIFY), s0, s1, s2, s3). ORIGINAL BUG: it tests style | 0x200
 * (always nonzero) to choose WS_HSCROLL over WS_VSCROLL, so 0x004a536d (or 0x200000) is dead. */
uint32_t en4_create_listbox(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t style,
                            uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    uint32_t v[10] = {o, 0x004e30feu, edx, ebx, 0x004e30fdu, style | 0x00100000u | 0x40000001u, s0, s1, s2, s3};
    return e->call(e->user, 0x004a4068u, v, 10);
}

/* 0x004a6020: a button's key handler: Enter runs the member action (+0x54 / +0x5c) or the callback
 * +0x58 (eax = o, edx = 0x0d, ebx = +0x54, ecx = o) and returns 0; other keys, or Enter with
 * neither, go to the default handler 0x004a798c(o, key, ebx, o). */
uint32_t en4_button_key_activate(En2Env* e, uint32_t o, uint32_t key, uint32_t ebx)
{
    if (key == 0x0d) {
        uint32_t t = en2_r32(e, o + 0x54);
        if (t != 0 && en2_r32(e, o + 0x5c) != 0) {
            member_call(e, o);
            return 0;
        }
        uint32_t cb = en2_r32(e, o + 0x58);
        if (cb != 0) {
            en2_call4(e, cb, o, 0x0d, t, o);
            return 0;
        }
    }
    return en2_call4(e, 0x004a798cu, o, key, ebx, o);
}

/* 0x004a7c94: a DirectInput device record: unacquire (+0x20) if acquired (+0x2c4, cleared), release
 * the device +0 and the object +0x2d8 (+0x08) and clear them. Return not defined. */
uint32_t en4_dinput_device_release(En2Env* e, uint32_t o)
{
    if (en2_r32(e, o + 0x2c4) != 0) {
        uint32_t d = en2_r32(e, o);
        en2_w32(e, o + 0x2c4, 0);
        if (d != 0) com1(e, d, 0x20);
    }
    uint32_t d = en2_r32(e, o);
    if (d != 0) {
        com1(e, d, 0x08);
        en2_w32(e, o, 0);
    }
    uint32_t x = en2_r32(e, o + 0x2d8);
    if (x != 0) {
        com1(e, x, 0x08);
        en2_w32(e, o + 0x2d8, 0);
    }
    return 0;
}

/* 0x004a8018: polls a DirectInput device (+0x2c8 = present): Poll (+0x64), GetDeviceState (+0x24,
 * 0x50 bytes into +0x274); DIERR_INPUTLOST (0x8007001e) -> +0x2c4 = 0, Acquire (+0x1c), +0x2c4 = 1
 * when that works; any other failure (or a failed Acquire) -> 0x004937b8(). */
uint32_t en4_dinput_device_poll(En2Env* e, uint32_t o)
{
    if (en2_r32(e, o + 0x2c8) == 0) return o;
    com1(e, en2_r32(e, o), 0x64);
    uint32_t d = en2_r32(e, o);
    uint32_t r = en2_call3(e, en2_r32(e, en2_r32(e, d) + 0x24), d, 0x50, o + 0x274);
    if (r == 0) return 0;
    if (r == 0x8007001eu) {
        en2_w32(e, o + 0x2c4, 0);
        if (com1(e, en2_r32(e, o), 0x1c) == 0) {
            en2_w32(e, o + 0x2c4, 1);
            return 0;
        }
    }
    return en2_call0(e, 0x004937b8u);
}

/* 0x004aa638: a DirectSound buffer record: with a buffer (+0x24): method +0x24 (buffer, [[+0x34]
 * + 0x30]), Release (+0x10 in this interface's table), +0x10 = 0; then the sample +0x38: freed
 * through 0x00497a28(s, 0) and 0x004aa39c, +0x38 = 0. Return not defined. */
uint32_t en4_dsound_buffer_release(En2Env* e, uint32_t o)
{
    uint32_t b = en2_r32(e, o + 0x24);
    if (b == 0) return 0;
    en2_call2(e, en2_r32(e, en2_r32(e, b) + 0x24), b, en2_r32(e, en2_r32(e, o + 0x34) + 0x30));
    com1(e, en2_r32(e, o + 0x24), 0x10);
    uint32_t s = en2_r32(e, o + 0x38);
    en2_w32(e, o + 0x10, 0);
    if (s != 0) {
        uint32_t r = en2_call2(e, 0x00497a28u, s, 0);
        en2_call1(e, 0x004aa39cu, r);
        en2_w32(e, o + 0x38, 0);
    }
    return 0;
}

/* 0x004ac9c4: WM_PAINT: BeginPaint([o], &ps) (0 -> return 2); a nonempty rcPaint (ps + 8) is
 * redrawn by 0x00495b20(&rcPaint); EndPaint; return 0. */
uint32_t en4_window_paint(En2Env* e, uint32_t o)
{
    uint32_t ps = e->scratch;
    if (en2_call2(e, EN4_BeginPaint, en2_r32(e, o), ps) == 0) return 2;
    if ((int32_t)(en2_r32(e, ps + 0x10) - en2_r32(e, ps + 8)) > 0 &&
        (int32_t)(en2_r32(e, ps + 0x14) - en2_r32(e, ps + 0x0c)) > 0)
        en2_call1(e, 0x00495b20u, ps + 8);
    en2_call2(e, EN4_EndPaint, en2_r32(e, o), ps);
    return 0;
}

/* 0x004acce0: the player node with id (1 .. +0x10) in the list +0 (id +0x90, next +0x94), walking
 * with the cursor +0x08; an id out of range shows error 0x1d, not found error 0x1c
 * (win32_show_error 0x004bc660, cdecl (code, 2)); returns the node or 0. */
uint32_t en4_session_find_player(En2Env* e, uint32_t o, uint32_t id)
{
    if ((int32_t)id < 1 || (int32_t)id > (int32_t)en2_r32(e, o + 0x10)) {
        en2_call2(e, 0x004bc660u, 0x1d, 2);
        return 0;
    }
    uint32_t p = en2_r32(e, o);
    en2_w32(e, o + 8, p);
    while (p != 0) {
        uint32_t c = en2_r32(e, o + 8);
        if (id == en2_r32(e, c + 0x90)) return en2_r32(e, o + 8);
        en2_w32(e, o + 8, en2_r32(e, c + 0x94));
        p = en2_r32(e, o + 8);
    }
    en2_call2(e, 0x004bc660u, 0x1c, 2);
    return 0;
}
