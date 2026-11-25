/*
 * pf8_netdlg.c: TM2.EXE 0x004662b8, an unlisted function (not in Ghidra's function list; reached from
 * 0x00465f60, tools/rt/unlisted.tsv): builds the network game dialog's controls. Ported over Pf2Env
 * (pf2_env.h); emulator diff: tools/emu/check_pf8_netdlg.sh.
 *
 * Registers: every call gets the four registers (eax, edx, ebx, ecx) as the original has them at that
 * point, including the values left from earlier instructions (the callees keep their registers), so the
 * port keeps the original's register values in locals (a, d, b, c, si, di) and follows them.
 * Globals: 0x0050fa24 network flag (cleared here, read later), 0x0050fa64 the image list,
 * 0x0050fa2c..0x0050fa38 its four bitmap indices, 0x0050fa3c..0x0050fa60 the control objects, 0x0050f9a8
 * the session object (0x004aaa5c(it) != 0: the host side), 0x0050f9d8 the settings object.
 * Control objects: +0x34 the vtable (slots 0xb0 / 0xb4 / 0xb8 colors and style, 0xe0 the event
 * callback, which is given the addresses of the handlers 0x004672fc..0x004676d0).
 */
#include "pf2_env.h"
#include "pf8_netdlg.h"

#define R32 pf2_r32
#define W32 pf2_w32

typedef struct { Pf2Env* e; uint32_t a, d, b, c; } Regs;

static uint32_t call_n(Regs* r, uint32_t fn, int ns, const uint32_t* st) {
    uint32_t v[12] = {r->a, r->d, r->b, r->c};
    for (int k = 0; k < ns; k++) v[4 + k] = st[k];
    r->a = r->e->call(r->e->user, fn, v, 4 + ns);
    return r->a;
}
#define CALL(fn) call_n(&R, (fn), 0, NULL)
#define CALLS(fn, ...) do { uint32_t st_[] = {__VA_ARGS__}; call_n(&R, (fn), (int)(sizeof st_ / 4), st_); } while (0)
/* an import (stdcall words only) */
#define API(fn, ...) do { uint32_t st_[] = {__VA_ARGS__}; R.a = e->call(e->user, (fn), st_, (int)(sizeof st_ / 4)); } while (0)
/* a virtual call through the object's vtable (+0x34) */
#define VCALL(slot) (R.b = R32(e, R.a + 0x34), CALL(R32(e, R.b + (slot))))

#define I_ImageList_Create 0x00c907fcu
#define I_ImageList_Add 0x00c907f8u
#define I_LoadBitmapA 0x00c90508u
#define F_HINSTANCE 0x004a8f40u
#define F_NEW 0x004a8f48u
#define F_HOST 0x004aaa5cu
#define SESSION 0x0050f9a8u

/* the three color / style slots of a control: 0xb0(0xffffff), 0xb4(0), 0xb8(0) */
static void style3(Pf2Env* e, Regs* rr, uint32_t g) {
    Regs R = *rr;
    R.a = R32(e, g);
    R.d = 0xffffff;
    VCALL(0xb0);
    R.a = R32(e, g);
    R.d = 0;
    VCALL(0xb4);
    R.a = R32(e, g);
    R.d = 0;
    VCALL(0xb8);
    *rr = R;
}

/* a button: new(0x1cc), 0x004a490c(obj, parent, id, 0; x, y, w, h, kind) -> global */
static void button(Pf2Env* e, Regs* rr, uint32_t si, uint32_t id, uint32_t g, uint32_t s0, uint32_t s1, uint32_t s2,
                   uint32_t s3, uint32_t s4) {
    Regs R = *rr;
    R.a = 0x1cc;
    CALL(F_NEW);
    if (R.a) {
        R.b = id;
        R.d = si;
        R.c = 0;
        CALLS(0x004a490cu, s0, s1, s2, s3, s4);
    }
    W32(e, g, R.a);
    *rr = R;
}

/* 0x004a4de0(obj, 0xcc0ff, 0xc0c0c): the button colors */
static void colors(Pf2Env* e, Regs* rr, uint32_t g) {
    Regs R = *rr;
    R.b = 0xc0c0c;
    R.d = 0xcc0ff;
    R.a = R32(e, g);
    CALL(0x004a4de0u);
    *rr = R;
}

/* 0x004a4bdc(obj, edx, ebx, 0; stack s): the button images */
static void images(Pf2Env* e, Regs* rr, uint32_t g, uint32_t d, uint32_t b, uint32_t s) {
    Regs R = *rr;
    R.b = b;
    R.d = d;
    R.a = R32(e, g);
    R.c = 0;
    CALLS(0x004a4bdcu, s);
    *rr = R;
}

/* list view column: 0x004a6480(list, 0, ebx width, ecx text; stack index) */
static void column(Pf2Env* e, Regs* rr, uint32_t g, uint32_t idx, uint32_t text, uint32_t w) {
    Regs R = *rr;
    R.c = text;
    R.b = w;
    R.a = R32(e, g);
    R.d = 0;
    CALLS(0x004a6480u, idx);
    *rr = R;
}

/* the tab order links 0x004a70ac(a, d, b, c; s0, s1, s2) with the controls as given */
static void link(Pf2Env* e, Regs* rr, uint32_t a, uint32_t d, uint32_t b, uint32_t c, uint32_t s0, uint32_t s1,
                 uint32_t s2) {
    Regs R = *rr;
    R.a = R32(e, a);
    R.d = R32(e, d);
    R.b = R32(e, b);
    R.c = c ? R32(e, c) : 0;
    CALLS(0x004a70acu, s0 ? R32(e, s0) : 0, R32(e, s1), R32(e, s2));
    *rr = R;
}

#define G(x) (0x0050fa00u + (x))

uint32_t pf8_net_dialog_build(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t si, di, tmp = e->scratch;
    CALL(0x004a8f34u);                                      /* the dialog window */
    di = si = R.a;
    R.d = 0;
    W32(e, G(0x24), 0);
    API(I_ImageList_Create, 0x14, 0x14, 0x18, 4, 1);
    W32(e, G(0x64), R.a);
    static const uint32_t bm[4][2] = {{0x86, 0x2c}, {0xad, 0x30}, {0x84, 0x34}, {0x85, 0x38}};
    for (int k = 0; k < 4; k++) {
        CALL(F_HINSTANCE);
        API(I_LoadBitmapA, R.a, bm[k][0]);
        if (k == 0) R.b = R32(e, G(0x64));
        else if (k == 1) R.c = R32(e, G(0x64));
        API(I_ImageList_Add, R32(e, G(0x64)), R.a, 0);
        W32(e, G(bm[k][1]), R.a);
    }
    /* the player list (host and client the same here) */
    R.a = SESSION;
    CALL(F_HOST);
    R.a = 0x54;
    CALL(F_NEW);
    if (R.a) {
        R.c = 0x4009;
        R.b = 0x97;
        R.d = di;
        CALLS(0x004a616cu, 0x12, 0x6d, 0x259, 0x8e);
    }
    W32(e, G(0x40), R.a);
    style3(e, &R, G(0x40));
    /* the chat box */
    R.a = 0x68;
    CALL(F_NEW);
    if (R.a) {
        R.c = 0x004e1583u;
        R.b = 0xab;
        R.d = si;
        CALLS(0x004a5f40u, 0, 0x12, 0x137, 0x186, 0x12);
    }
    W32(e, G(0x3c), R.a);
    style3(e, &R, G(0x3c));
    /* the settings list */
    R.a = 0x54;
    CALL(F_NEW);
    if (R.a) {
        R.c = 0x4001;
        R.b = 0xac;
        R.d = si;
        CALLS(0x004a616cu, 0x12, 0x16b, 0x25c, 0x4c);
    }
    W32(e, G(0x44), R.a);
    style3(e, &R, G(0x44));
    /* the buttons */
    R.a = SESSION;
    CALL(F_HOST);
    int host = R.a != 0;
    if (host) {
        button(e, &R, si, 0xb4, G(0x50), 1, 0x10, 0x1c1, 0x8c, 0x17);
        button(e, &R, si, 0xb9, G(0x48), 1, 0x10, 0x105, 0x8c, 0x17);
        button(e, &R, si, 0xba, G(0x4c), 1, 0x1e1, 0x105, 0x8c, 0x17);
        colors(e, &R, G(0x50));
        colors(e, &R, G(0x48));
        colors(e, &R, G(0x4c));
        if (R32(e, G(0x24)) == 1) images(e, &R, G(0x4c), 0x9c, 0x9b, 0);
        else images(e, &R, G(0x4c), 0x8e, 0x8d, 0);
        images(e, &R, G(0x50), 0x92, 0x90, 0x91);
        images(e, &R, G(0x48), 0x96, 0x95, 0);
    } else {
        button(e, &R, si, 0xb4, G(0x50), 1, 0x10, 0x1c1, 0x8c, 0x17);
        R.b = 0xc0c0c;
        R.d = 0xcc0ff;
        CALL(0x004a4de0u);                                  /* eax: the new button (still in eax) */
        images(e, &R, G(0x50), 0x94, 0x93, 0);
    }
    button(e, &R, si, 0xb5, G(0x54), 0xb, 0xfa, 0x1c1, 0x8c, 0x17);
    button(e, &R, si, 0xb6, G(0x58), 0xb, 0x1e4, 0x1c1, 0x8c, 0x17);
    button(e, &R, si, 0xb7, G(0x5c), 0xb, 0x1a3, 0x135, 0x60, 0x17);
    button(e, &R, si, 0xb8, G(0x60), 0xb, 0x20c, 0x135, 0x60, 0x17);
    colors(e, &R, G(0x54));
    colors(e, &R, G(0x58));
    colors(e, &R, G(0x5c));
    R.c = 0;
    colors(e, &R, G(0x60));
    images(e, &R, G(0x54), 0x76, 0x75, 0);
    images(e, &R, G(0x58), 0x78, 0x77, 0);
    images(e, &R, G(0x5c), 0x9a, 0x99, 0);
    images(e, &R, G(0x60), 0x98, 0x97, 0);
    /* the event handlers */
    static const uint32_t ev[5][2] = {{0x50, 0x004672fcu}, {0x54, 0x00467594u}, {0x58, 0x004675a8u},
                                      {0x5c, 0x004675c0u}, {0x60, 0x0046761cu}};
    for (int k = 0; k < 5; k++) {
        R.a = R32(e, G(ev[k][0]));
        R.d = ev[k][1];
        VCALL(0xe0);
    }
    R.a = SESSION;
    CALL(F_HOST);
    if (R.a) {
        R.a = R32(e, G(0x4c));
        R.d = 0x00467638u;
        VCALL(0xe0);
        R.a = R32(e, G(0x48));
        R.d = 0x004676d0u;
        VCALL(0xe0);
    }
    /* the player list columns (host / client texts) */
    R.a = SESSION;
    CALL(F_HOST);
    if (R.a) {
        column(e, &R, G(0x40), 0, 0x004e1584u, 0xcb);
        column(e, &R, G(0x40), 1, 0x004e1590u, 0xc9);
        column(e, &R, G(0x40), 2, 0x004e159du, 0x98);
        R.c = 0x004e15abu;
    } else {
        R.c = 0x004e15aeu;
        R.b = 0xcb;
        uint32_t z = R.a;                                   /* eax (0) is the index pushed */
        R.d = 0;
        R.a = R32(e, G(0x40));
        CALLS(0x004a6480u, z);
        column(e, &R, G(0x40), 1, 0x004e15bau, 0xc9);
        column(e, &R, G(0x40), 2, 0x004e15c7u, 0x98);
        R.c = 0x004e15d5u;
    }
    R.b = 0x2a;
    R.a = R32(e, G(0x40));
    R.d = 0;
    CALLS(0x004a6480u, 3);
    column(e, &R, G(0x44), 0, 0x004e15d8u, 0x66);
    column(e, &R, G(0x44), 1, 0x004e15deu, 0x192);
    column(e, &R, G(0x44), 2, 0x004e15e7u, 0x64);
    /* the settings rows: 0x004a63d8 adds a row, 0x004a64c4 / 0x004a6564 set its cells */
    R.a = R32(e, G(0x44));
    R.d = 0x004e15ebu;
    CALL(0x004a63d8u);
    R.c = 0;
    di = R.a;
    R.a = R32(e, G(0x44));
    R.b = di;
    CALL(0x004a64c4u);
    R.c = 1;
    R.d = 0x004e15ecu;
    R.a = R32(e, G(0x44));
    R.b = di;
    CALL(0x004a6564u);
    R.c = 2;
    R.d = 0x004e1609u;
    si = di + 1;
    R.a = R32(e, G(0x44));
    R.b = di;
    CALL(0x004a6564u);
    R.d = 0x004e160du;
    R.a = R32(e, G(0x44));
    R.b = si;
    R.c = 0;
    CALL(0x004a64c4u);
    R.a = R32(e, 0x0050f9d8u);
    R.c = 1;
    CALL(0x004ac040u);
    R.b = si;
    di = R32(e, G(0x44));
    R.d = R.a;
    R.a = di;
    CALL(0x004a6564u);
    R.c = 2;
    R.d = 0x004e161au;
    R.a = R32(e, G(0x44));
    R.b = si;
    CALL(0x004a6564u);
    R.d = 0x004e161eu;
    R.a = R32(e, G(0x44));
    si++;
    R.c = 0;
    R.b = si;
    CALL(0x004a64c4u);
    R.a = R32(e, 0x0050f9d8u);
    R.c = 1;
    CALL(0x004ac0acu);
    R.d = (uint32_t)(int32_t)(int8_t)(R.a & 0xff);
    R.a = 0x00624ea8u;
    R.b = si;
    CALL(0x00474b8cu);
    di = R32(e, G(0x44));
    R.d = R.a;
    R.a = di;
    CALL(0x004a6564u);
    R.c = 2;
    R.d = 0x004e162cu;
    R.a = R32(e, G(0x44));
    R.b = si;
    CALL(0x004a6564u);
    R.d = 0x004e1630u;
    R.a = R32(e, 0x0050f9d8u);
    si++;
    CALL(0x004ac0b8u);
    R.c = 0;
    di = R.a;
    R.b = si;
    R.a = R32(e, G(0x44));
    CALL(0x004a64c4u);
    uint32_t txt = 0;
    if (di == 7) txt = 0x004e163au;
    else if (di == 6) txt = 0x004e164au;
    else if (di == 5) txt = 0x004e165au;
    if (txt) {
        R.c = 1;
        R.d = txt;
        R.a = R32(e, G(0x44));
        R.b = si;
        CALL(0x004a6564u);
    }
    R.c = 2;
    R.d = 0x004e166bu;
    R.a = R32(e, G(0x44));
    R.b = si;
    CALL(0x004a6564u);
    R.d = 0x004e166fu;
    R.a = R32(e, G(0x44));
    si++;
    R.c = 0;
    R.b = si;
    CALL(0x004a64c4u);
    R.b = 0xa;                                              /* itoa(8, buffer, 10) */
    R.d = tmp;
    R.a = 8;
    R.c = 1;
    CALL(0x004b27b8u);
    R.d = 0x004e167fu;
    R.a = R32(e, G(0x44));
    R.b = si;
    CALL(0x004a6564u);
    R.c = 2;
    R.d = 0x004e1681u;
    R.a = R32(e, G(0x44));
    R.b = si;
    CALL(0x004a6564u);
    /* the tab order */
    R.a = SESSION;
    CALL(F_HOST);
    if (R.a) {
        link(e, &R, G(0x40), G(0x50), G(0x48), 0, 0, G(0x58), G(0x48));
        link(e, &R, G(0x48), G(0x40), G(0x3c), G(0x4c), G(0x4c), G(0x40), G(0x4c));
        link(e, &R, G(0x4c), G(0x40), G(0x60), G(0x48), G(0x48), G(0x48), G(0x3c));
        link(e, &R, G(0x3c), G(0x48), G(0x50), G(0x60), G(0x5c), G(0x4c), G(0x5c));
        link(e, &R, G(0x5c), G(0x4c), G(0x58), G(0x3c), G(0x60), G(0x3c), G(0x60));
        link(e, &R, G(0x60), G(0x4c), G(0x58), G(0x5c), G(0x3c), G(0x5c), G(0x50));
        link(e, &R, G(0x50), G(0x3c), G(0x40), G(0x58), G(0x54), G(0x60), G(0x54));
        link(e, &R, G(0x54), G(0x3c), G(0x40), G(0x50), G(0x58), G(0x50), G(0x58));
        link(e, &R, G(0x58), G(0x60), G(0x40), G(0x54), G(0x50), G(0x54), G(0x40));
    } else {
        link(e, &R, G(0x40), G(0x50), G(0x3c), 0, 0, G(0x58), G(0x3c));   /* the first word is eax (0) */
        link(e, &R, G(0x3c), G(0x40), G(0x50), G(0x60), G(0x5c), G(0x40), G(0x5c));
        link(e, &R, G(0x5c), G(0x40), G(0x58), G(0x3c), G(0x60), G(0x3c), G(0x60));
        link(e, &R, G(0x60), G(0x40), G(0x58), G(0x5c), G(0x3c), G(0x5c), G(0x50));
        link(e, &R, G(0x50), G(0x3c), G(0x40), G(0x58), G(0x54), G(0x60), G(0x54));
        link(e, &R, G(0x54), G(0x3c), G(0x40), G(0x50), G(0x58), G(0x50), G(0x58));
        link(e, &R, G(0x58), G(0x60), G(0x40), G(0x54), G(0x50), G(0x54), G(0x40));
    }
    CALL(0x004a8e6cu);
    /* show: 0x004a6ee4(control, 5) for each (0x48 / 0x4c only on the host) */
    static const uint32_t s1[3] = {0x40, 0x3c, 0x44}, s2[2] = {0x48, 0x4c}, s3[5] = {0x50, 0x54, 0x58, 0x5c, 0x60};
    for (int k = 0; k < 3; k++) { R.d = 5; R.a = R32(e, G(s1[k])); CALL(0x004a6ee4u); }
    R.a = SESSION;
    CALL(F_HOST);
    if (R.a)
        for (int k = 0; k < 2; k++) { R.d = 5; R.a = R32(e, G(s2[k])); CALL(0x004a6ee4u); }
    for (int k = 0; k < 5; k++) { R.d = 5; R.a = R32(e, G(s3[k])); CALL(0x004a6ee4u); }
    R.a = R32(e, G(0x3c));
    CALL(0x004a6f80u);
    return R.a;
}
