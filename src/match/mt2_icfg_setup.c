/*
 * mt2_icfg_setup.c: TM2.EXE 0x0045fc3c. See mt2_icfg_setup.h.
 */
#include "mt2_icfg_setup.h"

#include <stddef.h>

#define W_ACCEPT 0x0050e428u
#define W_CANCEL 0x0050e42cu
#define W_ROW 0x0050e430u
#define W_CUR 0x0050e434u
#define W_HELP 0x0050e438u
#define W_STATUS 0x0050e43cu
#define W_CODE 0x0050e440u

static uint32_t r8(const Mt2IcfgSetup* m, uint32_t a) { return *m->resolve(m->user, a); }
static uint32_t r32(const Mt2IcfgSetup* m, uint32_t a)
{
    const uint8_t* p = m->resolve(m->user, a);
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void w8(const Mt2IcfgSetup* m, uint32_t a, uint32_t v) { *m->resolve(m->user, a) = (uint8_t)v; }
static void w32(const Mt2IcfgSetup* m, uint32_t a, uint32_t v)
{
    uint8_t* p = m->resolve(m->user, a);
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t c4(const Mt2IcfgSetup* m, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx)
{
    return m->call(m->user, fn, eax, edx, ebx, ecx, NULL, 0);
}
static uint32_t c2(const Mt2IcfgSetup* m, uint32_t fn, uint32_t eax, uint32_t edx) { return c4(m, fn, eax, edx, 0, 0); }
/* method `ofs` of the widget held in global g: [[w + 0x34] + ofs](w, edx) */
static void method(const Mt2IcfgSetup* m, uint32_t g, uint32_t ofs, uint32_t edx)
{
    uint32_t w = r32(m, g);
    c2(m, r32(m, r32(m, w + 0x34) + ofs), w, edx);
}

/* allocate `size` (0x004a8f48) and construct with `ctor` (eax obj, edx window, ebx id, ecx, then
 * the stack words); a failed allocation stores 0 */
static void make(const Mt2IcfgSetup* m, uint32_t g, uint32_t size, uint32_t ctor, uint32_t hwnd, uint32_t id,
                 uint32_t ecx, const uint32_t* st, int ns)
{
    uint32_t p = c2(m, 0x004a8f48, size, 0);
    if (p != 0)
        p = m->call(m->user, ctor, p, hwnd, id, ecx, st, ns);
    w32(m, g, p);
}

void mt2_icfg_setup(const Mt2IcfgSetup* m, int32_t kind, int32_t row)
{
    static const uint32_t st_acc[5] = { 1, 0xac, 0x189, 0x8c, 0x17 }, st_can[5] = { 1, 0x148, 0x189, 0x8c, 0x17 };
    static const uint32_t st_row[5] = { 0xb, 0x147, 0xc1, 0x84, 0x16 }, st_cur[5] = { 0xb, 0x147, 0xda, 0x84, 0x16 };
    static const uint32_t st_help[4] = { 0xb4, 0x60, 0x116, 0x5a }, st_stat[4] = { 0xae, 0x12c, 0x122, 0x52 };
    static const uint32_t st_code[5] = { 0x800, 0x146, 0x108, 0x84, 0x16 };
    static const uint32_t help[3][6] = {
        { 0x004e0739, 0x004e075a, 0x004e0781, 0x004e07a7, 0x004e07d0, 0x004e755c },  /* keyboard */
        { 0x004e07dd, 0x004e07fe, 0x004e0826, 0x004e0850, 0x004e0875, 0x004e797c },  /* joystick */
        { 0x004e0899, 0x004e08b7, 0x004e08df, 0x004e0909, 0x004e092e, 0x004e795c } }; /* mouse */
    static const uint32_t props[7] = { W_ACCEPT, W_CANCEL, W_ROW, W_CUR, W_CODE, W_STATUS, W_HELP };
    char path[300];
    uint32_t hwnd, f, st[5];
    int k;

    hwnd = c2(m, 0x004a8f34, 0, 0);
    m->call_buf(m->user, 0x00499408, 0, 0x004e071e, 1, 0, path, 0);        /* SHELLDB3\INPUTDLG.PCX */
    m->call_buf(m->user, 0x004a2ea0, 0x0050e444, 0, 0, 0, path, 1);
    c4(m, 0x004a2ffc, 0x0050e444, 0xa0, 0x4d, 0);
    st[0] = 0; st[1] = 0xa0; st[2] = 0x4d; st[3] = 0x1df; st[4] = 0x1ab;
    m->call(m->user, 0x00c90574, 0, 0, 0, 0, st, 5);                          /* SetRect */
    st[0] = 0; st[1] = 0; st[2] = 1;
    m->call(m->user, 0x00c904f8, 0, 0, 0, 0, st, 3);                          /* InvalidateRect */
    c2(m, 0x004a8e6c, 0, 0);
    make(m, W_ACCEPT, 0x1cc, 0x004a490c, hwnd, 0x12c, 0x004e0734, st_acc, 5);
    make(m, W_CANCEL, 0x1cc, 0x004a490c, hwnd, 0x12d, 0x004e0735, st_can, 5);
    c4(m, 0x004a4de0, r32(m, W_ACCEPT), 0xcc0ff, 0xc0c0c, 0);
    c4(m, 0x004a4de0, r32(m, W_CANCEL), 0xcc0ff, 0xc0c0c, 0);
    st[0] = 0;
    m->call(m->user, 0x004a4b50, r32(m, W_ACCEPT), 0xce, 0xcd, 0, st, 1);
    m->call(m->user, 0x004a4b50, r32(m, W_CANCEL), 0x78, 0x77, 0, st, 1);
    make(m, W_ROW, 0x54, 0x004a6078, hwnd, 0x12e, 0x004e0736, st_row, 5);
    make(m, W_CUR, 0x54, 0x004a6078, hwnd, 0x12f, 0x004e0737, st_cur, 5);
    make(m, W_HELP, 0x7c, 0x004a5290, hwnd, 0x130, 0x200000, st_help, 4);
    make(m, W_STATUS, 0x7c, 0x004a5290, hwnd, 0x131, 0x200000, st_stat, 4);
    make(m, W_CODE, 0x68, 0x004a5f40, hwnd, 0x132, 0x004e0738, st_code, 5);
    for (k = 0; k < 2; k++) {
        uint32_t g = k ? W_CUR : W_ROW;
        method(m, g, 0xc0, 1);
        method(m, g, 0xb0, 0xffffff);
        method(m, g, 0xb4, 0);
        method(m, g, 0xb8, 0);
    }
    for (k = 0; k < 2; k++) {
        uint32_t g = k ? W_STATUS : W_HELP;
        method(m, g, 0xb0, 0xffffff);
        method(m, g, 0xb4, 0);
        method(m, g, 0xb8, 0);
    }
    method(m, W_CODE, 0xb0, 0);
    method(m, W_CODE, 0xb4, 0xffffff);
    method(m, W_CODE, 0xb8, 0xffffff);
    /* navigation links 0x004a70ac (eax, edx, ebx, ecx, 3 stack words) */
    st[0] = 0; st[1] = r32(m, W_CANCEL); st[2] = r32(m, W_ACCEPT);
    m->call(m->user, 0x004a70ac, r32(m, W_CODE), 0, 0, 0, st, 3);
    st[0] = r32(m, W_CANCEL); st[1] = r32(m, W_CODE); st[2] = r32(m, W_CANCEL);
    m->call(m->user, 0x004a70ac, r32(m, W_ACCEPT), r32(m, W_CODE), r32(m, W_CODE), r32(m, W_CANCEL), st, 3);
    st[0] = r32(m, W_ACCEPT); st[1] = r32(m, W_ACCEPT); st[2] = r32(m, W_CODE);
    m->call(m->user, 0x004a70ac, r32(m, W_CANCEL), r32(m, W_CODE), r32(m, W_CODE), r32(m, W_ACCEPT), st, 3);
    method(m, W_ACCEPT, 0xe0, 0x00461f6c);
    method(m, W_CANCEL, 0xe0, 0x00461f84);
    f = c2(m, 0x004a8fe8, (uint32_t)kind, r8(m, 0x004e7a48 + (uint32_t)row * 8)) & 0xff;
    w8(m, 0x0050e5c8, f);
    w8(m, 0x0050e5ca, f);
    w8(m, 0x0050e5cb, f);
    w8(m, 0x0050e5c9, f);
    c2(m, 0x004a6f08, r32(m, W_ROW), r32(m, 0x004e7a44 + (uint32_t)row * 8));
    if (kind == 1 || kind == 4 || kind == 2) {
        const uint32_t* h = help[kind == 1 ? 0 : kind == 4 ? 1 : 2];
        /* 0x004a5630 clears the list (edx holds the first line, not read); 0x004a5418 adds a line */
        c2(m, 0x004a5630, r32(m, W_HELP), h[0]);
        for (k = 0; k < 5; k++)
            c2(m, 0x004a5418, r32(m, W_HELP), h[k]);
        c2(m, 0x004a6f08, r32(m, W_CUR), r32(m, h[5] + 4 * r8(m, 0x0050e5c8)));
        c2(m, 0x004a6f08, r32(m, W_CODE), r32(m, h[5] + 4 * r8(m, 0x0050e5c8)));
    }
    c2(m, 0x004a5630, r32(m, W_STATUS), 1);
    c2(m, 0x0046086c, (uint32_t)row, 1);
    for (k = 0; k < 7; k++)
        c2(m, 0x004a6ee4, r32(m, props[k]), 5);
    c2(m, 0x004a6f80, r32(m, W_CODE), 0);
}

/* links 0x004a70ac: eax widget, edx / ebx / ecx and three stack words its neighbours */
static void link(const Mt2IcfgSetup* m, uint32_t w, uint32_t a, uint32_t b, uint32_t c, uint32_t s0, uint32_t s1,
                 uint32_t s2)
{
    uint32_t st[3];
    st[0] = s0; st[1] = s1; st[2] = s2;
    m->call(m->user, 0x004a70ac, w, a, b, c, st, 3);
}
#define G(a) r32(m, a)
#define LEFT(i) (0x0050e120u + 16u * (uint32_t)(i))
#define RIGHT(i) (0x0050e220u + 16u * (uint32_t)(i))

void mt2_icfg_create_widgets(const Mt2IcfgSetup* m)
{
    static const uint32_t s108[5] = { 3, 0x13, 0x2f, 0x87, 0x17 }, s10c[5] = { 3, 0x1e6, 0x2f, 0x87, 0x17 };
    static const uint32_t s414[5] = { 1, 0xac, 0x1c1, 0x8c, 0x17 }, s418[5] = { 1, 0x148, 0x1c1, 0x8c, 0x17 };
    static const uint32_t s41c[5] = { 1, 0x10, 0x1c1, 0x8c, 0x17 }, s420[5] = { 1, 0x1e4, 0x1c1, 0x8c, 0x17 };
    static const uint32_t devs[3][2] = { { 1, 0x0050e5ac }, { 4, 0x0050e5b0 }, { 2, 0x0050e5b4 } };
    static const uint32_t click[6][2] = { { 0x0050e414, 0x00461924 }, { 0x0050e41c, 0x004619e4 },
                                          { 0x0050e420, 0x00461a68 }, { 0x0050e418, 0x004618f8 },
                                          { 0x0050e108, 0x00461ae0 }, { 0x0050e10c, 0x00461b48 } };
    static const uint32_t shown[6] = { 0x0050e108, 0x0050e10c, 0x0050e414, 0x0050e418, 0x0050e41c, 0x0050e420 };
    char path[300];
    uint32_t hwnd, st[5], y, k, v;
    int32_t row, prev, next;
    int i;

    hwnd = c2(m, 0x004a8f34, 0, 0);
    m->call_buf(m->user, 0x00499408, 0, 0x004e0700, 1, 0, path, 0);        /* SHELLDB3\INPUTCFG.PCX */
    m->call_buf(m->user, 0x004a2ea0, 0x0050e4f8, 0, 0, 0, path, 1);
    c4(m, 0x004a2ffc, 0x0050e4f8, 0, 0, 0);
    c2(m, 0x00495b14, 0, 0);
    make(m, 0x0050e108, 0x1cc, 0x004a490c, hwnd, 0x6f, 0x004e0716, s108, 5);
    c4(m, 0x004a4de0, G(0x0050e108), 0xcc0ff, 0xc0c0c, 0);
    c4(m, 0x004a4c00, G(0x0050e108), 0xb4, 0xb5, 0);
    make(m, 0x0050e10c, 0x1cc, 0x004a490c, hwnd, 0x70, 0x004e0717, s10c, 5);
    c4(m, 0x004a4de0, G(0x0050e10c), 0xcc0ff, 0xc0c0c, 0);
    c4(m, 0x004a4c00, G(0x0050e10c), 0xb0, 0xb1, 0);
    c4(m, 0x004a4c78, G(0x0050e10c), 0xd5, 0, 0);
    make(m, 0x0050e414, 0x1cc, 0x004a490c, hwnd, 0x96, 0x004e0718, s414, 5);
    make(m, 0x0050e418, 0x1cc, 0x004a490c, hwnd, 0x97, 0x004e0719, s418, 5);
    c4(m, 0x004a4de0, G(0x0050e414), 0xcc0ff, 0xc0c0c, 0);
    c4(m, 0x004a4de0, G(0x0050e418), 0xcc0ff, 0xc0c0c, 0);
    st[0] = 0;
    m->call(m->user, 0x004a4b50, G(0x0050e414), 0xca, 0xc7, 0, st, 1);
    m->call(m->user, 0x004a4b50, G(0x0050e418), 0x78, 0x77, 0, st, 1);
    make(m, 0x0050e41c, 0x1cc, 0x004a490c, hwnd, 0x98, 0x004e071a, s41c, 5);
    make(m, 0x0050e420, 0x1cc, 0x004a490c, hwnd, 0x99, 0x004e071b, s420, 5);
    c4(m, 0x004a4de0, G(0x0050e41c), 0xcc0ff, 0xc0c0c, 0);
    c4(m, 0x004a4de0, G(0x0050e420), 0xcc0ff, 0xc0c0c, 0);
    m->call(m->user, 0x004a4b50, G(0x0050e41c), 0xb3, 0xb2, 0, st, 1);
    m->call(m->user, 0x004a4b50, G(0x0050e420), 0xaf, 0xae, 0, st, 1);
    for (i = 0; i < 3; i++)
        w32(m, devs[i][1], (c2(m, 0x004932a0, devs[i][0], 0) & 0xff) == 2 ? 0x08000000u : 0);
    w32(m, 0x0050e5b8, 0);

    for (row = 0, y = 0x4d; row < 16; row++, y += 0x16) {
        uint32_t sl[5] = { 1, 0x10, 0, 0x8c, 0x14 }, sr[5] = { 1, 0x1e4, 0, 0x8c, 0x14 };
        uint32_t lrec = 0x0050e110u + 16u * (uint32_t)row, rrec = 0x0050e210u + 16u * (uint32_t)row;
        sl[2] = sr[2] = y;
        k = r8(m, 0x004e7a48 + 8u * (uint32_t)row);
        make(m, lrec + 0x10, 0x1cc, 0x004a490c, hwnd, 0xb4u + (uint32_t)row, 0x004e071c, sl, 5);
        c4(m, 0x004a4de0, G(lrec + 0x10), 0xcc0ff, 0xc0c0c, 0);
        v = c2(m, 0x004a1f8c, k, 0) & 0xff;
        c2(m, 0x004a6f08, G(lrec + 0x10), r32(m, 0x004e755c + 4 * v));
        method(m, lrec + 0x10, 0xe0, 0x00461c10);
        w8(m, lrec + 0xd, v);
        w32(m, lrec + 4, 1);
        w8(m, lrec + 0xc, k);
        w32(m, lrec + 8, 0xb4u + (uint32_t)row);
        m->call(m->user, 0x004a4b50, G(lrec + 0x10), 0xb7, 0xb6, 0, st, 1);
        c2(m, 0x004a6ee4, G(lrec + 0x10), 5);
        make(m, rrec + 0x10, 0x1cc, 0x004a490c, hwnd, 0xc8u + (uint32_t)row, 0x004e071d, sr, 5);
        c4(m, 0x004a4de0, G(rrec + 0x10), 0xcc0ff, 0xc0c0c, 0);
        v = c2(m, 0x004a2758, k, 0) & 0xff;
        c2(m, 0x004a6f08, G(rrec + 0x10), r32(m, 0x004e797c + 4 * v));
        method(m, rrec + 0x10, 0xe0, 0x00461dc0);
        w8(m, rrec + 0xd, v);
        w32(m, rrec + 4, 2);
        w8(m, rrec + 0xc, k);
        w32(m, rrec + 8, 0xc8u + (uint32_t)row);
        m->call(m->user, 0x004a4b50, G(rrec + 0x10), 0xb7, 0xb6, 0, st, 1);
        c2(m, 0x004a6ee4, G(rrec + 0x10), 5);
    }
    c2(m, 0x00460438, 0, 0);
    c2(m, 0x004604f4, 0, 0);
    for (i = 0; i < 6; i++)
        method(m, click[i][0], 0xe0, click[i][1]);
    link(m, G(0x0050e108), G(0x0050e41c), G(LEFT(0)), G(0x0050e10c), G(0x0050e10c), G(0x0050e420), G(LEFT(0)));
    link(m, G(0x0050e10c), G(0x0050e420), G(RIGHT(0)), G(0x0050e108), G(0x0050e108), G(LEFT(0)), G(RIGHT(0)));
    link(m, G(0x0050e41c), G(LEFT(15)), G(0x0050e108), G(0x0050e420), G(0x0050e414), G(RIGHT(0)), G(0x0050e414));
    link(m, G(0x0050e414), G(LEFT(15)), G(0x0050e108), G(0x0050e41c), G(0x0050e418), G(0x0050e41c), G(0x0050e418));
    link(m, G(0x0050e418), G(RIGHT(15)), G(0x0050e10c), G(0x0050e414), G(0x0050e420), G(0x0050e414), G(0x0050e420));
    link(m, G(0x0050e420), G(RIGHT(15)), G(0x0050e10c), G(0x0050e418), G(0x0050e41c), G(0x0050e418), G(0x0050e108));
    /* each row: up / down wrap through all 16 (prev = (row + 15) % 16), across to the other list */
    for (row = 0; row < 16; row++) {
        prev = (row + 15) % 16;
        next = (row + 1) % 16;
        link(m, G(LEFT(row)), G(LEFT(prev)), G(LEFT(next)), G(RIGHT(row)), G(RIGHT(row)), G(0x0050e108),
             G(0x0050e10c));
        link(m, G(RIGHT(row)), G(RIGHT(prev)), G(RIGHT(next)), G(LEFT(row)), G(LEFT(row)), G(0x0050e10c),
             G(0x0050e41c));
    }
    link(m, G(LEFT(0)), G(0x0050e108), G(LEFT(1)), G(RIGHT(0)), G(RIGHT(0)), G(0x0050e108), G(0x0050e10c));
    link(m, G(RIGHT(0)), G(0x0050e10c), G(RIGHT(1)), G(LEFT(0)), G(LEFT(0)), G(0x0050e10c), G(0x0050e41c));
    link(m, G(LEFT(15)), G(LEFT(14)), G(0x0050e41c), G(RIGHT(15)), G(RIGHT(15)), G(0x0050e108), G(0x0050e10c));
    link(m, G(RIGHT(15)), G(RIGHT(14)), G(0x0050e420), G(LEFT(15)), G(LEFT(15)), G(0x0050e10c), G(0x0050e41c));
    c2(m, 0x004a8e6c, 0, 0);
    c2(m, 0x004a6f80, G(0x0050e414), 0);
    c2(m, 0x0046086c, 0, 0);
    c2(m, 0x00460ea0, 0, 0);
    for (i = 0; i < 6; i++)
        c2(m, 0x004a6ee4, G(shown[i]), 5);
}
