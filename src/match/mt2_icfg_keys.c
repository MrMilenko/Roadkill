/*
 * mt2_icfg_keys.c: TM2.EXE 0x004605b0 and 0x00460b04. See mt2_icfg_keys.h.
 */
#include "mt2_icfg_keys.h"

#include <stddef.h>

#define TAKEN 0x0050e5c4u
#define SHOWN_B 0x0050e5c9u
#define NEXT 0x0050e5cau
#define SHOWN 0x0050e5cbu
#define W_ACCEPT 0x0050e428u
#define W_CANCEL 0x0050e42cu
#define W_STATUS 0x0050e43cu
#define W_CODE 0x0050e440u

static uint32_t r8(const Mt2IcfgKeys* m, uint32_t a) { return *m->resolve(m->user, a); }
static uint32_t r32(const Mt2IcfgKeys* m, uint32_t a)
{
    const uint8_t* p = m->resolve(m->user, a);
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void w8(const Mt2IcfgKeys* m, uint32_t a, uint32_t v) { *m->resolve(m->user, a) = (uint8_t)v; }
static void w32(const Mt2IcfgKeys* m, uint32_t a, uint32_t v)
{
    uint8_t* p = m->resolve(m->user, a);
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t c2(const Mt2IcfgKeys* m, uint32_t fn, uint32_t eax, uint32_t edx)
{
    return m->call(m->user, fn, eax, edx, 0, 0, NULL, 0);
}

/* play (0x18 accept / 0x3f reject), flush 0x0047314c, commit on the sound object 0x00626010 */
static void sound(const Mt2IcfgKeys* m, uint32_t id, uint32_t seq)
{
    uint32_t obj = r32(m, 0x00626010), vt = r32(m, obj + 0x1c), n = r32(m, seq);
    uint32_t st[3] = { 0x63, n, 0 };
    w32(m, seq, n + 1);
    m->call(m->user, r32(m, vt), obj, id, 0, 0, st, 3);
    c2(m, 0x0047314c, r32(m, 0x00626010), 0);
    obj = r32(m, 0x00626010);
    c2(m, r32(m, r32(m, obj + 0x1c) + 4), obj, 0);
}
#define OK(m) sound(m, 0x18, 0x004e7554u)
#define BAD(m) sound(m, 0x3f, 0x004e7558u)

/* the rejected key: next = 0; when the status list's text equals `same` the list is cleared
 * (0x004a5630, LB_RESETCONTENT; edx holds msg there but is not read) and msg added (0x004a5418,
 * edx kept from the previous call). DECOMPILER TRAP: Ghidra shows 0x004a5418 without the text. */
static void reject(const Mt2IcfgKeys* m, uint32_t same, uint32_t msg)
{
    char buf[256];
    BAD(m);
    w8(m, NEXT, 0);
    m->call_buf(m->user, 0x004a559c, r32(m, W_STATUS), 0, buf);
    if (m->call_buf(m->user, 0x004a91b0, 0, same, buf) != 0) {
        c2(m, 0x004a5630, r32(m, W_STATUS), msg);
        c2(m, 0x004a5418, r32(m, W_STATUS), msg);
    }
}

void mt2_icfg_read_keyboard(const Mt2IcfgKeys* m, int32_t row)
{
    uint32_t key, n;
    c2(m, 0x00493748, (uint32_t)row, 0);
    key = c2(m, 0x004a2068, 0, 0) & 0xff;
    if (key != 0) {
        uint32_t act = 0;
        if (key == r8(m, SHOWN)) {
            if (r8(m, SHOWN) == 0x1c && (int32_t)r32(m, TAKEN) > 0)
                act = W_ACCEPT;
        } else if (r8(m, 0x0050e5cc + key) == 0) {
            if (key == 0x3b) {
                OK(m);
                w8(m, NEXT, 0);
                w32(m, TAKEN, r32(m, TAKEN) + 1);
            } else if (key == 0x3c) {
                uint32_t id;
                OK(m);
                id = r8(m, 0x004e7a48 + (uint32_t)row * 8);
                w32(m, TAKEN, r32(m, TAKEN) + 1);
                w8(m, NEXT, c2(m, 0x004a2008, id, 0));
            } else if (key == 1) {
                act = W_CANCEL;
            } else {
                reject(m, 0x004e0952, 0x004e096e);
            }
        } else if (key != 0x1c || r32(m, TAKEN) == 0) {
            OK(m);
            w8(m, NEXT, key);
            w32(m, TAKEN, r32(m, TAKEN) + 1);
        } else {
            act = W_ACCEPT;
        }
        if (act)
            c2(m, 0x004a4394, r32(m, act), 0);
    }
    n = r8(m, NEXT);
    if (n != r8(m, SHOWN)) {
        c2(m, 0x004a6f08, r32(m, W_CODE), r32(m, 0x004e755c + 4 * n));
        w8(m, SHOWN, r8(m, NEXT));
        w8(m, SHOWN_B, r8(m, NEXT));
        if (r8(m, NEXT) != 0)
            c2(m, 0x0046086c, (uint32_t)row, 1);
    }
}

void mt2_icfg_read_dev4(const Mt2IcfgKeys* m, int32_t row)
{
    uint32_t key, v, n;
    c2(m, 0x00493748, (uint32_t)row, 0);
    key = c2(m, 0x004a2068, 0, 0) & 0xff;
    v = c2(m, 0x004a2828, 0, 0) & 0xff;
    if (key == 0) {
        if (v != 0 && v != r8(m, SHOWN)) {
            if (r8(m, 0x0050e6d4 + v) != 0) {
                OK(m);
                w8(m, NEXT, v);
            } else {
                reject(m, 0x004e09b5, 0x004e09d1);
            }
        }
    } else if (key == 0x3b) {
        if (r8(m, SHOWN) != 0) {
            OK(m);
            w8(m, NEXT, 0);
        }
    } else if (key == 0x3c) {
        if ((c2(m, 0x004a27c8, r8(m, 0x004e7a48 + (uint32_t)row * 8), 0) & 0xff) != r8(m, SHOWN)) {
            OK(m);
            w8(m, NEXT, c2(m, 0x004a27c8, r8(m, 0x004e7a48 + (uint32_t)row * 8), 0));
        }
    } else if (key == 0x1c) {
        c2(m, 0x004a4394, r32(m, W_ACCEPT), 0);
    } else if (key == 1) {
        c2(m, 0x004a4394, r32(m, W_CANCEL), 0);
    } else if (key == 0xc8 || key == 0xcb) {
        OK(m);
        do {
            n = r8(m, NEXT);
            w8(m, NEXT, n > 0 ? n - 1 : 0x31);
        } while (r8(m, 0x0050e6d4 + r8(m, NEXT)) == 0);
    } else if (key == 0xd0 || key == 0xcd || key == 0x0f) {
        OK(m);
        do {
            w8(m, NEXT, r8(m, NEXT) + 1);
            if (r8(m, NEXT) >= 0x32)
                w8(m, NEXT, 0);
        } while (r8(m, 0x0050e6d4 + r8(m, NEXT)) == 0);
    } else {
        BAD(m);
    }
    n = r8(m, NEXT);
    if (n != r8(m, SHOWN)) {
        c2(m, 0x004a6f08, r32(m, W_CODE), r32(m, 0x004e797c + 4 * n));
        w8(m, SHOWN, r8(m, NEXT));
        w8(m, SHOWN_B, r8(m, NEXT));
        c2(m, 0x00460ea0, (uint32_t)row, 1);
    }
}
