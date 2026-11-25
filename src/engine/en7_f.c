/*
 * en7_f.c: see en7_f.h.
 */
#include "en7_f.h"

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

/* 0x004ab9d0: the network session object's destructor body: the thread +0x1f0 stopped as in
 * 0x004abd38 (WM_QUIT, 1 s wait, TerminateThread or CloseHandle), the message list (+0x1e0, cursor
 * +0x1e8) and the player list (+0x214, cursor +0x21c) freed node by node (0x00497a28(node + 0x1b0,
 * 0), freed as its answer - 0x1b0) and their fields cleared, the three mutexes +0x1f4 / +0x1f8 /
 * +0x1fc released and closed. Returns o. */
uint32_t en7_net_session_destroy(En2Env* e, uint32_t o)
{
    if (en2_r32(e, o + 0x1f0) != 0) {
        en2_call4(e, 0x00c90538u, en2_r32(e, o + 0x200), 0x12, 0, 0);
        uint32_t t = en2_r32(e, o + 0x1f0);
        en2_w32(e, o + 0x20c, 0);
        if (en2_call2(e, 0x00c9030cu, t, 1000) != 0) en2_call2(e, 0x00c90308u, en2_r32(e, o + 0x1f0), 0);
        else en2_call1(e, EN2_CloseHandle, en2_r32(e, o + 0x1f0));
        en2_w32(e, o + 0x1f0, 0);
    }
    free_list(e, o, 0x1e0, 0x1e8);
    en2_w32(e, o + 0x1e4, 0);
    en2_w32(e, o + 0x1e8, 0);
    en2_w32(e, o + 0x1ec, 0);
    en2_w32(e, o + 0x1e0, 0);
    free_list(e, o, 0x214, 0x21c);
    en2_w32(e, o + 0x214, 0);
    en2_w32(e, o + 0x218, 0);
    en2_w32(e, o + 0x220, 0);
    for (uint32_t k = 0x1f4; k <= 0x1fc; k += 4) {
        en2_call1(e, 0x00c902fcu, en2_r32(e, o + k));
        en2_call1(e, EN2_CloseHandle, en2_r32(e, o + k));
    }
    return o;
}

static uint32_t dp_close(En2Env* e, uint32_t o, uint32_t r)
{
    uint32_t dp = en2_r32(e, o + 0x24);
    en2_call1(e, m(e, dp, 0x10), dp);
    dp = en2_r32(e, o + 0x24);
    en2_call1(e, m(e, dp, 8), dp);
    en2_w32(e, o + 0x24, 0);
    en2_w32(e, o + 0x10, 0);
    return r;
}

/* 0x004aa7dc: joins the session sess (needs the DirectPlay object ready, +0x18; else E_FAIL): +0x0c
 * = 0; a DPSESSIONDESC2 on the stack (cleared by memset, dwSize 0x50, guidInstance from sess + 4,
 * guidApplication from [o], password = the player record's +0xdc (0x004ac06c)); the player record
 * +0x30 gets the session name (0x004ac01c(sess + 0x20)) and the values sess +0x1a0 / +0x1a4
 * (0x004ac0b4 / 0x004ac0a4); IDirectPlay2::Open (+0x60, DPOPEN_JOIN), then the local player
 * (0x004988e8([o + 0x34], dp)); a failure closes (+0x10) and releases DirectPlay, clears +0x24 and
 * +0x10 and returns the HRESULT. Success: a 0x1c byte object (operator new, constructor 0x004979fc)
 * -> +0x38, +0x10 = 1, returns 0. */
uint32_t en7_dplay_join_session(En2Env* e, uint32_t o, uint32_t sess)
{
    if (en2_r32(e, o + 0x18) == 0) return 0x80004005u;
    const uint32_t L = e->scratch;
    en2_w32(e, o + 0x0c, 0);
    en2_call3(e, 0x004bf830u, L, 0, 0x50);
    en2_w32(e, L, 0x50);
    uint32_t g = en2_r32(e, o);
    for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, L + 0x18 + k, en2_r32(e, g + k));
    for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, L + 8 + k, en2_r32(e, sess + 4 + k));
    en2_w32(e, L + 0x34, en2_call1(e, 0x004ac06cu, en2_r32(e, o + 0x30)));
    en2_call2(e, 0x004ac01cu, en2_r32(e, o + 0x30), sess + 0x20);
    en2_call2(e, 0x004ac0b4u, en2_r32(e, o + 0x30), en2_r32(e, sess + 0x1a0));
    en2_call2(e, 0x004ac0a4u, en2_r32(e, o + 0x30), en2_r32(e, sess + 0x1a4));
    uint32_t dp = en2_r32(e, o + 0x24);
    uint32_t r = en2_call3(e, m(e, dp, 0x60), dp, L, 1);
    if (r != 0) return dp_close(e, o, r);
    r = en2_call2(e, 0x004988e8u, en2_r32(e, o + 0x34), en2_r32(e, o + 0x24));
    if (r != 0) return dp_close(e, o, r);
    uint32_t p = en2_call1(e, 0x004a8f48u, 0x1c);
    if (p != 0) p = en2_call1(e, 0x004979fcu, p);
    en2_w32(e, o + 0x10, 1);
    en2_w32(e, o + 0x38, p);
    return 0;
}

/* 0x004a4eb4: draws a widget's bitmap for its state (+0xbc / +0xbd say which bitmaps exist; part
 * 0 / 1 / 2): normal +0xdc / +0xf0 / +0x104; pressed +0x118 / +0x12c / +0x140; focused +0x154 /
 * +0x168 / +0x17c; the third state +0x190 / +0x1a4 / +0x1b8. A missing state bitmap keeps the
 * normal one; a missing focused / third one (when its fallback flag is not set either) also asks
 * the vtable +0xe8 (o, hdc, rect, third) to draw. The bitmap is drawn by 0x004aa2d8(bitmap, hdc,
 * rect, rect). Returns 0; pops three stack words. */
uint32_t en7_widget_draw_bitmap(En2Env* e, uint32_t o, uint32_t hdc, uint32_t rect, uint32_t part,
                                uint32_t pressed, uint32_t focus, uint32_t third)
{
    uint8_t bc = en2_r8(e, o + 0xbc), bd = en2_r8(e, o + 0xbd);
    uint32_t p;
    int self = 0;
    if (part == 1 && (bc & 2)) p = o + 0xf0;
    else if (part == 2 && (bc & 4)) p = o + 0x104;
    else if (bc & 1) p = o + 0xdc;
    else p = 0;
    if (pressed != 0) {
        if (part == 1 && (bc & 0x10)) p = o + 0x12c;
        else if (part == 2 && (bc & 0x20)) p = o + 0x140;
        else if (part == 0 && (bc & 8)) p = o + 0x118;
    } else if (focus != 0) {
        if (part == 1) {
            if (bc & 0x40) p = o + 0x168;
            else if (!(bd & 2)) self = 1;
        } else if (part == 2) {
            if (bd & 1) p = o + 0x17c;
            else if (!(bd & 8)) self = 1;
        } else if (part == 0) {
            if (bc & 0x80) p = o + 0x154;
            else if (!(bd & 4)) self = 1;
        }
    } else if (third != 0) {
        if (part == 1 && (bd & 2)) p = o + 0x1a4;
        else if (part == 2 && (bd & 8)) p = o + 0x1b8;
        else if (part == 0 && (bd & 4)) p = o + 0x190;
        else self = 1;
    }
    if (p != 0) en2_call4(e, 0x004aa2d8u, p, hdc, rect, rect);
    if (self) en2_call4(e, en2_r32(e, en2_r32(e, o + 0x34) + 0xe8), o, hdc, rect, third);
    return 0;
}
