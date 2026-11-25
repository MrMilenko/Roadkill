/*
 * mt2_netui.c: the network setup screen (see mt2_netui.h).
 */
#include "mt2_netui.h"

#include <stdarg.h>
#include <string.h>

enum { W_LIST, W_NAME, W_SIGN, W_B1, W_B2, W_B3 };

static uint32_t r32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void w32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }

static uint32_t call(const Mt2NetUi* u, uint32_t addr, int n, ...) {
    uintptr_t a[12];
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) a[i] = va_arg(ap, uintptr_t);
    va_end(ap);
    return u->call(u->user, addr, a, n);
}
#define C(addr, ...) call(u, (addr), (int)(sizeof((uintptr_t[]){__VA_ARGS__}) / sizeof(uintptr_t)), __VA_ARGS__)
#define C0(addr) u->call(u->user, (addr), 0, 0)
#define P(x) ((uintptr_t)(x))

/* vtable call: fn = [[w + 0x34] + off], (this, edx) */
static void vt(const Mt2NetUi* u, uint32_t w, uint32_t off, uint32_t edx) {
    uint32_t fn = r32(u->resolve(u->user, r32(u->resolve(u->user, w) + 0x34)) + off);
    C(fn, w, edx);
}

static uint32_t sess_str(const Mt2NetUi* u, uint32_t fn, uint32_t empty, uint32_t dflt) {
    uint32_t s = C(fn, *u->session);
    if (C(0x00c90318u, 0, 0, 0, 0, s, empty) == 0) return dflt;     /* lstrcmpA(s, "") */
    return C(fn, *u->session);
}

void mt2_netui_build(const Mt2NetUi* u) {
    uint32_t key = 0, size;
    uint8_t buf[0x80] = {0};
    uint32_t* w = u->w;
    C(0x004ac4b0u, P(&key), 0);
    uint32_t mgr = C0(0x004a8f34u);
    uint32_t a = C(0x004a8f48u, 0x7c);
    if (a) a = C(0x004a5290u, a, mgr, 0x79, 0x200000, 0x12, 0x75, 0x123, 0xa4);
    w[W_LIST] = a;
    uint32_t s = sess_str(u, 0x004988c8u, 0x004e1406u, u->defaults[0]);
    a = C(0x004a8f48u, 0x68);
    if (a) a = C(0x004a5f40u, a, mgr, 0x97, s, 0, 0x12, 0x137, 0x123, 0x12);
    w[W_NAME] = a;
    s = sess_str(u, 0x004988e0u, 0x004e1407u, u->defaults[1]);
    a = C(0x004a8f48u, 0x68);
    if (a) a = C(0x004a5f40u, a, mgr, 0xa1, s, 0, 0x12, 0x168, 0x123, 0x12);
    w[W_SIGN] = a;
    static const uint32_t bid[3] = {0xb4, 0xb5, 0xb6}, bx[3] = {0x10, 0xfa, 0x1e4};
    for (int i = 0; i < 3; i++) {
        a = C(0x004a8f48u, 0x1cc);
        if (a) a = C(0x004a490cu, a, mgr, bid[i], 0, 1, bx[i], 0x1c1, 0x8c, 0x17);
        w[W_B1 + i] = a;
    }
    C(0x004a4bdcu, w[W_B1], 0x7b, 0x79, 0, 0x7a);
    C(0x004a4bdcu, w[W_B2], 0x7d, 0x7c, 0, 0xd1);
    C(0x004a4bdcu, w[W_B3], 0x78, 0x77, 0, 0);
    for (int i = W_LIST; i <= W_SIGN; i++) {
        vt(u, w[i], 0xb0, 0xffffff);
        vt(u, w[i], 0xb4, 0);
        vt(u, w[i], 0xb8, 0);
    }
    for (int i = W_B1; i <= W_B3; i++) C(0x004a4de0u, w[i], 0xcc0ff, 0xc0c0c);
    vt(u, w[W_B1], 0xe0, 0x00465a3cu);
    vt(u, w[W_B2], 0xe0, 0x00465b7cu);
    vt(u, w[W_B3], 0xe0, 0x00465cbcu);
    vt(u, w[W_LIST], 0xcc, 0x00465cd4u);
    vt(u, w[W_LIST], 0xd4, 0x00465cd4u);
    if (C(0x004ac528u, P(&key), 0x80000002u, 0) == 0) {
        size = 0x80;
        if (C(0x004ac5e4u, P(&key), 0x004e1408u, P(buf), P(&size)) == 0) C(0x004a6f08u, w[W_NAME], P(buf));
        size = 0x80;
        if (C(0x004ac5e4u, P(&key), 0x004e1416u, P(buf), P(&size)) == 0) C(0x004a6f08u, w[W_SIGN], P(buf));
    }
    C(0x004ac590u, P(&key));
    C(0x004a70acu, w[W_LIST], 0, 0, 0, 0, w[W_B3], w[W_NAME]);
    C(0x004a70acu, w[W_NAME], w[W_LIST], w[W_SIGN], 0, 0, w[W_LIST], w[W_SIGN]);
    C(0x004a70acu, w[W_SIGN], w[W_NAME], w[W_B1], 0, 0, w[W_NAME], w[W_B1]);
    C(0x004a70acu, w[W_B1], w[W_SIGN], w[W_LIST], w[W_B3], w[W_B2], w[W_SIGN], w[W_B2]);
    C(0x004a70acu, w[W_B2], w[W_SIGN], w[W_LIST], w[W_B1], w[W_B3], w[W_B1], w[W_B3]);
    C(0x004a70acu, w[W_B3], w[W_SIGN], w[W_LIST], w[W_B2], w[W_B1], w[W_B2], w[W_LIST]);
    C0(0x004a8e6cu);
    for (int i = W_LIST; i <= W_B3; i++) C(0x004a6ee4u, w[i], 5);
    C(0x004a6f80u, C(0x004a6e0cu, w[W_B1]) ? w[W_B1] : w[W_B3]);
}

void mt2_netui_free(const Mt2NetUi* u) {
    C(0x004a5630u, u->w[W_LIST]);
    for (int i = W_LIST; i <= W_B3; i++)
        if (u->w[i]) {
            uint32_t fn = r32(u->resolve(u->user, r32(u->resolve(u->user, u->w[i]) + 0x34)));
            C(fn, u->w[i], 2);
        }
}

void mt2_netui_select(const Mt2NetUi* u, uint32_t list) {
    uint8_t buf[0x80];
    if ((int32_t)C(0x004a54b4u, list) <= 0) return;
    int32_t i = (int32_t)C(0x004a54ccu, list);
    if (i < 0) return;
    if (i >= (int32_t)C(0x004ace88u, *u->enumr)) return;
    C(0x004a6dd8u, u->w[W_B1], 1);
    C(0x004a6e2cu, u->w[W_B1], 0, 0);
    C(0x004a6dd8u, u->w[W_B2], 1);
    C(0x004a6e2cu, u->w[W_B2], 0, 0);
    *u->provider = C(0x004a5394u, list);
    C(0x004a559cu, list, (uint32_t)i, P(buf));
}

void mt2_netui_connect(const Mt2NetUi* u, uint32_t result2, uint32_t icon) {
    uint8_t text[0x80], name[0x80];
    uint32_t key = 0;
    *u->result = 1;
    *u->result2 = result2;
    C(0x004a6ef4u, u->w[W_NAME], P(text), 0x80);
    C(0x004988b4u, *u->session, P(text), 0x80);
    C(0x004a6ef4u, u->w[W_SIGN], P(text), 0x80);
    C(0x004988d0u, *u->session, P(text));
    int32_t r = (int32_t)C(0x00499884u, icon);
    w32(u->resolve(u->user, *u->session) + 0x40, r != -1);
    C(0x004ac4b0u, P(&key), 0);
    uint32_t sel = C(0x004a54ccu, u->w[W_LIST]);
    *u->provider = C(0x004a5394u, u->w[W_LIST]);
    if (C(0x004a91b0u, *u->provider + 4, 0x004e1428u) == 0) {
        uint32_t h = C0(0x004a8f34u);
        C(0x00c90524u, 0, 0, 0, 0, h, 0x004e145cu, 0x004e1449u, 0x30);    /* MessageBoxA */
    }
    if (*u->provider != 0) {
        C(0x004a559cu, u->w[W_LIST], sel, P(name));
        C(0x004aa524u, 0x0050f9a8u, *u->provider);
        if (C(0x004ac528u, P(&key), 0x80000002u, 0) == 0) C(0x004ac638u, P(&key), 0x004e14d1u, P(name));
        C(0x004ac590u, P(&key));
    }
}

uint32_t mt2_netui_run(const Mt2NetUi* u) {
    uint32_t key = 0, size;
    uint8_t buf[0x80];
    C(0x004ac4b0u, P(&key), 0);
    u->bdfd20[0x14 / 4] = 0;
    u->bdfd20[0] = 0;
    u->bdfd20[0x18 / 4] = 0;
    u->bdfd20[1] = 0;
    C(0x004aa5b8u, 0x0050f9a8u);
    mt2_netui_build(u);
    *u->first = 1;
    *u->result = 0;
    *u->result2 = 0;
    do {
        if ((int32_t)C(0x004a54b4u, u->w[W_LIST]) < 1) {
            if (u->w[W_B1]) {
                C(0x004a6dd8u, u->w[W_B1], 0);
                C(0x004a6e2cu, u->w[W_B1], 0, 0);
            }
            if (u->w[W_B2]) {
                C(0x004a6dd8u, u->w[W_B2], 0);
                C(0x004a6e2cu, u->w[W_B2], 0, 0);
            }
        }
        if (*u->first != 0) {
            if ((int32_t)C(0x004ace8cu, *u->enumr, u->w[W_LIST]) > 0) {
                *u->first = 0;
            } else {
                C0(0x00496184u);
                uint32_t h = C0(0x004a8f34u);
                C(0x00c90524u, 0, 0, 0, 0, h, 0x004e1368u, 0x004e1348u, 0x30);    /* MessageBoxA */
                *u->result = 1;
                *u->result2 = 0;
            }
            if (C(0x004ac528u, P(&key), 0x80000002u, 0) == 0) {
                size = 0x80;
                if (C(0x004ac5e4u, P(&key), 0x004e13d6u, P(buf), P(&size)) == 0) {
                    uint32_t i = C(0x004a5470u, u->w[W_LIST], 0, P(buf));
                    if (i != 0xffffffffu) {
                        C(0x004a56f8u, u->w[W_LIST], i);
                        mt2_netui_select(u, u->w[W_LIST]);
                    }
                }
            }
            C(0x004ac590u, P(&key));
        }
        C0(0x00493748u);
        if (C(0x004936acu, 0x14, 0) & 0xff) { *u->result = 1; *u->result2 = 0; }
        if (C(0x004936acu, 0x15, 0) & 0xff) { *u->result2 = 1; *u->result = 1; }
        if (C(0x0049ecf4u, 0x2e) & 0xff) { *u->result = 1; *u->result2 = 0; }
        else if (C(0x0049ecf4u, 0x24) & 0xff) { *u->result = 1; *u->result2 = 2; }
        else if (C(0x0049ecf4u, 0x31) & 0xff) { *u->result = 1; *u->result2 = 3; }
    } while (*u->result != 1);
    if (*u->result2 == 0 || *u->result2 == 1) {
        C(0x004aa5b8u, 0x0050f9a8u);
    } else {
        if (C(0x004ac528u, P(&key), 0x80000002u, 0) == 0) {
            C(0x004a6ef4u, u->w[W_NAME], P(buf), 0x80);
            C(0x004ac638u, P(&key), 0x004e13e6u, P(buf));
            C(0x004a6ef4u, u->w[W_SIGN], P(buf), 0x80);
            C(0x004ac638u, P(&key), 0x004e13f4u, P(buf));
        }
        C(0x004ac590u, P(&key));
    }
    mt2_netui_free(u);
    return *u->result2;
}
