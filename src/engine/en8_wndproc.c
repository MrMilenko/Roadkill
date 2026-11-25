/*
 * en8_wndproc.c: the window object's message dispatcher, TM2.EXE 0x004a7154 (see en8_wndproc.h).
 * Verified under emulation: tools/emu/check_en8_wndproc.sh.
 */
#include "en8_wndproc.h"

#define I_CallWindowProcA 0x00c9046cu
#define I_ShowWindow      0x00c90584u
#define I_RemovePropA     0x00c90548u
#define I_SetWindowLongA  0x00c90578u
#define I_c904dc          0x00c904dcu   /* (hwnd); 0 -> PostQuitMessage(0) */
#define I_PostQuitMessage 0x00c90534u
#define I_DestroyWindow   0x00c9048cu
#define I_GetPropA        0x00c904e0u
#define I_GetMessagePos   0x00c904d8u
#define I_ScreenToClient  0x00c9054cu
#define I_GetMenu         0x00c904d0u
#define I_GetMenuItemInfoA 0x00c904d4u
#define I_PeekMessageA    0x00c90530u
#define PROP_OBJ          0x004e3194u
#define PROP_OLD          0x004e31d0u

/* frame (the original's ebp - 0x64 = F) */
enum { W_MII = 0x00, W_MSG = 0x2c, W_PT = 0x48 };

static uint32_t vt(En2Env* e, uint32_t o, uint32_t slot) { return en2_r32(e, en2_r32(e, o + 0x34) + slot); }
static uint32_t m1(En2Env* e, uint32_t o, uint32_t slot) { return en2_call1(e, vt(e, o, slot), o); }
static uint32_t m2(En2Env* e, uint32_t o, uint32_t slot, uint32_t a) { return en2_call2(e, vt(e, o, slot), o, a); }
static uint32_t m3(En2Env* e, uint32_t o, uint32_t slot, uint32_t a, uint32_t b)
{ return en2_call3(e, vt(e, o, slot), o, a, b); }
static uint32_t m4(En2Env* e, uint32_t o, uint32_t slot, uint32_t a, uint32_t b, uint32_t c)
{ return en2_call4(e, vt(e, o, slot), o, a, b, c); }
static uint32_t prop_obj(En2Env* e, uint32_t h) { return en2_call2(e, I_GetPropA, h, PROP_OBJ); }
#define LO(x) ((x) & 0xffffu)
#define HI(x) ((x) >> 16)

/* WM_COMMAND (also reached after WM_NOTIFY) */
static uint32_t on_command(En2Env* e, uint32_t obj, uint32_t wp, uint32_t lp, uint32_t r)
{
    const uint32_t F = e->scratch;
    uint32_t code = HI(wp);
    if (lp != 0) {   /* from a control: its object's command handler */
        uint32_t o2 = prop_obj(e, lp);
        if (o2 == 0) return r;
        return m3(e, o2, 0x9c, code, 0);
    }
    /* a menu: the item's data (MIIM_DATA) is a handler function (eax = code, edx = id) */
    en2_w32(e, F + W_MII, 0x2c);
    en2_w32(e, F + W_MII + 4, 0x20);
    uint32_t menu = en2_call1(e, I_GetMenu, en2_r32(e, obj));
    en2_call4(e, I_GetMenuItemInfoA, menu, LO(wp), 0, F + W_MII);
    uint32_t fn = en2_r32(e, F + W_MII + 0x20);
    if (fn != 0) return en2_call2(e, fn, code, LO(wp));
    return m4(e, obj, 0x60, LO(wp), code, lp);
}

uint32_t en8_wndproc(En2Env* e, uint32_t obj, uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t esi_in,
                     uint32_t lp)
{
    const uint32_t F = e->scratch;
    uint32_t r = esi_in;
    switch (msg) {
    case 0x02: r = 0; break;                                     /* WM_DESTROY */
    case 0x03: r = m3(e, obj, 0x20, LO(lp), HI(lp)); break;      /* WM_MOVE */
    case 0x05: r = m3(e, obj, 0x1c, LO(lp), HI(lp)); break;      /* WM_SIZE */
    case 0x07: r = m2(e, obj, 0x58, wp); break;
    case 0x08: r = m2(e, obj, 0x5c, wp); break;
    case 0x0f: r = m2(e, obj, 0x50, wp); break;
    case 0x10:                                                   /* WM_CLOSE */
        if (m1(e, obj, 0x10) != 0 && en2_r32(e, obj) != 0) {
            en2_call2(e, I_ShowWindow, en2_r32(e, obj), 0);
            if (en2_call2(e, I_RemovePropA, en2_r32(e, obj), PROP_OLD) != 0 && en2_r32(e, obj + 4) != 0)
                en2_call3(e, I_SetWindowLongA, en2_r32(e, obj), 0xfffffffcu /* GWL_WNDPROC */, en2_r32(e, obj + 4));
            uint32_t t = en2_call1(e, I_c904dc, en2_r32(e, obj));
            if (t == 0) en2_call1(e, I_PostQuitMessage, 0);
            en2_call1(e, I_DestroyWindow, en2_r32(e, obj));
            en2_w32(e, obj, 0);
        }
        r = 0;
        break;
    case 0x11: r = m1(e, obj, 0x10); break;
    case 0x14: r = m2(e, obj, 0x54, wp); break;
    case 0x1c: r = m2(e, obj, 0x70, wp); break;
    case 0x21: r = m3(e, obj, 0x4c, wp, LO(lp)); break;
    case 0x24: r = m2(e, obj, 0x74, lp); break;
    case 0x2b: {                                                 /* WM_DRAWITEM: the item's object */
        uint32_t o2 = prop_obj(e, en2_r32(e, lp + 0x14));
        r = o2 ? m2(e, o2, 0xa0, lp) : 0;
        break;
    }
    case 0x4e: {                                                 /* WM_NOTIFY */
        uint32_t o2 = prop_obj(e, en2_r32(e, lp));
        if (o2 != 0) {
            uint32_t pos = en2_call0(e, I_GetMessagePos);
            en2_w32(e, F + W_PT, LO(pos));
            en2_w32(e, F + W_PT + 4, HI(pos));
            en2_call2(e, I_ScreenToClient, en2_r32(e, o2), F + W_PT);
            r = m4(e, o2, 0x94, en2_r32(e, F + W_PT), en2_r32(e, F + W_PT + 4), lp);
        }
        /* ORIGINAL: falls through into WM_COMMAND */
        r = on_command(e, obj, wp, lp, r);
        break;
    }
    case 0x100: r = m3(e, obj, 0x2c, wp, lp); break;
    case 0x101: r = m3(e, obj, 0x28, wp, lp); break;
    case 0x102: r = m3(e, obj, 0x24, wp, lp); break;
    case 0x104: r = m2(e, obj, 0x7c, wp) != 1 ? 2 : 1; break;
    case 0x105: r = m2(e, obj, 0x78, wp) != 1 ? 2 : 1; break;
    case 0x111: r = on_command(e, obj, wp, lp, r); break;
    case 0x112: r = m2(e, obj, 0x84, wp); break;
    case 0x113: r = m2(e, obj, 0x18, wp); break;
    case 0x116: r = m2(e, obj, 0x14, wp); break;
    case 0x121:
        if (wp == 2) { r = m1(e, obj, 0x6c); break; }
        if (wp != 0) { r = 0; break; }
        {
            uint32_t o2 = prop_obj(e, lp);
            if (o2 == 0) break;
            while (en2_call5(e, I_PeekMessageA, F + W_MSG, 0, 0, 0, 0) == 0) r = m1(e, o2, 0x6c);
        }
        break;
    case 0x132: case 0x133: case 0x134: case 0x135: case 0x136: case 0x137: case 0x138: {   /* WM_CTLCOLOR* */
        uint32_t o2 = prop_obj(e, lp);
        if (o2 == 0) { r = 2; break; }
        uint32_t b = m2(e, o2, 0xa8, wp);
        if (b != 0) return b;
        break;
    }
    case 0x200: r = m3(e, obj, 0x48, LO(lp), HI(lp)); break;
    case 0x201: r = m3(e, obj, 0x30, LO(lp), HI(lp)); break;
    case 0x202: r = m3(e, obj, 0x34, LO(lp), HI(lp)); break;
    case 0x203: r = m3(e, obj, 0x38, LO(lp), HI(lp)); break;
    case 0x204: r = m3(e, obj, 0x3c, LO(lp), HI(lp)); break;
    case 0x205: r = m3(e, obj, 0x40, LO(lp), HI(lp)); break;
    case 0x206: r = m3(e, obj, 0x44, LO(lp), HI(lp)); break;
    case 0x211: r = m2(e, obj, 0x64, wp); break;
    case 0x212: r = m2(e, obj, 0x68, wp); break;
    case 0x218: r = m2(e, obj, 0x80, wp); break;
    case 0x30f: r = m1(e, obj, 0x90); break;
    case 0x310: r = m2(e, obj, 0x8c, wp); break;
    case 0x311: r = m2(e, obj, 0x88, wp); break;
    case 0x3b9: r = m3(e, obj, 0x98, wp, lp); break;
    default: r = (msg == en2_r32(e, obj + 0x14)) ? 1 : 2; break;
    }
    if (r != 2) return r;
    uint32_t prev = en2_r32(e, obj + 4);
    if (prev == 0) return 0;
    {
        uint32_t v[5] = {prev, hwnd, msg, wp, lp};
        return e->call(e->user, I_CallWindowProcA, v, 5);
    }
}
