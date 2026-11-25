/*
 * en7_h.c: see en7_h.h.
 */
#include "en7_h.h"

static uint32_t m(En2Env* e, uint32_t obj, uint32_t off) { return en2_r32(e, en2_r32(e, obj) + off); }

static void free_list(En2Env* e, uint32_t o, uint32_t head, uint32_t cursor)
{
    en2_w32(e, o + cursor, en2_r32(e, o + head));
    if (en2_r32(e, o + head) == 0) return;
    uint32_t nx;
    do {
        uint32_t p = en2_r32(e, o + cursor);
        nx = en2_r32(e, p + 0x1cc);
        if (p != 0) en2_call1(e, 0x004aa39cu, en2_call2(e, 0x00497a28u, p + 0x1b0, 0) - 0x1b0);
        en2_w32(e, o + cursor, nx);
    } while (nx != 0);
}

/* 0x004abb68: (re)starts the network object: +0 = dp, +0x224 = list box, +0x228 = x; state +0x208
 * = 0 (under the state mutex); the old thread stopped (as 0x004abd38), the session and message lists
 * freed (as 0x004ab9d0); a new thread CreateThread(0, 0, 0x004ac494, o, 0, &+0x200) -> +0x1f0 (state
 * 0 again first); Sleep(500). Returns 0. */
uint32_t en7_net_session_start(En2Env* e, uint32_t o, uint32_t dp, uint32_t lb, uint32_t x)
{
    en2_w32(e, o, dp);
    uint32_t mtx = en2_r32(e, o + 0x1f8);
    en2_w32(e, o + 0x224, lb);
    en2_w32(e, o + 0x228, x);
    en2_call2(e, 0x00c9030cu, mtx, 0xffffffffu);
    mtx = en2_r32(e, o + 0x1f8);
    en2_w32(e, o + 0x208, 0);
    en2_call1(e, 0x00c902fcu, mtx);
    if (en2_r32(e, o + 0x1f0) != 0) {
        en2_call4(e, 0x00c90538u, en2_r32(e, o + 0x200), 0x12, 0, 0);
        uint32_t t = en2_r32(e, o + 0x1f0);
        en2_w32(e, o + 0x20c, 0);
        if (en2_call2(e, 0x00c9030cu, t, 1000) != 0) en2_call2(e, 0x00c90308u, en2_r32(e, o + 0x1f0), 0);
        else en2_call1(e, EN2_CloseHandle, en2_r32(e, o + 0x1f0));
        en2_w32(e, o + 0x1f0, 0);
    }
    free_list(e, o, 0x214, 0x21c);
    en2_w32(e, o + 0x218, 0);
    en2_w32(e, o + 0x220, 0);
    en2_w32(e, o + 0x214, 0);
    free_list(e, o, 0x1e0, 0x1e8);
    en2_w32(e, o + 0x1e4, 0);
    en2_w32(e, o + 0x1e8, 0);
    en2_w32(e, o + 0x1ec, 0);
    en2_w32(e, o + 0x1e0, 0);
    if (en2_r32(e, o + 0x1f0) == 0) {
        en2_call2(e, 0x00c9030cu, en2_r32(e, o + 0x1f8), 0xffffffffu);
        mtx = en2_r32(e, o + 0x1f8);
        en2_w32(e, o + 0x208, 0);
        en2_call1(e, 0x00c902fcu, mtx);
        uint32_t v[6] = {0, 0, 0x004ac494u, o, 0, o + 0x200};
        en2_w32(e, o + 0x1f0, e->call(e->user, 0x00c902b4u, v, 6));
    }
    en2_call1(e, 0x00c90304u, 0x1f4);
    return 0;
}

/* 0x004aa690: hosts a session (needs +0x18; else E_FAIL): +0x0c = 1; a DPSESSIONDESC2 (memset,
 * dwSize 0x50, flags 0x44, guidApplication from [o], 8 players, the name / password / user words of
 * the player record +0x30 (0x004ac040 / 0x004ac06c / 0x004ac0b8 / 0x004ac0ac), dwUser3 = 3, dwUser4
 * = timeGetTime); IDirectPlay2::Open (+0x60, DPOPEN_CREATE) (failure: Release, +0x24 / +0x10 / +0x18
 * cleared, the HRESULT returned); method +0x38 (dp, record + 0x22c, 0); the local player
 * 0x004988e8 (failure: Close, Release, +0x24 / +0x10 cleared); success as 0x004aa7dc. */
uint32_t en7_dplay_host_session(En2Env* e, uint32_t o)
{
    if (en2_r32(e, o + 0x18) == 0) return 0x80004005u;
    const uint32_t L = e->scratch;
    en2_w32(e, o + 0x0c, 1);
    en2_call3(e, 0x004bf830u, L, 0, 0x50);
    en2_w32(e, L, 0x50);
    uint32_t g = en2_r32(e, o);
    for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, L + 0x18 + k, en2_r32(e, g + k));
    en2_w32(e, L + 0x28, 8);
    en2_w32(e, L + 4, 0x44);
    en2_w32(e, L + 0x30, en2_call1(e, 0x004ac040u, en2_r32(e, o + 0x30)));
    en2_w32(e, L + 0x34, en2_call1(e, 0x004ac06cu, en2_r32(e, o + 0x30)));
    en2_w32(e, L + 0x40, en2_call1(e, 0x004ac0b8u, en2_r32(e, o + 0x30)));
    en2_w32(e, L + 0x44, en2_call1(e, 0x004ac0acu, en2_r32(e, o + 0x30)));
    en2_w32(e, L + 0x48, 3);
    en2_w32(e, L + 0x4c, en2_call0(e, 0x00c90640u));
    uint32_t dp = en2_r32(e, o + 0x24);
    uint32_t r = en2_call3(e, m(e, dp, 0x60), dp, L, 2);
    if (r != 0) {
        dp = en2_r32(e, o + 0x24);
        en2_call1(e, m(e, dp, 8), dp);
        en2_w32(e, o + 0x24, 0);
        en2_w32(e, o + 0x10, 0);
        en2_w32(e, o + 0x18, 0);
        return r;
    }
    dp = en2_r32(e, o + 0x24);
    en2_call3(e, m(e, dp, 0x38), dp, en2_r32(e, o + 0x30) + 0x22c, 0);
    r = en2_call2(e, 0x004988e8u, en2_r32(e, o + 0x34), en2_r32(e, o + 0x24));
    if (r != 0) {
        dp = en2_r32(e, o + 0x24);
        en2_call1(e, m(e, dp, 0x10), dp);
        dp = en2_r32(e, o + 0x24);
        en2_call1(e, m(e, dp, 8), dp);
        en2_w32(e, o + 0x24, 0);
        en2_w32(e, o + 0x10, 0);
        return r;
    }
    uint32_t p = en2_call1(e, 0x004a8f48u, 0x1c);
    if (p != 0) p = en2_call1(e, 0x004979fcu, p);
    en2_w32(e, o + 0x10, 1);
    en2_w32(e, o + 0x38, p);
    return 0;
}

/* 0x004a9e88: a dialog object's message dispatch (o, hwnd, msg, wParam; lParam on the stack):
 * WM_INITDIALOG: +0 = hwnd, vtable +0xcc (o, wParam); WM_COMMAND: the control (lParam, or
 * GetDlgItem(+0, id)) with a widget ("ST Object") gets vtable +0x9c (widget, notify code), without a
 * control vtable +0x60 (o, id, notify, 0); WM_CLOSE: EndDialog (modal +0x54) or DestroyWindow, 0;
 * WM_DESTROY: +0 = 0 for a modal one; WM_QUERYENDSESSION: vtable +0x10 (o) with 1 / 0 swapped;
 * other messages the default procedure 0x004a7154. ORIGINAL BUG: WM_DESTROY and a WM_COMMAND whose
 * control has no widget return the caller's edi (never set here): those returns are undefined (the
 * port returns 0). Pops one stack word. */
uint32_t en7_dialog_proc(En2Env* e, uint32_t o, uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp)
{
    uint32_t vt = en2_r32(e, o + 0x34);
    if (msg == 0x110) {
        en2_w32(e, o, hwnd);
        return en2_call4(e, en2_r32(e, vt + 0xcc), o, wp, vt, wp);
    }
    if (msg == 0x111) {
        uint32_t code = wp >> 16, ctl = lp;
        if (ctl == 0) ctl = en2_call2(e, 0x00c904c8u, en2_r32(e, o), wp & 0xffff);
        if (ctl != 0) {
            uint32_t g = en2_call2(e, 0x00c904e0u, ctl, 0x004e3338u);
            if (g == 0) return 0;
            uint32_t gv = en2_r32(e, g + 0x34);
            return en2_call4(e, en2_r32(e, gv + 0x9c), g, code, 0, gv);
        }
        return en2_call4(e, en2_r32(e, vt + 0x60), o, wp & 0xffff, code, 0);
    }
    if (msg == 0x10 || msg == 2) {
        if (msg == 0x10) {
            if (en2_r32(e, o + 0x54) != 0) en2_call2(e, 0x00c904a4u, en2_r32(e, o), 0);
            else en2_call1(e, 0x00c9048cu, en2_r32(e, o));
        }
        if (en2_r32(e, o + 0x54) != 0) en2_w32(e, o, 0);
        return 0;
    }
    if (msg == 0x11) {
        uint32_t r = en2_call4(e, en2_r32(e, vt + 0x10), o, vt, msg, wp);
        if (r == 1) return 0;
        if (r == 0) return 1;
        return r;
    }
    uint32_t v[5] = {o, hwnd, msg, wp, lp};
    return e->call(e->user, 0x004a7154u, v, 5);
}
