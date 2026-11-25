/*
 * mt2_icfg_dlg.c: the key rebinding dialog, TM2.EXE 0x00462168..0x00462e5a. See mt2_icfg_dlg.h.
 */
#include "mt2_icfg_dlg.h"

#include <stddef.h>

static uint32_t r8(const Mt2IcfgDlg* m, uint32_t a) { return *m->resolve(m->user, a); }
static uint32_t r32(const Mt2IcfgDlg* m, uint32_t a)
{
    const uint8_t* p = m->resolve(m->user, a);
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void w8(const Mt2IcfgDlg* m, uint32_t a, uint32_t v) { *m->resolve(m->user, a) = (uint8_t)v; }
static void w32(const Mt2IcfgDlg* m, uint32_t a, uint32_t v)
{
    uint8_t* p = m->resolve(m->user, a);
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t c2(const Mt2IcfgDlg* m, uint32_t fn, uint32_t eax, uint32_t edx)
{
    return m->call(m->user, fn, eax, edx, 0, 0, NULL, 0);
}
static void text(const Mt2IcfgDlg* m, uint32_t widget, uint32_t s) { c2(m, 0x004a6f08, widget, s); }

#define SEQ_OK 0x004e7554u
#define SEQ_BAD 0x004e7558u

/* the play / flush / commit sequence on the sound object 0x00626010 (id 0x18 accept, 0x3f reject) */
static void sound(const Mt2IcfgDlg* m, uint32_t id, uint32_t seq)
{
    uint32_t obj = r32(m, 0x00626010), vt = r32(m, obj + 0x1c), n = r32(m, seq);
    uint32_t st[3] = { 0x63, n, 0 };
    w32(m, seq, n + 1);
    m->call(m->user, r32(m, vt), obj, id, 0, 0, st, 3);
    c2(m, 0x0047314c, r32(m, 0x00626010), 0);
    obj = r32(m, 0x00626010);
    c2(m, r32(m, r32(m, obj + 0x1c) + 4), obj, 0);
}

uint32_t mt2_icfg_dlg_ctor(const Mt2IcfgDlg* m, uint32_t dlg)
{
    uint32_t r = c2(m, 0x004a9dd0, dlg, 1);
    int k;
    r = c2(m, 0x004a4264, r + 0x6c, 0);
    r = c2(m, 0x004a4264, r + 0xc0, 0);
    r += 0xc0;
    for (k = 0; k < 5; k++) {
        r = c2(m, 0x004a3fa0, r, 0) + 0x54;
        w32(m, r - 0x20, 0x004e50fc);
    }
    r -= 0x54 + 0x33c;
    w32(m, r + 0x34, 0x004e5008);
    w32(m, r + 0x58, 1);
    w32(m, r + 0x5c, 0xffffffffu);
    w32(m, r + 0x60, 0xffffffffu);
    return r;
}

uint32_t mt2_icfg_dlg_dtor(const Mt2IcfgDlg* m, uint32_t dlg, uint32_t flags)
{
    uint32_t r;
    int k, j;
    if (flags & 4) {
        r = c2(m, 0x004a9ff8, dlg, 0x004e4fe0);
        c2(m, 0x004aa018, r, 0);
        return dlg;
    }
    w32(m, dlg + 0x34, 0x004e5008);
    r = c2(m, 0x004a3ff4, dlg + 0x33c, 0);
    for (k = 0; k < 4; k++)
        r = c2(m, 0x004a3ff4, r - 0x54, 0);
    for (j = 0; j < 2; j++) {
        r = c2(m, 0x004aa064, r - 0x18, 0);
        for (k = 0; k < 3; k++)
            r = c2(m, 0x004aa064, r - 0x14, 0);
        r = c2(m, 0x004a3ff4, r - 0x6c, 0);
    }
    r = c2(m, 0x004a3ff4, r - 0x6c, 1);
    if (flags & 2)
        c2(m, 0x004aa39c, r, 0);
    return r;
}

void mt2_icfg_dlg_read_keyboard(const Mt2IcfgDlg* m, uint32_t dlg)
{
    uint32_t k, i;
    w8(m, dlg + 0x65, c2(m, 0x004a3e24, 0x0050ea98, 0));
    c2(m, 0x004a3d88, 0x0050ea98, 0);
    k = r8(m, dlg + 0x65);
    if (k != 0 && k != r8(m, dlg + 0x69)) {
        if (r8(m, 0x0050e5cc + k) != 0) {
            sound(m, 0x18, SEQ_OK);
            w8(m, dlg + 0x68, r8(m, dlg + 0x65));
        } else {
            sound(m, 0x3f, SEQ_BAD);
            text(m, dlg + 0x1ec, 0x004e0aa6);
        }
        if (r8(m, dlg + 0x65) == 1) {
            sound(m, 0x18, SEQ_OK);
            w8(m, dlg + 0x68, 0);
        }
    }
    k = r8(m, dlg + 0x68);
    if (k != r8(m, dlg + 0x69)) {
        text(m, dlg + 0x1ec, r32(m, 0x004e755c + 4 * k));
        text(m, dlg + 0x33c, r32(m, 0x004e755c + 4 * r8(m, dlg + 0x68)));
        w8(m, dlg + 0x69, r8(m, dlg + 0x68));
        w32(m, dlg + 0x60, r8(m, dlg + 0x68));
        c2(m, 0x004a1f6c, r8(m, 0x004e7a48 + r32(m, dlg + 0x5c) * 8), r8(m, dlg + 0x60));
    }
    for (i = 0; (int32_t)i < 16; i++) {
        if (r8(m, 0x0050e11d + i * 16) == r32(m, dlg + 0x60) && i != r32(m, dlg + 0x5c)) {
            /* "Conflict with " + the other row's name, built in a 256 byte stack buffer */
            char buf[256];
            size_t n = 0;
            uint32_t s = 0x004e0ac2, t = r32(m, 0x004e7a44 + i * 8);
            while ((buf[n] = (char)r8(m, s++)) != 0)
                n++;
            while ((buf[n] = (char)r8(m, t++)) != 0)
                n++;
            m->text_local(m->user, dlg + 0x1ec, buf);
            i = 16;
        }
    }
}

/* 0x00462500 and 0x004627c4: one body, different device tables */
typedef struct {
    uint32_t getter, field, tab0, tabdec, wrap, label, names, bound, conflict;
    int getter_takes_key;
} Dev;

static void read_dev(const Mt2IcfgDlg* m, uint32_t dlg, const Dev* d)
{
    uint32_t c, v, i;
    c2(m, 0x00493748, dlg, 0);
    c = c2(m, 0x004a2068, 0, 0);
    w8(m, dlg + 0x65, c);
    w8(m, dlg + d->field, d->getter_takes_key ? c2(m, d->getter, c, 0) : m->call(m->user, d->getter, 0, 0, 0, 0, NULL, 0));
    c = r8(m, dlg + 0x65);
    if (c == 0) {
        v = r8(m, dlg + d->field);
        if (v != 0 && v != r8(m, dlg + 0x69)) {
            if (r8(m, d->tab0 + v) != 0) {
                sound(m, 0x18, SEQ_OK);
                w8(m, dlg + 0x68, r8(m, dlg + d->field));
            } else {
                sound(m, 0x3f, SEQ_BAD);
                text(m, dlg + 0x1ec, d->label);
            }
        }
    } else if (c == 1) {
        if (r8(m, dlg + 0x69) != 0) {
            sound(m, 0x18, SEQ_OK);
            w8(m, dlg + 0x68, 0);
        }
    } else if (c == 0xc8 || c == 0xcb) {
        sound(m, 0x18, SEQ_OK);
        do {
            v = r8(m, dlg + 0x68);
            w8(m, dlg + 0x68, v > 0 ? v - 1 : d->wrap);
        } while (r8(m, d->tabdec + r8(m, dlg + 0x68)) == 0);
    } else if (c == 0xd0 || c == 0xcd || c == 0x0f) {
        sound(m, 0x18, SEQ_OK);
        /* ORIGINAL BUG (0x004627c4): steps up through the device 4 table 0x0050e6d4 and wraps at
         * 50 even for device 2, whose down step uses 0x0050e6cc and wraps at 7 */
        do {
            v = (r8(m, dlg + 0x68) + 1) & 0xff;
            w8(m, dlg + 0x68, v);
            if (v >= 0x32)
                w8(m, dlg + 0x68, 0);
        } while (r8(m, 0x0050e6d4 + r8(m, dlg + 0x68)) == 0);
    } else {
        sound(m, 0x3f, SEQ_BAD);
    }
    v = r8(m, dlg + 0x68);
    if (v != r8(m, dlg + 0x69)) {
        text(m, dlg + 0x33c, r32(m, d->names + 4 * v));
        w8(m, dlg + 0x69, r8(m, dlg + 0x68));
        w32(m, dlg + 0x60, r8(m, dlg + 0x68));
    }
    /* ORIGINAL BUG: the conflict test reads the dialog's own row (+0x5c) every pass instead of
     * row i (the keyboard reader 0x004622fc reads row i): it compares the row with itself and,
     * when the row's own entry equals the code, sets the conflict text 15 times */
    for (i = 0; i < 16; i++) {
        if (r8(m, d->bound + r32(m, dlg + 0x5c) * 16) == r32(m, dlg + 0x60) && i != r32(m, dlg + 0x5c))
            text(m, dlg + 0x1ec, d->conflict);
    }
}

void mt2_icfg_dlg_read_dev4(const Mt2IcfgDlg* m, uint32_t dlg)
{
    static const Dev d = { 0x004a2828, 0x66, 0x0050e6d4, 0x0050e6d4, 0x31, 0x004e0ad1, 0x004e797c,
                           0x0050e21d, 0x004e0aed, 0 };
    read_dev(m, dlg, &d);
}

void mt2_icfg_dlg_read_dev2(const Mt2IcfgDlg* m, uint32_t dlg)
{
    static const Dev d = { 0x004a22e4, 0x67, 0x0050e6cc, 0x0050e6cc, 7, 0x004e0af6, 0x004e795c,
                           0x0050e31d, 0x004e0b12, 1 };
    read_dev(m, dlg, &d);
}

uint32_t mt2_icfg_dlg_build(const Mt2IcfgDlg* m, uint32_t dlg)
{
    static const uint32_t ctl[5][3] = { { 0x1ec, 0xffffffffu, 0x004e0b1c }, { 0x240, 0x3e8, 0x004e0b1d },
                                        { 0x294, 0x3eb, 0x004e0b1e }, { 0x2e8, 0x3e9, 0x004e0b1f },
                                        { 0x33c, 0x3ea, 0x004e0b20 } };
    const uint32_t big = 0x80000000u;
    uint32_t cr[5] = { 0, big, big, big, big };
    uint32_t a1[3] = { 0x00462f60, 0, 0 }, a2[3] = { 0x00462f50, 0, 0 };
    uint32_t f, kind, names, prompt;
    int k;
    /* the OK / cancel buttons (0x004a4354, then their handlers 0x00462d90 / 0x00462d80 by
     * 0x004a4458); the callee leaves ebx / ecx changed, 0x004a4458 reads neither */
    m->call(m->user, 0x004a4354, dlg + 0x6c, r32(m, dlg), 1, 0x004e0b1b, cr, 5);
    m->call(m->user, 0x004a4458, dlg + 0x6c, dlg, 0, 0, a1, 3);
    m->call(m->user, 0x004a4354, dlg + 0x12c, r32(m, dlg), 2, 0x004e0b1b, cr, 5);
    m->call(m->user, 0x004a4458, dlg + 0x12c, dlg, 0, 0, a2, 3);
    for (k = 0; k < 5; k++)
        m->call(m->user, 0x004a60bc, dlg + ctl[k][0], r32(m, dlg), ctl[k][1], ctl[k][2], cr, 5);
    f = c2(m, 0x004a8fe8, r32(m, dlg + 0x58), r8(m, 0x004e7a48 + r32(m, dlg + 0x5c) * 8)) & 0xff;
    w8(m, dlg + 0x64, f);
    w8(m, dlg + 0x68, f);
    w8(m, dlg + 0x69, f);
    c2(m, 0x004a4180, dlg + 0x33c, 0xffffff);
    c2(m, 0x004a4174, dlg + 0x33c, 0xffffff);
    c2(m, 0x004a4168, dlg + 0x33c, 0);
    kind = r32(m, dlg + 0x58);
    if (kind == 1 || kind == 4 || kind == 2) {
        prompt = kind == 1 ? 0x004e0b21 : kind == 4 ? 0x004e0b5b : 0x004e0bda;
        names = kind == 1 ? 0x004e755c : kind == 4 ? 0x004e797c : 0x004e795c;
        text(m, dlg + 0x240, prompt);
        text(m, dlg + 0x2e8, r32(m, names + 4 * r8(m, dlg + 0x64)));
        text(m, dlg + 0x33c, r32(m, names + 4 * r8(m, dlg + 0x64)));
    }
    text(m, dlg + 0x294, r32(m, 0x004e7a44 + r32(m, dlg + 0x5c) * 8));
    text(m, dlg + 0x1ec, 0x004e0c53);
    return 1;
}

uint32_t mt2_icfg_dlg_ok(const Mt2IcfgDlg* m, uint32_t dlg)
{
    uint32_t kind = r32(m, dlg + 0x58), lab = 0, names = 0, tab = 0;
    if (kind == 1) {
        lab = 0x0050e120; names = 0x004e755c; tab = 0x0050e11d;
    } else if (kind == 2) {
        lab = 0x0050e320; names = 0x004e795c; tab = 0x0050e31d;
    } else if (kind == 4) {
        lab = 0x0050e220; names = 0x004e797c; tab = 0x0050e21d;
    }
    if (lab) {
        text(m, r32(m, lab + r32(m, dlg + 0x5c) * 16), r32(m, names + r32(m, dlg + 0x60) * 4));
        w8(m, tab + r32(m, dlg + 0x5c) * 16, r8(m, dlg + 0x60));
    }
    c2(m, r32(m, r32(m, dlg + 0x34) + 0xc8), dlg, 0);
    return 0;
}
