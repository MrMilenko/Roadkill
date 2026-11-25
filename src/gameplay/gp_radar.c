/* gp_radar.c: see gp_radar.h */
#include "gameplay/gp_radar.h"

#include "math/tm2math.h"
#include "weapons/wmem.h"

static int32_t iabs(int32_t v) {
    uint32_t m = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ m) - m);
}

/* colour bytes +4, +5, +6 by car type */
static void colour(uint32_t type, uint8_t c[3]) {
    static const struct { uint32_t t; uint8_t c[3]; } tab[] = {
        {0x10, {0xff, 0x90, 0xba}}, {0x20, {0xff, 0x00, 0x00}}, {0x30, {0x64, 0x64, 0xff}},
        {0x40, {0x82, 0xca, 0x36}}, {0x50, {0xc8, 0x00, 0x00}}, {0x60, {0x00, 0xff, 0x00}},
        {0x70, {0xff, 0x00, 0xff}}, {0x80, {0xff, 0xff, 0xff}}, {0x90, {0xdc, 0x64, 0x00}},
        {0xa0, {0xc8, 0xc8, 0xfa}}, {0xb0, {0x7d, 0x7d, 0x7d}}, {0xc0, {0xc8, 0xc8, 0x00}},
        {0xd0, {0xa0, 0x05, 0xc8}}, {0xf0, {0x00, 0x00, 0x00}},
    };
    for (unsigned i = 0; i < sizeof tab / sizeof tab[0]; i++)
        if (tab[i].t == type) {
            c[0] = tab[i].c[0], c[1] = tab[i].c[1], c[2] = tab[i].c[2];
            return;
        }
    c[0] = 0xff, c[1] = 0xf0, c[2] = 0x78;
}

void gp_radar_blip(GpRadarEnv* env, uint32_t type, const int32_t pos[2], int32_t angle, int32_t view) {
    uint8_t* v = env->views + view * 0x370;
    int32_t X = pos[0], Y = pos[1];
    int32_t scale = w_i32(v, 0x368);
    int32_t x = (int32_t)((uint32_t)X * (uint32_t)tm2_cos(angle) - (uint32_t)tm2_sin(angle) * (uint32_t)Y) / scale;
    int32_t y = (int32_t)(0u - ((uint32_t)X * (uint32_t)tm2_sin(angle) + (uint32_t)tm2_cos(angle) * (uint32_t)Y)) / scale;
    if (x < -0x23 || x > 0x23 || y < -0x16 || y > 0x15 || w_i32(v, 0x360) + 2 > 0x18) {
        if (x >= -0x23 && x < 0x24) {
            v[y > 0 ? 0x35D : 0x35C] = 1;
            return;
        }
        if (x < 0 && iabs(x) > iabs(y)) {
            v[0x35F] = 1;
            return;
        }
        if (x > 0 && iabs(x) > iabs(y)) {
            v[0x35E] = 1;
            return;
        }
        v[y > 0 ? 0x35D : 0x35C] = 1;
        return;
    }
    if (env->split(env->user) == 0) {
        x += *env->scr_x + 0x25;
        y += *env->scr_y + 0x17;
    }
    int32_t c = w_i32(v, 0x360);
    w_s32(v, 0x360, (uint32_t)(c + 1));
    int32_t c1 = w_i32(v, 0x360);
    w_s32(v, 0x360, (uint32_t)(c1 + 1));
    uint8_t* a = v + 0x30 + c * 16;
    uint8_t* b = v + 0x30 + c1 * 16;
    if (env->wide(env->user) == 0) {
        w_s16(a, 0x0C, (uint32_t)x); w_s16(a, 0x0E, (uint32_t)y);
        w_s16(a, 0x08, (uint32_t)(x - 1)); w_s16(a, 0x0A, (uint32_t)(y - 1));
        w_s16(b, 0x0A, (uint32_t)y); w_s16(b, 0x0C, (uint32_t)x);
        w_s16(b, 0x08, (uint32_t)(x - 1)); w_s16(b, 0x0E, (uint32_t)(y - 1));
    } else {
        w_s16(a, 0x08, (uint32_t)(x - 1)); w_s16(a, 0x0C, (uint32_t)(x + 1));
        w_s16(a, 0x0A, (uint32_t)(y - 1)); w_s16(a, 0x0E, (uint32_t)(y - 1));
        w_s16(b, 0x0A, (uint32_t)y); w_s16(b, 0x0C, (uint32_t)(x + 1));
        w_s16(b, 0x0E, (uint32_t)y); w_s16(b, 0x08, (uint32_t)(x - 1));
    }
    uint8_t col[3];
    colour(type, col);
    a[5] = col[1]; a[6] = col[2]; a[4] = col[0];
    b[4] = col[0]; b[5] = col[1]; b[6] = col[2];
}

void gp_radar_init(GpRadarEnv* env) {
    for (int view = 0; view < 2; view++) {
        uint8_t* r = env->views + view * 0x370;
        r[0x1F] = 0x28; r[0x05] = 0x26; r[0x07] = 0x28; r[0x04] = 0x11;
        r[0x06] = 5; r[0x1C] = 0x24; r[0x1D] = 0x96; r[0x1E] = 0x24;
        for (int k = 0; k < 48; k++)
            r[0x37 + 0x10 * k] = 0x40;
        r[0x337] = 0x40; r[0x347] = 0x40;
        r[0x334] = 0x24; r[0x336] = 0x24; r[0x344] = 0x24; r[0x346] = 0x24;
        r[0x335] = 0x96; r[0x345] = 0x96;
        if (env->split(env->user) == 0) {
            uint32_t X = (uint16_t)*env->scr_x, Y = (uint16_t)*env->scr_y;
            w_s16(r, 0x20, X - 3); w_s16(r, 0x22, Y - 3); w_s16(r, 0x24, X + 0x4C); w_s16(r, 0x26, Y - 3);
            w_s16(r, 0x2C, X + 0x4C); w_s16(r, 0x28, X - 3); w_s16(r, 0x2E, Y + 0x30); w_s16(r, 0x2A, Y + 0x30);
            uint8_t wide = env->wide(env->user);
            X = (uint16_t)*env->scr_x;
            Y = (uint16_t)*env->scr_y;
            if (wide) {
                w_s16(r, 0x33A, Y + 0x16); w_s16(r, 0x33E, Y + 0x16); w_s16(r, 0x338, X);
                w_s16(r, 0x348, X + 0x25); w_s16(r, 0x33C, X + 0x49); w_s16(r, 0x34C, X + 0x25);
                w_s16(r, 0x34A, Y); w_s16(r, 0x34E, Y + 0x2D);
            } else {
                w_s16(r, 0x348, X + 0x25); w_s16(r, 0x34A, Y + 0x16); w_s16(r, 0x338, X + 0x23);
                w_s16(r, 0x34C, X + 0x25); w_s16(r, 0x33A, Y + 0x17); w_s16(r, 0x34E, Y + 0x18);
                w_s16(r, 0x33C, X + 0x27); w_s16(r, 0x33E, Y + 0x17);
            }
        }
        w_s32(r, 0x364, 8000);
        w_s32(r, 0x368, (8000 << 12) / 0x18);
        w_s32(r, 0x36C, 0);
        w_s32(r, 0x360, 0);
    }
}

/* the frame quad R + j * 0x18 (j reread for every store) at the screen offset box */
static void frame_quad(GpRadarEnv* env, uint8_t* r, int mark) {
#define FQ(off) (r + w_i32(r, 0x36C) * 0x18 + (off))
    uint32_t X = (uint16_t)*env->scr_x, Y = (uint16_t)*env->scr_y;
    w_s16(FQ(0x08), 0, X);
    w_s16(FQ(0x0A), 0, (uint16_t)*env->scr_y);
    uint32_t x2 = (uint16_t)*env->scr_x + 0x49;
    w_s16(FQ(0x0C), 0, x2);
    w_s16(FQ(0x0E), 0, (uint16_t)*env->scr_y);
    w_s16(FQ(0x14), 0, x2);
    uint32_t y2 = (uint16_t)*env->scr_y + 0x2D;
    w_s16(FQ(0x16), 0, y2);
    w_s16(FQ(0x10), 0, (uint16_t)*env->scr_x);
    w_s16(FQ(0x12), 0, y2);
    if (mark)
        *FQ(0x07) |= 2;
    (void)Y;
#undef FQ
}

void gp_radar_draw(GpRadarEnv* env, int32_t view) {
    int wide = (int8_t)env->wide(env->user) != 0;
    env->begin(env->user);
    uint8_t* r = env->views + view * 0x370;
    uint32_t ra = env->views_addr + (uint32_t)view * 0x370;
    if (wide) {
        frame_quad(env, r, 1);
        env->add_prim(env->user, ra + (uint32_t)(w_i32(r, 0x36C) * 0x18));
        env->add_prim(env->user, ra + 0x330);
        env->add_prim(env->user, ra + 0x340);
    }
    static const uint16_t flag[4] = {0x35C, 0x35D, 0x35F, 0x35E};
    static const int32_t dx[4] = {0x1F, 0x1F, -7, 0x48}, dy[4] = {-7, 0x2C, 0x11, 0x11};
    for (int k = 0; k < 4; k++) {
        if (r[flag[k]] == 0)
            continue;
        uint8_t* p = env->arrows + view * 0x18 + k * 0x30;
        const uint8_t* im = env->images + view * 8 + k * 0x10;
        uint8_t up = k == 1;
        p[7] = 0x7C;
        p[4] = 0xFF;
        p[5] = 0xFF;
        w_s16(p, 0x08, (uint32_t)((uint16_t)*env->scr_x + dx[k]));
        p[6] = 0xFF;
        w_s16(p, 0x0A, (uint32_t)((uint16_t)*env->scr_y + dy[k]));
        w_s32(p, 0x14, w_u16(im, 4));
        p[0x10] = (uint8_t)(im[0] + 1);
        p[0x11] = (uint8_t)(im[1] + up);
        w_s16(p, 0x0C, im[2]);
        w_s16(p, 0x0E, im[3]);
        p[0x12] = (uint8_t)(im[0] + 1 + im[2]);
        p[0x13] = (uint8_t)(im[1] + up + im[3]);
        env->add_prim(env->user, env->arrows_addr + (uint32_t)(view * 0x18 + k * 0x30));
    }
    for (int32_t i = 0; i < w_i32(r, 0x360); i++)
        env->add_prim(env->user, ra + 0x30 + (uint32_t)i * 0x10);
    if (!wide) {
        env->add_prim(env->user, ra + 0x330);
        env->add_prim(env->user, ra + 0x340);
        frame_quad(env, r, 0);
        env->add_prim(env->user, ra + (uint32_t)(w_i32(r, 0x36C) * 0x18));
    }
    w_s32(r, 0x36C, 0);
    w_s32(r, 0x360, 0);
    r[0x35E] = 0;
    r[0x35F] = 0;
    r[0x35D] = 0;
    r[0x35C] = 0;
}
