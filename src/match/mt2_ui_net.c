/*
 * mt2_ui_net.c: network session dialog pieces 0x004670fc..0x00469a8c. See mt2_ui_net.h.
 */
#include "mt2_ui_net.h"

#include <string.h>

#define S(h) ((uintptr_t)n->resolve(n->user, (h)))

#define GBASE 0x0050f9a8u
#define SESSION 0x0050f9a8u

static uint32_t rd(const uint8_t* p, uint32_t o) { uint32_t v; memcpy(&v, p + o, 4); return v; }
static uint32_t G(const Mt2UiNet* n, uint32_t a) { return rd(n->g, a - GBASE); }
static void SETG(const Mt2UiNet* n, uint32_t a, uint32_t v) { memcpy(n->g + (a - GBASE), &v, 4); }

static uint32_t call(const Mt2UiNet* n, uint32_t fn, int k, uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4)
{
    uintptr_t a[5] = {a0, a1, a2, a3, a4};
    return n->call(n->user, fn, k, a);
}
#define C0(f) call(n, (f), 0, 0, 0, 0, 0, 0)
#define C1(f, a) call(n, (f), 1, (a), 0, 0, 0, 0)
#define C2(f, a, b) call(n, (f), 2, (a), (b), 0, 0, 0)
#define C3(f, a, b, c) call(n, (f), 3, (a), (b), (c), 0, 0)
#define C4(f, a, b, c, d) call(n, (f), 4, (a), (b), (c), (d), 0)

/* method of a widget (table at +0x34) or picture (+0x30) */
static uint32_t method(const Mt2UiNet* n, uint32_t obj, uint32_t tab, uint32_t off)
{
    return rd(n->resolve(n->user, rd(n->resolve(n->user, obj), tab)), off);
}

static void destroy(const Mt2UiNet* n, uint32_t a)
{
    uint32_t w = G(n, a);
    if (w != 0)
        C2(method(n, w, 0x34, 0), w, 2);
}

static void picture_free(const Mt2UiNet* n, uint32_t a)
{
    uint32_t p = G(n, a);
    C1(method(n, p, 0x30, 0xc), p);
}

static void list_add(const Mt2UiNet* n, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t w = G(n, 0x0050fa44);
    uint32_t i = C1(0x004a63d8, w);
    C4(0x004a64c4, w, a, i, 0);
    C4(0x004a6564, w, b, i, 1);
    C4(0x004a6564, w, c, i, 2);
    C3(0x004a625c, w, i, 0);
}

void mt2_ui_net_list_add(const Mt2UiNet* n, uint32_t a, uint32_t b)
{
    list_add(n, a, b, 0x004e168b);
}

void mt2_ui_net_list_add_player(const Mt2UiNet* n, uint32_t a, uint32_t b)
{
    uint32_t w = G(n, 0x0050fa44);
    uint32_t i = C1(0x004a63d8, w);
    C4(0x004a64c4, w, a, i, 0);
    C4(0x004a6564, w, b, i, 1);
    C4(0x004a6564, w, G(n, 0x0050f9dc) + 0x44, i, 2);
    C3(0x004a625c, w, i, 0);
}

static void join_restore(const Mt2UiNet* n)
{
    static const uint32_t wid[7] = {0x0050fab4, 0x0050fab8, 0x0050fabc, 0x0050fac4, 0x0050fac8, 0x0050fad0, 0x0050facc};
    static const int order[7] = {0, 1, 2, 3, 4, 6, 5};
    uint32_t hwnd = C0(0x004a8f34), h[7], p;
    int k;
    if (C1(0x00c90500, hwnd) == 0)
        return;
    for (k = 0; k < 7; k++)
        h[k] = C1(0x004a6dcc, G(n, wid[k]));
    C2(0x00c90584, hwnd, 9);
    C0(0x00485398);
    p = G(n, 0x0050fac0);
    C4(method(n, p, 0x30, 0x1c), p, 0x148, 0x52, 0);
    C0(0x00495b14);
    for (k = 0; k < 7; k++)
        C3(0x00c904f8, h[order[k]], 0, 1);
}

/* the same list selection step as 0x00469a8c (inline in 0x00468aa4) */
static void join_list_step(const Mt2UiNet* n)
{
    int32_t r = (int32_t)C1(0x004a54b4, G(n, 0x0050fab8));
    uint32_t s;
    if (r != -1 && r > 0) {
        r = (int32_t)C1(0x004a54cc, G(n, 0x0050fab8));
        if (r >= 0 && r < 0x32) {
            s = C1(0x004a5394, G(n, 0x0050fab8));
            SETG(n, 0x0050faa8, s);
            if (s == G(n, 0x0050faac)) {
                C3(0x00497d2c, s + 0x1b0, G(n, 0x0050fabc), 0);
                return;
            }
        }
    }
    C1(0x004a5630, G(n, 0x0050fabc));
}

void mt2_ui_net_join_restore_cancel(const Mt2UiNet* n)
{
    join_restore(n);
    SETG(n, 0x0050faa4, 0);
    SETG(n, 0x0050faa0, 1);
    C1(0x004abd38, G(n, 0x0050f9d8));
}

void mt2_ui_net_join_restore(const Mt2UiNet* n)
{
    join_restore(n);
}

void mt2_ui_net_join_destroy(const Mt2UiNet* n)
{
    static const uint32_t wid[7] = {0x0050fab4, 0x0050fab8, 0x0050fabc, 0x0050fac4, 0x0050fac8, 0x0050facc, 0x0050fad0};
    int k;
    C1(0x004a5630, G(n, 0x0050fab8));
    C1(0x004a5630, G(n, 0x0050fabc));
    for (k = 0; k < 7; k++)
        destroy(n, wid[k]);
    picture_free(n, 0x0050fac0);
}

void mt2_ui_net_join_go(const Mt2UiNet* n)
{
    char buf[0x80];
    SETG(n, 0x0050faa0, 1);
    SETG(n, 0x0050faa4, 4);
    C3(0x004a6ef4, G(n, 0x0050fab4), (uintptr_t)buf, 0x80);
    C2(0x004ac044, G(n, 0x0050f9d8), (uintptr_t)buf);
    C1(0x004abd38, G(n, 0x0050f9d8));
    if (C2(0x004aa7dc, SESSION, G(n, 0x0050faa8)) == 0) {
        *n->arena = (uint8_t)C1(0x004ac0ac, G(n, 0x0050f9d8));
        *n->game_type = (uint8_t)C1(0x004ac0b8, G(n, 0x0050f9d8));
    } else {
        SETG(n, 0x0050faa4, 0);
        SETG(n, 0x0050faa0, 1);
    }
}

void mt2_ui_net_join_select(const Mt2UiNet* n, uint32_t item)
{
    int32_t r;
    uint32_t s;
    if ((int32_t)C1(0x004a54b4, G(n, 0x0050fab8)) > 0) {
        C2(0x004a6dd8, G(n, 0x0050fac8), 1);
        C3(0x004a6e2c, G(n, 0x0050fac8), 0, 0);
    }
    s = C1(0x004a5394, item);
    SETG(n, 0x0050faa8, s);
    SETG(n, 0x0050faac, s);
    r = (int32_t)C1(0x004a54b4, G(n, 0x0050fab8));
    if (r != -1 && r > 0) {
        r = (int32_t)C1(0x004a54cc, G(n, 0x0050fab8));
        if (r >= 0 && r < 0x32) {
            s = C1(0x004a5394, G(n, 0x0050fab8));
            SETG(n, 0x0050faa8, s);
            if (s == G(n, 0x0050faac)) {
                C3(0x00497d2c, s + 0x1b0, G(n, 0x0050fabc), 0);
                C0(0x004693cc);
                return;
            }
        }
    }
    C1(0x004a5630, G(n, 0x0050fabc));
    C0(0x004693cc);
}

void mt2_ui_net_host_destroy(const Mt2UiNet* n)
{
    static const uint32_t wid[10] = {0x0050fa74, 0x0050fa78, 0x0050fa7c, 0x0050fa80, 0x0050fa84,
                                     0x0050fa88, 0x0050fa8c, 0x0050fa94, 0x0050fa98, 0x0050fa9c};
    uint32_t key[1];
    int k;
    C2(0x004ac4b0, (uintptr_t)key, 0);
    if (C3(0x004ac528, (uintptr_t)key, 0x80000002u, 0) == 0) {
        uint32_t mode = C1(0x004aaa70, SESSION);
        C3(0x004ac614, (uintptr_t)key, 0x004e1703, mode);
    }
    C1(0x004ac590, (uintptr_t)key);
    for (k = 0; k < 10; k++)
        destroy(n, wid[k]);
    picture_free(n, 0x0050fa90);
}

/* the picture of the join screen: table entry idx */
static void join_picture(const Mt2UiNet* n, char* path, uint32_t idx)
{
    uint32_t p, rect[4];
    C3(0x00499408, (uintptr_t)path, S(rd(n->resolve(n->user, 0x004e7b64 + 4 * idx), 0)), 1);
    p = G(n, 0x0050fac0);
    C1(method(n, p, 0x30, 0xc), p);
    p = G(n, 0x0050fac0);
    C3(method(n, p, 0x30, 0), p, (uintptr_t)path, 0);
    p = G(n, 0x0050fac0);
    C4(method(n, p, 0x30, 0x1c), p, 0x148, 0x52, 0);
    call(n, 0x00c90574, 5, (uintptr_t)rect, 0x148, 0x52, 0x276, 0x136);
    C3(0x004a6e2c, 0x00bdfcd0, (uintptr_t)rect, 1);
}

/* row 0 "None Selected" (compare text at cmp, set text at set), then the picture back to 0 */
static void join_none(const Mt2UiNet* n, char* b, uint32_t cmp, uint32_t set, int keep_if_same_arena)
{
    C3(0x004a559c, G(n, 0x0050fac4), 0, (uintptr_t)b);
    if (C2(0x004a91b0, (uintptr_t)b, S(cmp)) != 0) {
        C1(0x004a5630, G(n, 0x0050fac4));
        C3(0x004a55f4, G(n, 0x0050fac4), 0, S(set));
    }
    if (keep_if_same_arena) {
        if (G(n, 0x0050fab0) == rd(n->resolve(n->user, G(n, 0x0050faa8)), 0x1a4))
            return;
    } else if (G(n, 0x0050fab0) == 0) {
        return;
    }
    SETG(n, 0x0050fab0, 0);
    join_picture(n, b, 0);
}

/* rows 3 / 4: label + number */
static int join_count_row(const Mt2UiNet* n, char* b, char* b2, uint32_t row, uint32_t label, uint32_t off)
{
    char num[12];
    C3(0x004a559c, G(n, 0x0050fac4), row, (uintptr_t)b2);
    C3(0x004b27b8, rd(n->resolve(n->user, G(n, 0x0050faa8)), off), (uintptr_t)num, 10);
    strcpy(b, (const char*)n->resolve(n->user, label));
    strcat(b, num);
    if (C2(0x004a91b0, (uintptr_t)b2, (uintptr_t)b) == 0)
        return 0;
    C2(0x004a5430, G(n, 0x0050fac4), row);
    C3(0x004a55f4, G(n, 0x0050fac4), row, (uintptr_t)b);
    return 1;
}

void mt2_ui_net_join_info(const Mt2UiNet* n)
{
    char b[0x12c], b2[0x80];
    int32_t r;
    uint32_t s, arena, mode, txt;

    r = (int32_t)C1(0x004a54b4, G(n, 0x0050fab8));
    if (r == -1 || r <= 0) {
        join_none(n, b, 0x004e1978, 0x004e1986, 0);
        return;
    }
    r = (int32_t)C1(0x004a54cc, G(n, 0x0050fab8));
    if (r < 0 || r >= 0x32) {
        join_none(n, b, 0x004e195c, 0x004e196a, 0);
        return;
    }
    s = C1(0x004a5394, G(n, 0x0050fab8));
    SETG(n, 0x0050faa8, s);
    if (s != G(n, 0x0050faac)) {
        join_none(n, b, 0x004e1940, 0x004e194e, 1);
        return;
    }
    arena = rd(n->resolve(n->user, s), 0x1a4);
    if (arena != G(n, 0x0050fab0)) {
        SETG(n, 0x0050fab0, arena);
        join_picture(n, b, arena);
    }
    /* row 0: the session name */
    C3(0x004a559c, G(n, 0x0050fac4), 0, (uintptr_t)b);
    if (C2(0x004a91b0, (uintptr_t)b, S(0x004e18b8)) == 0)
        C1(0x004a5630, G(n, 0x0050fac4));
    C3(0x004a559c, G(n, 0x0050fac4), 0, (uintptr_t)b);
    if (C2(0x004a91b0, (uintptr_t)b, S(G(n, 0x0050faa8) + 0x20)) != 0) {
        C2(0x004a5430, G(n, 0x0050fac4), 0);
        C3(0x004a55f4, G(n, 0x0050fac4), 0, S(G(n, 0x0050faa8) + 0x20));
    }
    /* row 1: the arena name (the signed byte at +0x1a4: Watcom reads it as dword +0x1a1 >> 24) */
    C3(0x004a559c, G(n, 0x0050fac4), 1, (uintptr_t)b);
    txt = C2(0x00474c1c, 0x00624ea8, (uint32_t)(int32_t)(int8_t)n->resolve(n->user, G(n, 0x0050faa8))[0x1a4]);
    if (C2(0x004a91b0, (uintptr_t)b, S(txt)) != 0) {
        C2(0x004a5430, G(n, 0x0050fac4), 1);
        txt = C2(0x00474c1c, 0x00624ea8, (uint32_t)(int32_t)(int8_t)n->resolve(n->user, G(n, 0x0050faa8))[0x1a4]);
        C3(0x004a55f4, G(n, 0x0050fac4), 1, S(txt));
    }
    /* row 2: the mode */
    mode = rd(n->resolve(n->user, G(n, 0x0050faa8)), 0x1a0);
    if (mode == 7 || mode == 6 || mode == 5) {
        uint32_t cmp = mode == 7 ? 0x004e18c6 : mode == 6 ? 0x004e18e6 : 0x004e1904;
        uint32_t set = mode == 7 ? 0x004e18d6 : mode == 6 ? 0x004e18f5 : 0x004e1914;
        C3(0x004a559c, G(n, 0x0050fac4), 2, (uintptr_t)b);
        if (C2(0x004a91b0, (uintptr_t)b, S(cmp)) != 0) {
            C2(0x004a5430, G(n, 0x0050fac4), 2);
            C3(0x004a55f4, G(n, 0x0050fac4), 2, S(set));
        }
    }
    join_count_row(n, b, b2, 3, 0x004e1924, 0x14);
    join_count_row(n, b, b2, 4, 0x004e1932, 0x18);
}

void mt2_ui_net_host_start(const Mt2UiNet* n)
{
    char buf[0x80];
    uint32_t ok;
    C3(0x004a6ef4, G(n, 0x0050fa74), (uintptr_t)buf, 0x80);
    if (buf[0] == 0) {
        uint32_t hwnd = C0(0x004a8f34);
        call(n, 0x00c90524, 4, hwnd, 0x004e171f, 0x004e170f, 0x30, 0);
        return;
    }
    SETG(n, 0x0050fa68, 1);
    SETG(n, 0x0050fa6c, 4);
    C2(0x004ac01c, G(n, 0x0050f9d8), (uintptr_t)buf);
    C3(0x004a6ef4, G(n, 0x0050fa78), (uintptr_t)buf, 0x80);
    C2(0x004ac044, G(n, 0x0050f9d8), (uintptr_t)buf);
    C1(0x004abd38, G(n, 0x0050f9d8));
    ok = C1(0x004aa690, SESSION);
    *n->game_type = (uint8_t)C1(0x004ac0b8, G(n, 0x0050f9d8));
    *n->arena = n->g[0x0050fa70 - GBASE];
    if (ok != 0) {
        C1(0x004aa638, SESSION);
        SETG(n, 0x0050fa68, 1);
        SETG(n, 0x0050fa6c, 0);
    }
}

uint32_t mt2_ui_net_join_run(const Mt2UiNet* n)
{
    char buf[0x80];
    uint32_t key[1], r;
    C2(0x004ac4b0, (uintptr_t)key, 0);
    SETG(n, 0x0050fab0, 0);
    C1(0x004aa638, SESSION);
    C0(0x0044ee0c);
    C0(0x00468db8);
    C3(0x004a6ef4, G(n, 0x0050fab4), (uintptr_t)buf, 0x80);
    C2(0x004ac044, G(n, 0x0050f9d8), (uintptr_t)buf);
    C1(0x004abd38, G(n, 0x0050f9d8));
    C2(0x004ab72c, G(n, 0x0050f9d8), 0x00468850);
    C2(0x004ab734, G(n, 0x0050f9d8), 0x00468988);
    r = C1(0x004aa68c, SESSION);
    C3(0x004abb68, G(n, 0x0050f9d8), r, G(n, 0x0050fab8));
    C2(0x004a56f8, G(n, 0x0050fab8), 0);
    SETG(n, 0x0050faa0, 0);
    SETG(n, 0x0050faa4, 0);
    do {
        if ((int32_t)C1(0x004a54b4, G(n, 0x0050fab8)) < 1)
            C2(0x004a6dd8, G(n, 0x0050fac8), 0);
        C0(0x00493748);
        if ((uint8_t)C2(0x004936ac, 0x14, 0) != 0) {
            SETG(n, 0x0050faa4, 0);
            SETG(n, 0x0050faa0, 1);
        }
        if ((uint8_t)C2(0x004936ac, 0x15, 0) != 0) {
            SETG(n, 0x0050faa4, 1);
            SETG(n, 0x0050faa0, 1);
        }
        join_list_step(n);
        C0(0x004693cc);
        C3(0x004a6ef4, G(n, 0x0050fab4), (uintptr_t)buf, 0x80);
        C2(0x004ac044, G(n, 0x0050f9d8), (uintptr_t)buf);
        join_restore(n);
    } while (G(n, 0x0050faa0) != 1);
    r = G(n, 0x0050faa4);
    if (r == 0 || r == 1) {
        C2(0x004ab72c, G(n, 0x0050f9d8), 0);
        C1(0x004abd38, G(n, 0x0050f9d8));
        C1(0x004aa5b8, SESSION);
    }
    C0(0x004692bc);
    return G(n, 0x0050faa4);
}

#define CALLN(f, k, a0, a1, a2, a3, a4, a5, a6, a7, a8) do { \
        uintptr_t a_[9] = {(a0), (a1), (a2), (a3), (a4), (a5), (a6), (a7), (a8)}; \
        n->call(n->user, (f), (k), a_); } while (0)

static uint32_t callr(const Mt2UiNet* n, uint32_t f, int k, const uintptr_t* a)
{
    return n->call(n->user, f, k, a);
}

/* widget method +off (w, arg) */
static void wcall(const Mt2UiNet* n, uint32_t a, uint32_t off, uint32_t arg)
{
    uint32_t w = G(n, a);
    C2(method(n, w, 0x34, off), w, arg);
}

static void style(const Mt2UiNet* n, uint32_t a)
{
    wcall(n, a, 0xb0, 0xffffff);
    wcall(n, a, 0xb4, 0);
    wcall(n, a, 0xb8, 0);
}

/* alloc + create: sprite (5 stack arguments) / panel (4) / text button (5) */
static void create(const Mt2UiNet* n, uint32_t dst, uint32_t size, uint32_t fn, uint32_t hwnd, uint32_t id,
                   uint32_t ecx, int nstack, const uint32_t* st)
{
    uint32_t mem = C1(0x004a8f48, size), w = 0;
    if (mem != 0) {
        uintptr_t a[9] = {mem, hwnd, id, ecx, 0, 0, 0, 0, 0};
        int k;
        for (k = 0; k < nstack; k++)
            a[4 + k] = st[k];
        w = callr(n, fn, 4 + nstack, a);
    }
    SETG(n, dst, w);
}

static void nav(const Mt2UiNet* n, uint32_t w, uint32_t up, uint32_t down, uint32_t left, uint32_t right,
                uint32_t prev, uint32_t next)
{
    CALLN(0x004a70ac, 7, w, up, down, left, right, prev, next, 0, 0);
}

void mt2_ui_net_join_build(const Mt2UiNet* n)
{
    static const uint32_t spr[5] = {0x20, 0x12, 0x104, 0x124, 0x13};
    static const uint32_t pan1[4] = {0x12, 0x54, 0x124, 0x93};
    static const uint32_t pan2[4] = {0x12, 0x134, 0x124, 0x77};
    static const uint32_t pan3[4] = {0x14a, 0x14f, 0x124, 0x5c};
    static const uint32_t but1[5] = {1, 0x10, 0x1c1, 0x8c, 0x17};
    static const uint32_t but2[5] = {1, 0xfa, 0x1c1, 0x8c, 0x17};
    static const uint32_t but3[5] = {1, 0x1e4, 0x1c1, 0x8c, 0x17};
    static const uint32_t all[7] = {0x0050fab4, 0x0050fab8, 0x0050fabc, 0x0050fac4, 0x0050fac8, 0x0050facc, 0x0050fad0};
    char path[0x12c];
    uint32_t hwnd = C0(0x004a8f34), p, text;
    int k;

    p = C1(0x004a8f48, 0xb4);
    if (p != 0) {
        p = C1(0x004a2e60, p);
        memcpy(n->resolve(n->user, p) + 0x30, &(uint32_t){0x004e50d8}, 4);
    }
    SETG(n, 0x0050fac0, p);
    C3(0x00499408, (uintptr_t)path, S(rd(n->resolve(n->user, 0x004e7b64), 0)), 1);
    p = G(n, 0x0050fac0);
    C3(method(n, p, 0x30, 0), p, (uintptr_t)path, 0);
    p = G(n, 0x0050fac0);
    C4(method(n, p, 0x30, 0x1c), p, 0x148, 0x52, 0);
    C0(0x00495b14);
    text = C1(0x004ac06c, G(n, 0x0050f9d8));
    create(n, 0x0050fab4, 0x68, 0x004a5f40, hwnd, 0x79, text, 5, spr);
    style(n, 0x0050fab4);
    C2(0x004a5fe0, G(n, 0x0050fab4), 10);
    create(n, 0x0050fab8, 0x7c, 0x004a5290, hwnd, 0x83, 0x200000, 4, pan1);
    style(n, 0x0050fab8);
    create(n, 0x0050fabc, 0x7c, 0x004a5290, hwnd, 0x8d, 0x200100, 4, pan2);
    style(n, 0x0050fabc);
    create(n, 0x0050fac4, 0x7c, 0x004a5290, hwnd, 0xa1, 0x200100, 4, pan3);
    style(n, 0x0050fac4);
    create(n, 0x0050fac8, 0x1cc, 0x004a490c, hwnd, 0xb4, 0, 5, but1);
    create(n, 0x0050facc, 0x1cc, 0x004a490c, hwnd, 0xb6, 0, 5, but2);
    create(n, 0x0050fad0, 0x1cc, 0x004a490c, hwnd, 0xb7, 0, 5, but3);
    C3(0x004a4de0, G(n, 0x0050fac8), 0xcc0ff, 0xc0c0c);
    C3(0x004a4de0, G(n, 0x0050facc), 0xcc0ff, 0xc0c0c);
    C3(0x004a4de0, G(n, 0x0050fad0), 0xcc0ff, 0xc0c0c);
    CALLN(0x004a4bdc, 5, G(n, 0x0050fac8), 0x7b, 0x79, 0, 0x7a, 0, 0, 0, 0);
    CALLN(0x004a4bdc, 5, G(n, 0x0050facc), 0x76, 0x75, 0, 0, 0, 0, 0, 0);
    CALLN(0x004a4bdc, 5, G(n, 0x0050fad0), 0x78, 0x77, 0, 0, 0, 0, 0, 0);
    wcall(n, 0x0050fac8, 0xe0, 0x004699c4);
    wcall(n, 0x0050facc, 0xe0, 0x00469a60);
    wcall(n, 0x0050fad0, 0xe0, 0x00469a74);
    wcall(n, 0x0050fab8, 0xcc, 0x00469a8c);
    wcall(n, 0x0050fab8, 0xd4, 0x00469a8c);
    wcall(n, 0x0050fabc, 0xcc, 0x00469b34);
    wcall(n, 0x0050fabc, 0xd4, 0x00469b34);
    nav(n, G(n, 0x0050fab8), 0, 0, 0, 0, G(n, 0x0050fad0), G(n, 0x0050fab4));
    nav(n, G(n, 0x0050fab4), G(n, 0x0050fab8), G(n, 0x0050fac8), 0, 0, G(n, 0x0050fab8), G(n, 0x0050fac8));
    nav(n, G(n, 0x0050fac8), G(n, 0x0050fab4), G(n, 0x0050fab8), G(n, 0x0050fad0), G(n, 0x0050facc),
        G(n, 0x0050fab4), G(n, 0x0050facc));
    nav(n, G(n, 0x0050facc), G(n, 0x0050fab4), G(n, 0x0050fab8), G(n, 0x0050fac8), G(n, 0x0050fad0),
        G(n, 0x0050fac8), G(n, 0x0050fad0));
    nav(n, G(n, 0x0050fad0), G(n, 0x0050fab4), G(n, 0x0050fab8), G(n, 0x0050facc), G(n, 0x0050fac8),
        G(n, 0x0050facc), G(n, 0x0050fab8));
    C0(0x004a8e6c);
    for (k = 0; k < 7; k++)
        C2(0x004a6ee4, G(n, all[k]), 5);
    C1(0x004a6f80, G(n, 0x0050facc));
    C2(0x004a6dd8, G(n, 0x0050fac8), 0);
    C3(0x004a6e2c, G(n, 0x0050fac8), 0, 0);
}

void mt2_ui_net_host_build(const Mt2UiNet* n)
{
    static const uint32_t spr1[5] = {0, 0x142, 0x44, 0x128, 0x13};
    static const uint32_t spr2[5] = {0, 0x142, 0x77, 0x128, 0x13};
    static const uint32_t m1[5] = {6, 0x12, 0x44, 0x104, 0x13};
    static const uint32_t m2[5] = {6, 0x12, 0x5e, 0x104, 0x13};
    static const uint32_t m3[5] = {6, 0x12, 0x78, 0x104, 0x13};
    static const uint32_t a1[5] = {1, 0x1f0, 0xf4, 0x28, 0x64};
    static const uint32_t a2[5] = {1, 0x68, 0xf4, 0x28, 0x64};
    static const uint32_t b1[5] = {1, 0x10, 0x1c1, 0x8c, 0x17};
    static const uint32_t b2[5] = {1, 0xfa, 0x1c1, 0x8c, 0x17};
    static const uint32_t b3[5] = {1, 0x1e4, 0x1c1, 0x8c, 0x17};
    static const uint32_t navs[10][7] = {
        {0x7c, 0x94, 0x80, 0x74, 0x74, 0x9c, 0x80}, {0x80, 0x7c, 0x84, 0x74, 0x74, 0x7c, 0x84},
        {0x84, 0x80, 0x8c, 0x78, 0x78, 0x80, 0x74}, {0x74, 0x9c, 0x78, 0, 0, 0x84, 0x78},
        {0x78, 0x74, 0x88, 0, 0, 0x74, 0x8c}, {0x88, 0x78, 0x9c, 0x8c, 0x8c, 0x8c, 0x94},
        {0x8c, 0x84, 0x94, 0x88, 0x88, 0x78, 0x88}, {0x94, 0x8c, 0x7c, 0x9c, 0x98, 0x88, 0x98},
        {0x98, 0x8c, 0x7c, 0x94, 0x9c, 0x94, 0x9c}, {0x9c, 0x88, 0x74, 0x98, 0x94, 0x98, 0x7c}};
    static const uint32_t handlers[8][2] = {
        {0x7c, 0x004686d4}, {0x80, 0x00468724}, {0x84, 0x00468774}, {0x94, 0x004684ec},
        {0x98, 0x004684d8}, {0x9c, 0x004684c0}, {0x88, 0x004685d4}, {0x8c, 0x00468650}};
    static const uint32_t props[10] = {0x74, 0x78, 0x7c, 0x80, 0x84, 0x88, 0x8c, 0x94, 0x98, 0x9c};
    char buf[0x12c];
    uint32_t key[1], size[1], mode[1], hwnd, text, p;
    int32_t arena;
    int k;

    C2(0x004ac4b0, (uintptr_t)key, 0);
    hwnd = C0(0x004a8f34);
    text = C1(0x004ac040, G(n, 0x0050f9d8));
    if (call(n, 0x00c90318, 2, S(text), S(0x004e16e7), 0, 0, 0) == 0)
        text = rd(n->resolve(n->user, 0x004e7afc), 0);
    else
        text = C1(0x004ac040, G(n, 0x0050f9d8));
    create(n, 0x0050fa74, 0x68, 0x004a5f40, hwnd, 0xab, text, 5, spr1);
    style(n, 0x0050fa74);
    text = C1(0x004ac06c, G(n, 0x0050f9d8));
    create(n, 0x0050fa78, 0x68, 0x004a5f40, hwnd, 0xbf, text, 5, spr2);
    style(n, 0x0050fa78);
    C2(0x004a5fe0, G(n, 0x0050fa78), 10);
    create(n, 0x0050fa7c, 0x1cc, 0x004a490c, hwnd, 0x79, 0, 5, m1);
    style(n, 0x0050fa7c);
    C3(0x004a4de0, G(n, 0x0050fa7c), 0xcc0ff, 0xc0c0c);
    create(n, 0x0050fa80, 0x1cc, 0x004a490c, hwnd, 0x7a, 0, 5, m2);
    style(n, 0x0050fa80);
    C3(0x004a4de0, G(n, 0x0050fa80), 0xcc0ff, 0xc0c0c);
    create(n, 0x0050fa84, 0x1cc, 0x004a490c, hwnd, 0x7b, 0, 5, m3);
    style(n, 0x0050fa84);
    C3(0x004a4de0, G(n, 0x0050fa84), 0xcc0ff, 0xc0c0c);
    create(n, 0x0050fa88, 0x1cc, 0x004a490c, hwnd, 0x83, 0, 5, a1);
    create(n, 0x0050fa8c, 0x1cc, 0x004a490c, hwnd, 0x84, 0, 5, a2);
    C3(0x004a4de0, G(n, 0x0050fa88), 0xcc0ff, 0xc0c0c);
    C3(0x004a4de0, G(n, 0x0050fa8c), 0xcc0ff, 0xc0c0c);

    arena = (int8_t)*n->arena;
    if (arena > 9) {
        uint32_t need = arena == 10 ? 0x23 : arena == 11 ? 0x24 : arena == 12 ? 0x25 : 0;
        if (C1(0x0044eee4, 0x1f) == 0 || need == 0 || C1(0x0044eee4, need) == 0)
            *n->arena = 1;
        /* else: the arena stays (the original stores the same value back) */
    }
    p = C1(0x004a8f48, 0xb4);
    if (p != 0) {
        p = C1(0x004a2e60, p);
        memcpy(n->resolve(n->user, p) + 0x30, &(uint32_t){0x004e50d8}, 4);
    }
    SETG(n, 0x0050fa90, p);
    C3(0x00499408, (uintptr_t)buf, S(rd(n->resolve(n->user, 0x004e7b20 + 4 * (uint32_t)(int32_t)(int8_t)*n->arena), 0)), 1);
    p = G(n, 0x0050fa90);
    C3(method(n, p, 0x30, 0), p, (uintptr_t)buf, 0);
    p = G(n, 0x0050fa90);
    C4(method(n, p, 0x30, 0x1c), p, 0x94, 0xa5, 0);
    SETG(n, 0x0050fa70, (uint32_t)(int32_t)(int8_t)*n->arena);
    create(n, 0x0050fa94, 0x1cc, 0x004a490c, hwnd, 0xb5, 0, 5, b1);
    create(n, 0x0050fa98, 0x1cc, 0x004a490c, hwnd, 0xb6, 0, 5, b2);
    create(n, 0x0050fa9c, 0x1cc, 0x004a490c, hwnd, 0xb7, 0, 5, b3);
    C3(0x004a4de0, G(n, 0x0050fa94), 0xcc0ff, 0xc0c0c);
    C3(0x004a4de0, G(n, 0x0050fa98), 0xcc0ff, 0xc0c0c);
    C3(0x004a4de0, G(n, 0x0050fa9c), 0xcc0ff, 0xc0c0c);
    C4(0x004a4c00, G(n, 0x0050fa7c), 0x89, 0x8a, 0);
    C4(0x004a4c00, G(n, 0x0050fa80), 0x87, 0x88, 0);
    C4(0x004a4c00, G(n, 0x0050fa84), 0x8b, 0x8c, 0);
    CALLN(0x004a4bdc, 5, G(n, 0x0050fa88), 0x81, 0x80, 0, 0, 0, 0, 0, 0);
    CALLN(0x004a4bdc, 5, G(n, 0x0050fa8c), 0x83, 0x82, 0, 0, 0, 0, 0, 0);
    CALLN(0x004a4bdc, 5, G(n, 0x0050fa94), 0x7f, 0x7e, 0, 0, 0, 0, 0, 0);
    CALLN(0x004a4bdc, 5, G(n, 0x0050fa98), 0x76, 0x75, 0, 0, 0, 0, 0, 0);
    CALLN(0x004a4bdc, 5, G(n, 0x0050fa9c), 0x78, 0x77, 0, 0, 0, 0, 0, 0);
    for (k = 0; k < 8; k++)
        wcall(n, 0x0050fa00 + handlers[k][0], 0xe0, handlers[k][1]);
    C2(0x004aaa74, SESSION, 7);
    if (C3(0x004ac528, (uintptr_t)key, 0x80000002u, 0) == 0) {
        size[0] = 0x80;
        if (C4(0x004ac5e4, (uintptr_t)key, 0x004e16e8, (uintptr_t)buf, (uintptr_t)size) == 0)
            C2(0x004a6f08, G(n, 0x0050fa74), (uintptr_t)buf);
        if (C3(0x004ac5bc, (uintptr_t)key, 0x004e16f7, (uintptr_t)mode) == 0
            && (mode[0] == 6 || mode[0] == 5 || mode[0] == 7)) {
            C2(0x004aaa74, SESSION, mode[0]);
            *n->game_type = (uint8_t)mode[0];
        }
    }
    C1(0x004ac590, (uintptr_t)key);
    mode[0] = C1(0x004aaa70, SESSION);
    if (mode[0] == 7) {
        wcall(n, 0x0050fa7c, 0xd0, 1); wcall(n, 0x0050fa80, 0xd0, 0); wcall(n, 0x0050fa84, 0xd0, 0);
    }
    if (mode[0] == 6) {
        wcall(n, 0x0050fa7c, 0xd0, 0); wcall(n, 0x0050fa80, 0xd0, 1); wcall(n, 0x0050fa84, 0xd0, 0);
    }
    if (mode[0] == 5) {
        wcall(n, 0x0050fa7c, 0xd0, 0); wcall(n, 0x0050fa80, 0xd0, 0); wcall(n, 0x0050fa84, 0xd0, 1);
    }
    for (k = 0; k < 10; k++) {
        uint32_t v[7];
        int j;
        for (j = 0; j < 7; j++)
            v[j] = navs[k][j] ? G(n, 0x0050fa00 + navs[k][j]) : 0;
        nav(n, v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
    }
    C0(0x004a8e6c);
    for (k = 0; k < 10; k++)
        C2(0x004a6ee4, G(n, 0x0050fa00 + props[k]), 5);
    C1(0x004a6f80, G(n, 0x0050fa94));
}

uint32_t mt2_ui_net_host_run(const Mt2UiNet* n)
{
    char path[0x12c], name[0x80], save[0x80];
    uint32_t key[1], r, p;
    C2(0x004ac4b0, (uintptr_t)key, 0);
    C0(0x00467904);
    SETG(n, 0x0050fa68, 0);
    SETG(n, 0x0050fa6c, 0);
    do {
        int32_t arena = (int8_t)*n->arena;
        if ((uint32_t)arena != G(n, 0x0050fa70)) {
            SETG(n, 0x0050fa70, (uint32_t)arena);
            C3(0x00499408, (uintptr_t)path, S(rd(n->resolve(n->user, 0x004e7b20 + 4 * (uint32_t)arena), 0)), 1);
            p = G(n, 0x0050fa90);
            C1(method(n, p, 0x30, 0xc), p);
            p = G(n, 0x0050fa90);
            C3(method(n, p, 0x30, 0), p, (uintptr_t)path, 0);
            p = G(n, 0x0050fa90);
            C4(method(n, p, 0x30, 0x1c), p, 0x94, 0xa5, 0);
            C3(0x00c904f8, 0, 0, 0);
        }
        C2(0x004ac0a4, G(n, 0x0050f9d8), (uint32_t)(int32_t)(int8_t)*n->arena);
        C0(0x00493748);
        if ((uint8_t)C2(0x004936ac, 0x14, 0) != 0) {
            SETG(n, 0x0050fa68, 1);
            SETG(n, 0x0050fa6c, 0);
        }
        if ((uint8_t)C2(0x004936ac, 0x15, 0) != 0) {
            SETG(n, 0x0050fa6c, 1);
            SETG(n, 0x0050fa68, 1);
        }
        C3(0x004a6ef4, G(n, 0x0050fa74), (uintptr_t)name, 0x80);
        C2(0x004ac01c, G(n, 0x0050f9d8), (uintptr_t)name);
        C3(0x004a6ef4, G(n, 0x0050fa78), (uintptr_t)name, 0x80);
        C2(0x004ac044, G(n, 0x0050f9d8), (uintptr_t)name);
    } while (G(n, 0x0050fa68) != 1);
    r = G(n, 0x0050fa6c);
    if (r == 0 || r == 1) {
        C1(0x004aa638, SESSION);
    } else {
        if (C3(0x004ac528, (uintptr_t)key, 0x80000002u, 0) == 0) {
            C3(0x004a6ef4, G(n, 0x0050fa74), (uintptr_t)save, 0x80);
            C3(0x004ac638, (uintptr_t)key, 0x004e16d8, (uintptr_t)save);
        }
        C1(0x004ac590, (uintptr_t)key);
    }
    C0(0x004682c8);
    return G(n, 0x0050fa6c);
}
