/*
 * en4_list.c: see en4_list.h. Verified under emulation: tools/emu/check_en4_list.sh.
 */
#include "en4_list.h"

#define EN4_lstrcpyA        0x00c9031cu
#define EN4_GetDlgItem      0x00c904c8u
#define EN4_GetPropA        0x00c904e0u
#define EN4_CreateWindowExA 0x00c90480u
#define EN4_LoadMenuA       0x00c90518u
#define EN4_SetWindowLongA  0x00c90578u
#define EN4_SetPropA        0x00c90570u
#define EN4_RegisterWindowMessageA 0x00c90540u
#define EN4_GlobalAlloc     0x00c902d4u
#define EN4_GlobalLock      0x00c902dcu
#define EN4_GlobalUnlock    0x00c902e0u
#define EN4_GlobalFree      0x00c902d8u
#define EN4_CreatePalette   0x00c901d0u

/* 0x00497a68: appends a new node (operator new 0x004a8f48(0x138); no null check, as in the
 * original) to the list {+0 head, +4 tail, +8 cursor, +0x0c count} while the count is below 20:
 * key -1, +4 .. +0x10 = 0, strings +0x14 / +0x94 = "" (EXE), +0x114 = -1, +0x118 = 0, +0x11c ..
 * +0x12c = -1, next +0x134 = 0. Tail and cursor end on the new node (the tail is found by walking
 * from the head with the cursor). Returns the node, 0 when full. */
uint32_t en4_keyed_list_add(En2Env* e, uint32_t o)
{
    if ((int32_t)en2_r32(e, o + 0x0c) >= 0x14) return 0;
    uint32_t p = en2_call1(e, 0x004a8f48u, 0x138);
    en2_w32(e, o + 0x0c, en2_r32(e, o + 0x0c) + 1);
    uint32_t head = en2_r32(e, o);
    if (head == 0) {
        en2_w32(e, o, p);
        en2_w32(e, p + 0x134, 0);
        en2_w32(e, o + 4, p);
        en2_w32(e, o + 8, p);
    } else {
        en2_w32(e, o + 8, head);
        do {
            en2_w32(e, o + 4, en2_r32(e, o + 8));
            en2_w32(e, o + 8, en2_r32(e, en2_r32(e, o + 4) + 0x134));
        } while (en2_r32(e, o + 8) != 0);
        en2_w32(e, en2_r32(e, o + 4) + 0x134, p);
        en2_w32(e, p + 0x134, 0);
        en2_w32(e, o + 4, p);
        en2_w32(e, o + 8, p);
    }
    en2_w32(e, p, 0xffffffffu);
    for (uint32_t f = 4; f <= 0x10; f += 4) en2_w32(e, p + f, 0);
    en2_w32(e, p + 0x114, 0xffffffffu);
    en2_w32(e, p + 0x118, 0);
    for (uint32_t f = 0x11c; f <= 0x12c; f += 4) en2_w32(e, p + f, 0xffffffffu);
    en2_call2(e, EN4_lstrcpyA, p + 0x14, 0x004e2f58u);
    en2_call2(e, EN4_lstrcpyA, p + 0x94, 0x004e2f59u);
    return p;
}

/* 0x00497c7c: refreshes the list from DirectPlay: marks every node stale (+4 = 1), count +0x10 =
 * 0, mode +0x14; with an object dp, method +0x30 (an enumeration: mode 1 -> (dp, ebx, callback
 * 0x004982d4, o, 0x80), mode 2 -> (dp, 0, callback, o, 0x20)); when that succeeds the nodes still
 * stale are unlinked and freed (0x004aa39c), the node count +0x0c decremented. Returns +0x10.
 * ORIGINAL BUG: for other modes it tests the caller's esi (never set here) instead of a result;
 * the port takes it as a failure (nothing removed). Only modes 1 and 2 are verified. */
uint32_t en4_session_enum(En2Env* e, uint32_t o, uint32_t dp, uint32_t ebx, uint32_t mode)
{
    for (uint32_t p = en2_r32(e, o); p != 0; p = en2_r32(e, p + 0x134)) en2_w32(e, p + 4, 1);
    en2_w32(e, o + 0x10, 0);
    en2_w32(e, o + 0x14, mode);
    if (dp == 0) return en2_r32(e, o + 0x10);
    uint32_t r = 1;
    if (mode == 1 || mode == 2) {
        uint32_t v[5] = {dp, mode == 1 ? ebx : 0, 0x004982d4u, o, mode == 1 ? 0x80u : 0x20u};
        r = e->call(e->user, en2_r32(e, en2_r32(e, dp) + 0x30), v, 5);
    }
    if (r == 0) {
        uint32_t prev = en2_r32(e, o), cur = prev;
        while (cur != 0) {
            if (en2_r32(e, cur + 4) == 1) {
                if (cur == en2_r32(e, o)) en2_w32(e, o, en2_r32(e, cur + 0x134));
                else en2_w32(e, prev + 0x134, en2_r32(e, cur + 0x134));
                uint32_t next = en2_r32(e, cur + 0x134);
                en2_call1(e, 0x004aa39cu, cur);
                en2_w32(e, o + 0x0c, en2_r32(e, o + 0x0c) - 1);
                cur = next;
            } else {
                prev = cur;
                cur = en2_r32(e, cur + 0x134);
            }
        }
    }
    return en2_r32(e, o + 0x10);
}

/* 0x004982d4: the enumeration callback (stdcall; id, type, DPNAME name, flags, list o): the node
 * with key id (added by 0x00497a68 when missing, keyed id) counts (+0x10 of o), gets the short
 * (+8) and long (+0x0c) names at +0x14 / +0x94 and is not stale (+4 = 0). Returns 1 (continue),
 * 0 when a node could not be added. */
uint32_t en4_enum_players_cb(En2Env* e, uint32_t id, uint32_t type, uint32_t name, uint32_t flags, uint32_t o)
{
    (void)type; (void)flags;
    uint32_t p = en2_r32(e, o);
    while (p != 0 && en2_r32(e, p) != id) p = en2_r32(e, p + 0x134);
    int added = 0;
    if (p == 0) {
        p = en2_call1(e, 0x00497a68u, o);
        if (p == 0) return 0;
        added = 1;
    }
    en2_w32(e, o + 0x10, en2_r32(e, o + 0x10) + 1);
    en2_call2(e, EN4_lstrcpyA, p + 0x14, en2_r32(e, name + 8));
    en2_call2(e, EN4_lstrcpyA, p + 0x94, en2_r32(e, name + 0x0c));
    en2_w32(e, p + 4, 0);
    if (added) en2_w32(e, p, id);
    return 1;
}

/* 0x004a22e4: joystick direction code for device 2 (input_get_device_type 0x004932a0 must say 1):
 * buttons 0x004c41c8 / 0x004c4208 / 0x004c41e8 of the joystick 0x00a82164 give 5 / 6 / 7, then
 * axis x 0x004c4130 > 0 -> 1, < 0 -> 2, axis y 0x004c414c > 0 -> 3, < 0 -> 4 (each axis asked
 * again for the second test). Returns the code in the low byte (0 = nothing). */
uint32_t en4_joy_direction(En2Env* e)
{
    uint32_t r = en2_call1(e, 0x004932a0u, 2);
    uint8_t d = 0;
    if ((r & 0xff) != 1) return (r & ~0xffu) | d;
    const uint32_t J = 0x00a82164u;
    if (en2_call1(e, 0x004c41c8u, J) != 0) d = 5;
    else if (en2_call1(e, 0x004c4208u, J) != 0) d = 6;
    else if ((r = en2_call1(e, 0x004c41e8u, J)) != 0) d = 7;
    if ((int32_t)(r = en2_call1(e, 0x004c4130u, J)) > 0) return (r & ~0xffu) | 1;
    if ((int32_t)(r = en2_call1(e, 0x004c4130u, J)) < 0) return (r & ~0xffu) | 2;
    if ((int32_t)(r = en2_call1(e, 0x004c414cu, J)) > 0) return (r & ~0xffu) | 3;
    if ((int32_t)(r = en2_call1(e, 0x004c414cu, J)) < 0) return (r & ~0xffu) | 4;
    return (r & ~0xffu) | d;
}

/* 0x004a2644: clears the joystick button tables 0x00a823fc / 0x00a8247c (0x80 bytes each), opens
 * the device 0x00a82560 (0x004a7be0); when that works, its caps 0x004a81e8 -> 0x00a82530 and the
 * feature flags 0x00a82534 (axes >= 3, +0x214), 0x00a82538 (axes >= 4), 0x00a8253c (+0x218),
 * 0x00a82540 (+0x224), 0x00a82544 (+0x228), 0x00a82548 (+0x22c), 0x00a8254c (+0x230), each 0/1.
 * Returns the open result. */
uint32_t en4_joy_open(En2Env* e)
{
    en2_call3(e, 0x004bf830u, 0x00a823fcu, 0, 0x80);
    en2_call3(e, 0x004bf830u, 0x00a8247cu, 0, 0x80);
    uint32_t r = en2_call1(e, 0x004a7be0u, 0x00a82560u);
    if (r == 0) return 0;
    uint32_t c = en2_call1(e, 0x004a81e8u, 0x00a82560u);
    en2_w32(e, 0x00a82530u, c);
    if (c == 0) return r;
    en2_w32(e, 0x00a82534u, en2_r32(e, c + 0x214) >= 3);
    en2_w32(e, 0x00a82538u, en2_r32(e, en2_r32(e, 0x00a82530u) + 0x214) >= 4);
    en2_w32(e, 0x00a8253cu, en2_r32(e, en2_r32(e, 0x00a82530u) + 0x218) != 0);
    en2_w32(e, 0x00a82540u, en2_r32(e, en2_r32(e, 0x00a82530u) + 0x224) != 0);
    en2_w32(e, 0x00a82544u, en2_r32(e, en2_r32(e, 0x00a82530u) + 0x228) != 0);
    en2_w32(e, 0x00a82548u, en2_r32(e, en2_r32(e, 0x00a82530u) + 0x22c) != 0);
    en2_w32(e, 0x00a8254cu, en2_r32(e, en2_r32(e, 0x00a82530u) + 0x230) != 0);
    return r;
}

/* 0x004a4068: a control: id +0x50; the dialog's own item (GetDlgItem(parent, id)) when there is
 * one (+0x4c = 1 when the parent's widget object, its "ST Object" property, is a modal dialog,
 * its +0x54 = 1), else CreateWindowExA(0, cls, s0 (text), s1 (style), s2, s3, s4, s5 (rect),
 * parent, id, hinst 0x00bdfae8, 0) (failure -> return 1); then 0x004a7118(o) (subclass) and
 * +0x38 = 0, return 0. Pops six stack words. */
uint32_t en4_control_create(En2Env* e, uint32_t o, uint32_t cls, uint32_t parent, uint32_t id,
                            uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5)
{
    en2_w32(e, o + 0x50, id);
    en2_w32(e, o, en2_call2(e, EN4_GetDlgItem, parent, id));
    en2_w32(e, o + 0x4c, 0);
    if (en2_r32(e, o) != 0) {
        uint32_t g = en2_call2(e, EN4_GetPropA, parent, 0x004e30ecu);
        if (g != 0 && en2_r32(e, g + 0x54) == 1) en2_w32(e, o + 0x4c, 1);
    }
    if (en2_r32(e, o) == 0) {
        uint32_t v[12] = {0, cls, s0, s1, s2, s3, s4, s5, parent, id, en2_r32(e, 0x00bdfae8u), 0};
        uint32_t h = e->call(e->user, EN4_CreateWindowExA, v, 12);
        en2_w32(e, o, h);
        if (h == 0) return 1;
    }
    en2_call1(e, 0x004a7118u, o);
    en2_w32(e, o + 0x38, 0);
    return 0;
}

/* Two identical copies (0x004a59b0, 0x004a5e6c): fire event which (1 or 2) of a widget: event 1 =
 * member action {+0x58 resolver, +0x5c adjust} on the target +0x64, else the callback +0x54;
 * event 2 = member action {+0x6c, +0x70} on +0x78, else the callback +0x68. The member call:
 * g = resolver(t = target + adjust; edx = &resolver, ebx = t, ecx = o), g(t, o; ebx = t, ecx = o);
 * the callback gets (o, which; ebx as passed, ecx = o). Returns 1 if something ran. */
static uint32_t fire(En2Env* e, uint32_t o, uint32_t which, uint32_t ebx)
{
    uint32_t tgt, mp, cb;
    if (which == 1) { tgt = 0x64; mp = 0x58; cb = 0x54; }
    else if (which == 2) { tgt = 0x78; mp = 0x6c; cb = 0x68; }
    else return 0;
    if (en2_r32(e, o + tgt) != 0 && en2_r32(e, o + mp) != 0) {
        uint32_t t = en2_r32(e, o + tgt) + en2_r32(e, o + mp + 4);
        uint32_t g = en2_call4(e, en2_r32(e, o + mp), t, o + mp, t, o);
        en2_call4(e, g, t, o, t, o);
        return 1;
    }
    uint32_t f = en2_r32(e, o + cb);
    if (f == 0) return 0;
    en2_call4(e, f, o, which, ebx, o);
    return 1;
}
uint32_t en4_widget_fire_a(En2Env* e, uint32_t o, uint32_t which, uint32_t ebx) { return fire(e, o, which, ebx); }
uint32_t en4_widget_fire_b(En2Env* e, uint32_t o, uint32_t which, uint32_t ebx) { return fire(e, o, which, ebx); }

/* 0x004a6be8 (win32_create_window): hinst -> 0x00bdfae8; a menu resource id (low 16 bits) is
 * loaded into +0x10; CreateWindowExA(exstyle, cls, name, s0..s5, +0x10, hinst, 0) -> +0 (0 ->
 * return 0); the window is subclassed (old GWL_WNDPROC -> +4, the object's procedure +8 set),
 * tagged with the "ST Object" property = o, and +0x14 = RegisterWindowMessageA(
 * "QueryCancelAutoPlay"). Returns 1; pops eight stack words. */
uint32_t en4_create_main_window(En2Env* e, uint32_t o, uint32_t exstyle, uint32_t cls, uint32_t name,
                                uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5,
                                uint32_t menu, uint32_t hinst)
{
    en2_w32(e, 0x00bdfae8u, hinst);
    if (menu != 0) en2_w32(e, o + 0x10, en2_call2(e, EN4_LoadMenuA, hinst, menu & 0xffff));
    uint32_t v[12] = {exstyle, cls, name, s0, s1, s2, s3, s4, s5, en2_r32(e, o + 0x10), en2_r32(e, 0x00bdfae8u), 0};
    uint32_t h = e->call(e->user, EN4_CreateWindowExA, v, 12);
    en2_w32(e, o, h);
    if (h == 0) return 0;
    en2_w32(e, o + 4, en2_call2(e, EN2_GetWindowLongA, h, (uint32_t)-4));
    en2_call3(e, EN4_SetWindowLongA, en2_r32(e, o), (uint32_t)-4, en2_r32(e, o + 8));
    en2_call3(e, EN4_SetPropA, en2_r32(e, o), 0x004e31b2u, o);
    en2_w32(e, o + 0x14, en2_call1(e, EN4_RegisterWindowMessageA, 0x004e31bcu));
    return 1;
}

/* 0x004aa1bc: a GDI palette from the color table +8 (count +4, entries of 4 bytes whose bytes
 * 0x2a / 0x29 / 0x28 become peRed / peGreen / peBlue; peFlags left as GlobalAlloc gave them) in a
 * LOGPALETTE {0x300, count} (GlobalAlloc GMEM_MOVEABLE, handle in 0x00bdfb34, GlobalLock); +0 =
 * CreatePalette; unlock and free. Returns 0, or 1 when the allocation fails; 0 without a table. */
uint32_t en4_palette_create(En2Env* e, uint32_t o)
{
    if (en2_r32(e, o + 8) == 0) return 0;
    uint32_t hm = en2_call2(e, EN4_GlobalAlloc, 2, en2_r32(e, o + 4) * 4 + 8);
    en2_w32(e, 0x00bdfb34u, hm);
    if (hm == 0) return 1;
    uint32_t lp = en2_call1(e, EN4_GlobalLock, hm);
    en2_w16(e, lp, 0x300);
    en2_w16(e, lp + 2, en2_r16(e, o + 4));
    uint32_t q = lp;
    for (int32_t i = 0; i < (int32_t)en2_r32(e, o + 4); i++, q += 4) {
        en2_w8(e, q + 4, en2_r8(e, en2_r32(e, o + 8) + 4u * (uint32_t)i + 0x2a));
        en2_w8(e, q + 5, en2_r8(e, en2_r32(e, o + 8) + 4u * (uint32_t)i + 0x29));
        en2_w8(e, q + 6, en2_r8(e, en2_r32(e, o + 8) + 4u * (uint32_t)i + 0x28));
    }
    en2_w32(e, o, en2_call1(e, EN4_CreatePalette, lp));
    en2_call1(e, EN4_GlobalUnlock, en2_r32(e, 0x00bdfb34u));
    en2_call1(e, EN4_GlobalFree, en2_r32(e, 0x00bdfb34u));
    return 0;
}

/* 0x004ac718: WM_ACTIVATEAPP: a change of +0x38. Activated: WS_SYSMENU (0x80000) removed unless it
 * was saved in 0x004e8538, keys reset (0x0049eab8), DirectDraw restore (0x004b42c8), the CD
 * resumed (0x004b8b40(0x00c850bc, 2)) if it was paused (0x00bdfcc8), the clock resynced
 * (0x004931fc). Deactivated: 0x004e8538 = the style's WS_SYSMENU, added if missing; the CD mode
 * (0x004b8ad4) 0x20e (playing) -> 0x00bdfcc8 = 1 and paused (0x004b8b40(.., 3)); DirectDraw
 * deactivate 0x004b42b8. Returns 2. */
uint32_t en4_window_activate(En2Env* e, uint32_t o, uint32_t active)
{
    if (active == en2_r32(e, o + 0x38)) return 2;
    uint32_t s = en2_call2(e, EN2_GetWindowLongA, en2_r32(e, o), (uint32_t)-16);
    en2_w32(e, o + 0x38, active);
    if (active != 0) {
        if (en2_r32(e, 0x004e8538u) == 0) en2_call3(e, EN4_SetWindowLongA, en2_r32(e, o), (uint32_t)-16, s & 0xfff7ffffu);
        en2_call0(e, 0x0049eab8u);
        en2_call1(e, 0x004b42c8u, EN2_DISPLAY);
        if (en2_r32(e, 0x00bdfcc8u) != 0) en2_call2(e, 0x004b8b40u, 0x00c850bcu, 2);
        en2_call0(e, 0x004931fcu);
        return 2;
    }
    en2_w32(e, 0x004e8538u, s & 0x80000u);
    if ((s & 0x80000u) == 0) en2_call3(e, EN4_SetWindowLongA, en2_r32(e, o), (uint32_t)-16, s | 0x80000u);
    uint32_t m = e->scratch;
    en2_call2(e, 0x004b8ad4u, 0x00c850bcu, m);
    uint32_t pl = en2_r32(e, m) == 0x20e;
    en2_w32(e, 0x00bdfcc8u, pl);
    if (pl) en2_call2(e, 0x004b8b40u, 0x00c850bcu, 3);
    en2_call1(e, 0x004b42b8u, EN2_DISPLAY);
    return 2;
}
