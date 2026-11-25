/*
 * en7_a.c: see en7_a.h.
 */
#include "en7_a.h"

#define EN7_GetAsyncKeyState   0x00c904b8u
#define EN7_OffsetRect         0x00c9052cu
#define EN7_CreateSolidBrush   0x00c901d4u
#define EN7_FrameRect          0x00c904b4u
#define EN7_DeleteObject       0x00c901dcu
#define EN7_DirectSoundCreate  0x00c907d8u   /* dsound ordinal 1, through the thunk 0x004d1908 */

static uint32_t vfn(En2Env* e, uint32_t o, uint32_t vtoff, uint32_t off) { return en2_r32(e, en2_r32(e, o + vtoff) + off); }

/* 0x004a1f1e: printf into the debug buffer 0x004e8fea: vsprintf (runtime 0x004c3913)(buffer, fmt,
 * &va) where va is the address of the first variadic argument on the caller's stack. */
uint32_t en7_debug_printf(En2Env* e, uint32_t fmt, uint32_t va)
{
    uint32_t p = e->scratch;
    en2_w32(e, p, va);
    return en2_call3(e, 0x004c3913u, 0x004e8feau, fmt, p);
}

/* 0x004a2dcc: runs the registered exit handlers: pops each node of the list 0x004e91e8 (+0 next),
 * pushes a frame {previous frame, node +4, node +8, node +0x0c when [node +4] == 4} on the handler
 * frame chain 0x00c8b9c0, calls the runtime 0x004c44e2 (eax = node +4, or +0x0c for kind 4),
 * clears the node's link and pops the frame (chain = [chain]). */
uint32_t en7_exit_handlers_run(En2Env* e)
{
    const uint32_t F = e->scratch;
    uint32_t r = 0;
    for (;;) {
        uint32_t n = en2_r32(e, 0x004e91e8u);
        if (n == 0) return r;
        en2_w32(e, 0x004e91e8u, en2_r32(e, n));
        en2_w32(e, F, en2_r32(e, 0x00c8b9c0u));
        en2_w32(e, 0x00c8b9c0u, F);
        en2_w32(e, F + 4, en2_r32(e, n + 4));
        en2_w32(e, F + 8, en2_r32(e, n + 8));
        uint32_t a = en2_r32(e, n + 4);
        if (en2_r32(e, a) == 4) {
            a = en2_r32(e, n + 0x0c);
            en2_w32(e, F + 0x0c, a);
        }
        en2_call2(e, 0x004c44e2u, a, n);
        en2_w32(e, n, 0);
        en2_w32(e, 0x00c8b9c0u, en2_r32(e, en2_r32(e, 0x00c8b9c0u)));
        r = en2_r32(e, 0x00c8b9c0u);
    }
}

/* 0x004a20c4: game_loop_run; for a joystick device (input_get_device_type(1) == 1) 0x0049ea60()
 * (its answer goes to a jump table whose four entries are all the next instruction: unused);
 * again for device type 1: the key state snapshot 0x0049e9d8, Print Screen (0x2c) -> key 0x54 and
 * F10 (0x79) -> key 0x44 (0x0049eb3c) when GetAsyncKeyState has bit 15 or 0 set, then 0x00493644.
 * Return not defined. */
uint32_t en7_joystick_poll(En2Env* e)
{
    en2_call0(e, 0x004a8e6cu);
    if ((en2_call1(e, 0x004932a0u, 1) & 0xff) == 1) en2_call0(e, 0x0049ea60u);
    if ((en2_call1(e, 0x004932a0u, 1) & 0xff) != 1) return 0;
    en2_call0(e, 0x0049e9d8u);
    if (en2_call1(e, EN7_GetAsyncKeyState, 0x2c) & 0x8001) en2_call1(e, 0x0049eb3cu, 0x54);
    if (en2_call1(e, EN7_GetAsyncKeyState, 0x79) & 0x8001) en2_call1(e, 0x0049eb3cu, 0x44);
    en2_call0(e, 0x00493644u);
    return 0;
}

/* 0x004a4e00: WM_DRAWITEM of a widget (DRAWITEMSTRUCT dis: +0x10 itemState, +0x18 hDC, +0x1c
 * rcItem): s = vtable +0xcc (o, dis; ebx as passed, ecx = the vtable) (the widget's state bits);
 * vtable +0xe4 (o, hDC, &rcItem, s & 1; itemState & 4, s & 4, s & 8) draws the frame; when the
 * text (0x004a6ef4(o, buffer, 200)) is not empty vtable +0xec (o, hDC, &rcItem, buffer; s & 4,
 * itemState & 4) draws it. Returns 1. */
uint32_t en7_widget_draw_item(En2Env* e, uint32_t o, uint32_t dis, uint32_t ebx)
{
    const uint32_t buf = e->scratch;
    uint32_t rc = dis + 0x1c, hdc = en2_r32(e, dis + 0x18);
    uint32_t vt = en2_r32(e, o + 0x34);
    uint32_t s = en2_call4(e, en2_r32(e, vt + 0xcc), o, dis, ebx, vt);
    uint32_t dis4 = en2_r32(e, dis + 0x10) & 4;
    uint32_t v[7] = {o, hdc, rc, s & 1, dis4, s & 4, s & 8};
    e->call(e->user, vfn(e, o, 0x34, 0xe4), v, 7);
    if ((int32_t)en2_call3(e, 0x004a6ef4u, o, buf, 0xc8) > 0) {
        uint32_t w[6] = {o, hdc, rc, buf, s & 4, dis4};
        e->call(e->user, vfn(e, o, 0x34, 0xec), w, 6);
    }
    return 1;
}

/* 0x004a5060: with on != 0, a two color frame inside the RECT: the inner rect (left + 3, top + 2,
 * right - 4, bottom - 3) framed with the brush of color +0xd0 and the same rect moved by (1, 1)
 * with color +0xcc (CreateSolidBrush, FrameRect, DeleteObject). Returns 0. */
uint32_t en7_draw_focus_frame(En2Env* e, uint32_t o, uint32_t hdc, uint32_t rect, uint32_t on)
{
    if (on == 0) return 0;
    const uint32_t R1 = e->scratch, R2 = e->scratch + 0x10;
    en2_w32(e, R2, en2_r32(e, rect) + 3);
    en2_w32(e, R2 + 4, en2_r32(e, rect + 4) + 2);
    en2_w32(e, R2 + 8, en2_r32(e, rect + 8) - 4);
    en2_w32(e, R2 + 12, en2_r32(e, rect + 12) - 3);
    for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, R1 + k, en2_r32(e, R2 + k));
    en2_call3(e, EN7_OffsetRect, R1, 1, 1);
    uint32_t b1 = en2_call1(e, EN7_CreateSolidBrush, en2_r32(e, o + 0xd0));
    uint32_t b2 = en2_call1(e, EN7_CreateSolidBrush, en2_r32(e, o + 0xcc));
    en2_call3(e, EN7_FrameRect, hdc, R1, b2);
    en2_call3(e, EN7_FrameRect, hdc, R2, b1);
    en2_call1(e, EN7_DeleteObject, b1);
    en2_call1(e, EN7_DeleteObject, b2);
    return 0;
}

/* 0x004aa524: (re)creates DirectSound for the device desc ([desc] = its GUID pointer): the old sound
 * is freed (0x004aa5b8) when +0x24 is set; with desc and devices (+0x20 > 0): the old object +0x28
 * released, DirectSoundCreate([desc], &+0x28, 0); on success its QueryInterface (method 0) for the
 * IID 0x004e51f4 into +0x24 and +0x18 = 1, returns 0; failure or no device -1. Message record as
 * 0x004aab90 / 0x004aabc4. */
uint32_t en7_dsound_create(En2Env* e, uint32_t o, uint32_t desc)
{
    const uint32_t rec = e->scratch;
    en2_call1(e, 0x004aab90u, rec);
    uint32_t r = 0xffffffffu;
    if (en2_r32(e, o + 0x24) != 0) en2_call1(e, 0x004aa5b8u, o);
    if (desc != 0 && (int32_t)en2_r32(e, o + 0x20) > 0) {
        uint32_t d = en2_r32(e, o + 0x28);
        if (d != 0) {
            en2_call1(e, vfn(e, d, 0, 8), d);
            en2_w32(e, o + 0x28, 0);
        }
        if (en2_call3(e, EN7_DirectSoundCreate, en2_r32(e, desc), o + 0x28, 0) == 0) {
            d = en2_r32(e, o + 0x28);
            if (d != 0) en2_call3(e, en2_r32(e, en2_r32(e, d)), d, 0x004e51f4u, o + 0x24);
            en2_w32(e, o + 0x18, 1);
            r = 0;
        }
    }
    en2_call2(e, 0x004aabc4u, rec, 0);
    return r;
}

/* 0x004ab43c: DirectPlay session enumeration: a DPSESSIONDESC2 at +8 (0x7c bytes cleared, dwSize
 * 0x50, the application GUID copied from [+4] to +0x20, the password pointer +0x3c = o + 0xdc),
 * +0x220 = 0; with a DirectPlay object +0, EnumSessions (method +0x34)(dp, +8, 1000 ms, callback
 * 0x004ac2f0, o, flags +0x210): a failure is named (0x004aac00) and passed to the error callback
 * +0x254 (eax), success calls +0x258. Message record as 0x004aab90 / 0x004aabc4. */
uint32_t en7_dplay_enum_sessions(En2Env* e, uint32_t o)
{
    const uint32_t rec = e->scratch;
    en2_call1(e, 0x004aab90u, rec);
    en2_w32(e, o + 0x220, 0);
    en2_call3(e, 0x004bf830u, o + 8, 0, 0x7c);
    uint32_t g = en2_r32(e, o + 4);
    en2_w32(e, o + 8, 0x50);
    for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, o + 0x20 + k, en2_r32(e, g + k));
    uint32_t dp = en2_r32(e, o);
    en2_w32(e, o + 0x3c, o + 0xdc);
    if (dp != 0) {
        uint32_t v[6] = {dp, o + 8, 0x3e8, 0x004ac2f0u, o, en2_r32(e, o + 0x210)};
        uint32_t r = e->call(e->user, vfn(e, dp, 0, 0x34), v, 6);
        if (r != 0) {
            en2_call2(e, 0x004aac00u, rec, r);
            uint32_t f = en2_r32(e, o + 0x254);
            if (f != 0) en2_call1(e, f, r);
        } else {
            uint32_t f = en2_r32(e, o + 0x258);
            if (f != 0) en2_call0(e, f);
        }
    }
    return en2_call2(e, 0x004aabc4u, rec, 0);
}

/* 0x004a2ea0 (render_set_background): vtable (+0x30) +0x0c (o), the window 0x004a8f34() -> +0x2c,
 * the pixel format string 0x00ad3200 = 0x004e84b8 / 0x004e84c4 by 0x00496284; flag bl = 0: the file
 * path is opened (0x00498b38, size out), read whole into 0x00ad3204 (0x00498bb8), closed
 * (0x00498cb0), 1 on open failure or short read; bl != 0: path is the image itself (0 -> 0x00ad3204).
 * +0x14 = the image; vtable +0x20 (o); +0x28 = 0; display 0x004b4828(0x00c84840, o + 0x24, [o],
 * [o + 4]; 0x800, 0): nonzero is returned, else vtable +0x10 (o) and 0. */
uint32_t en7_render_set_background(En2Env* e, uint32_t o, uint32_t path, uint32_t ebx)
{
    const uint32_t sz = e->scratch;
    en2_call4(e, vfn(e, o, 0x30, 0x0c), o, en2_r32(e, o + 0x30), ebx, path);
    en2_w32(e, o + 0x2c, en2_call0(e, 0x004a8f34u));
    en2_w32(e, 0x00ad3200u, (en2_call0(e, 0x00496284u) & 0xff) == 0 ? 0x004e84b8u : 0x004e84c4u);
    if ((ebx & 0xff) == 0) {
        uint32_t h = en2_call2(e, 0x00498b38u, path, sz);
        if (h == 0xffffffffu) return 1;
        uint32_t n = en2_call3(e, 0x00498bb8u, h, 0x00ad3204u, en2_r32(e, sz));
        en2_call2(e, 0x00498cb0u, h, n);
        if (n != en2_r32(e, sz)) return 1;
        en2_w32(e, o + 0x14, 0x00ad3204u);
    } else {
        en2_w32(e, o + 0x14, path);
        if (path == 0) en2_w32(e, o + 0x14, 0x00ad3204u);
    }
    en2_call2(e, vfn(e, o, 0x30, 0x20), o, en2_r32(e, o + 0x30));
    en2_w32(e, o + 0x28, 0);
    uint32_t v[6] = {EN2_DISPLAY, o + 0x24, en2_r32(e, o), en2_r32(e, o + 4), 0x800, 0};
    uint32_t r = e->call(e->user, 0x004b4828u, v, 6);
    if (r != 0) return r;
    en2_call2(e, vfn(e, o, 0x30, 0x10), o, en2_r32(e, o + 0x30));
    return 0;
}
