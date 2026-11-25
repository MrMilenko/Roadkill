/*
 * gp4_score.c: the score / player list overlay, TM2.EXE 0x00437134 (see gp4_score.h).
 */
#include "gp4_score.h"

#include <math.h>
#include <string.h>

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }

#define STR(h) ((const char*)e->mem(e->user, (h)))

/* strcpy as the original does it, two bytes a round */
static void copy(char* d, const char* s) {
    for (;;) {
        d[0] = s[0];
        if (!s[0]) return;
        d[1] = s[1];
        if (!s[1]) return;
        d += 2;
        s += 2;
    }
}

static void cut(const Gp4ScoreEnv* e, char* buf, int32_t room, int32_t dots) {
    int32_t w = e->width(e->user, 7, buf);
    if (w <= room) return;
    int32_t excess = dots + (w - room);
    int32_t n = (int32_t)strlen(buf);
    while (excess > 0) {
        excess -= e->char_width(e->user, 7, (uint8_t)buf[n - 1]);
        n--;
    }
    buf[n] = '.';
    buf[n + 1] = '.';
    buf[n + 2] = '.';
    buf[n + 3] = 0;
}

static void digits(int32_t tens_src, int32_t units_src, char* a, char* b) {
    a[0] = (char)((tens_src % 100) / 10 + 0x30);
    b[0] = (char)((units_src % 100) % 10 + 0x30);
}

/* the two digits right aligned at x */
static void number(const Gp4ScoreEnv* e, char* a, char* b, int32_t x, int32_t y) {
    if (a[0] == '0') a[0] = ' ';
    int32_t c1 = a[0] == '1', c2 = b[0] == '1';
    int32_t w0 = e->width(e->user, 7, a);
    int32_t w1 = e->width(e->user, 7, b);
    e->draw(e->user, 7, x - w0 - w1 - c1 - c2, y, a);
    e->draw(e->user, 7, x - w1 - c2, y, b);
}

void gp4_score_draw(const Gp4ScoreEnv* e) {
    char buf[200], b1[4];
    int32_t W = (int32_t)e->call(e->user, 0x4958d8, 0, 0);
    int32_t q = (W * 100) / 0x140;
    int32_t x0 = W - q - 8;
    if ((uint8_t)e->call(e->user, 0x4632b0, 0, 0) == 0) {
        if (*e->mode > 1) *e->mode = 0;
        int32_t y = 0xf;
        for (int32_t i = 0; i < (int32_t)e->call(e->user, 0x45b078, 0, 0); i++, y += 0xb) {
            uint32_t c = e->call(e->user, 0x45b080, (uint32_t)i, 0);
            if (c == 0 || rd32(e->mem(e->user, c) + 0x58c) <= 0) continue;
            int32_t k = (int32_t)e->call(e->user, 0x45bb20, (uint32_t)(int32_t)rd16(e->mem(e->user, c) + 0x30), 0);
            e->draw(e->user, 2, W - 100, y, STR(e->names[k]));
        }
        return;
    }
    int32_t half = q / 2;
    int32_t dots = e->width(e->user, 7, STR(0x4e0004));
    if (*e->mode == 1) {
        int sc = (uint8_t)e->call(e->user, 0x474500, 0, 0) == 7;
        e->draw(e->user, 2, x0, 0xf, STR(0x4e0008));
        e->draw(e->user, 2, W - 0x2e, 0xf, STR(sc ? 0x4e000f : 0x4e0013));
        e->draw(e->user, 2, W - 0x18, 0xf, STR(sc ? 0x4e0011 : 0x4e0015));
        int32_t y = 0x1a;
        for (int32_t k = 0; k < (int32_t)e->call(e->user, 0x4632c8, 0, 0); k++, y += 0xb) {
            uint32_t idx = e->call(e->user, 0x464cdc, (uint32_t)k, 0);
            buf[1] = 0;
            b1[1] = 0;
            if (sc) {
                int32_t a1 = (int32_t)e->call(e->user, 0x45b9c4, idx, 0);
                int32_t a2 = (int32_t)e->call(e->user, 0x45b9c4, idx, 0);
                digits(a1, a2, buf, b1);
            } else {
                int32_t v = rd32(e->scores + 12 * idx + 4);
                digits(v, v, buf, b1);
            }
            number(e, buf, b1, W - 0x10, y);
            if (sc) {
                int32_t a1 = (int32_t)e->call(e->user, 0x45b9cc, idx, 0);
                int32_t a2 = (int32_t)e->call(e->user, 0x45b9cc, idx, 0);
                digits(a1, a2, buf, b1);
            } else {
                int32_t v = rd32(e->scores + 12 * idx);
                digits(v, v, buf, b1);
            }
            number(e, buf, b1, W - 0x24, y);
            copy(buf, STR(e->call(e->user, 0x4632e0, (uint32_t)k, 0)));
            cut(e, buf, half, dots);
            e->draw(e->user, 7, x0, y, buf);
        }
    } else if (*e->mode == 2) {
        double f = (double)W * (double)e->k[0];
        int32_t c = (int32_t)trunc(f * (double)e->k[1]);
        int32_t h2 = c / 2;
        int32_t xl = (int32_t)trunc((double)(W - c) - f * (double)e->k[2]);
        int32_t dots2 = e->width(e->user, 7, STR(0x4e0017));
        e->draw(e->user, 2, xl, 0xf, STR(0x4e001b));
        e->draw(e->user, 2, xl + h2, 0xf, STR(0x4e0022));
        int32_t y = 0x1a;
        for (int32_t k = 0; k < (int32_t)e->call(e->user, 0x4632c8, 0, 0); k++, y += 0xb) {
            copy(buf, STR(e->call(e->user, 0x4632e0, (uint32_t)k, 0)));
            cut(e, buf, h2, dots2);
            e->draw(e->user, 7, xl, y, buf);
            int8_t t = (int8_t)e->call(e->user, 0x4986dc, (uint32_t)*e->list_arg, (uint32_t)k);
            copy(buf, STR(e->call(e->user, 0x474d68, 0x624ea8, (uint32_t)(int32_t)t)));
            cut(e, buf, h2, dots2);
            e->draw(e->user, 7, xl + h2, y, buf);
        }
    }
}
