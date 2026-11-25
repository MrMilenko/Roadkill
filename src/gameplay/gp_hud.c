/* gp_hud.c: see gp_hud.h */
#include "gameplay/gp_hud.h"

#include "weapons/wmem.h"

static uint8_t* H(GpHudEnv* env, uint32_t a) { return env->d + (a - GP_HUD_BASE); }
static void b8(GpHudEnv* env, uint32_t a, uint32_t v) { *H(env, a) = (uint8_t)v; }
static void b16(GpHudEnv* env, uint32_t a, uint32_t v) { w_s16(H(env, a), 0, v); }

void gp_hud_mirror_init(GpHudEnv* env) {
    /* the stores in the original's order */
    b8(env, 0x4f71e3, 0x28);
    b16(env, 0x4f71e4, 0xd1); b16(env, 0x4f71e6, 0x12); b16(env, 0x4f71e8, 0x13a); b16(env, 0x4f71ea, 0x12);
    b16(env, 0x4f71f0, 0x13a); b16(env, 0x4f71ec, 0xd1);
    b16(env, 0x4f71fe, 0x32); b16(env, 0x4f7202, 0x32); b16(env, 0x4f7222, 0x32); b16(env, 0x4f721e, 0x32);
    b16(env, 0x4f723a, 0x32); b16(env, 0x4f7236, 0x32);
    b16(env, 0x4f7200, 0x13a); b16(env, 0x4f71f2, 0x14); b16(env, 0x4f71ee, 0x14);
    b8(env, 0x4f71e0, 0); b8(env, 0x4f71e1, 0);
    b16(env, 0x4f71fc, 0xd1); b16(env, 0x4f7204, 0xd1);
    b8(env, 0x4f71e2, 0);
    b16(env, 0x4f7208, 0x13a);
    b8(env, 0x4f71f8, 0); b8(env, 0x4f71f9, 0); b8(env, 0x4f71fa, 0);
    b8(env, 0x4f7210, 0); b8(env, 0x4f7211, 0); b8(env, 0x4f7212, 0);
    b8(env, 0x4f7213, 0x28);
    b16(env, 0x4f7214, 0xd1); b16(env, 0x4f721c, 0xd1);
    b8(env, 0x4f71fb, 0x28);
    b16(env, 0x4f7218, 0xd3); b16(env, 0x4f7220, 0xd3); b16(env, 0x4f7230, 0x13a); b16(env, 0x4f7238, 0x13a);
    b16(env, 0x4f7216, 0x14); b16(env, 0x4f721a, 0x14); b16(env, 0x4f722e, 0x14); b16(env, 0x4f7232, 0x14);
    b16(env, 0x4f720a, 0x34); b16(env, 0x4f7206, 0x34);
    b16(env, 0x4f722c, 0x138); b16(env, 0x4f7234, 0x138);
    b8(env, 0x4f7228, 0); b8(env, 0x4f7229, 0); b8(env, 0x4f722a, 0); b8(env, 0x4f722b, 0x28);
}

void gp_hud_panels_init(GpHudEnv* env) {
    for (uint32_t v = 0; v < 0x1000; v += 0x800) {
        uint32_t p = GP_HUD_BASE + v;
        static const uint8_t grey[] = {0x3c, 0x3d, 0x3e, 0x78, 0x79, 0x7a, 0x00, 0x01, 0x02, 0xb4, 0xb5, 0xb6};
        for (unsigned k = 0; k < sizeof grey; k++)
            b8(env, p + grey[k], 10);
        b8(env, p + 0x27, 0x28);
        b8(env, p + 0x24, 0);
        b8(env, p + 0x3f, 0x28);
        b8(env, p + 0x63, 0x28);
        b8(env, p + 0x25, 0xff);
        b8(env, p + 0x26, 0);
        uint32_t x = *env->scr_x;
        b16(env, p + 0x64, x);
        uint32_t y = *env->scr_y;
        b16(env, p + 0x66, y);
        x = *env->scr_x + 0x5a;
        b16(env, p + 0x68, x);
        b16(env, p + 0x70, x);
        y = *env->scr_y;
        b16(env, p + 0x6a, y);
        b16(env, p + 0x72, y + 10);
        b16(env, p + 0x6c, *env->scr_x);
        b16(env, p + 0x6e, y + 10);
        static const uint16_t code[] = {0x7b, 0x03, 0x9f, 0xb7, 0x607, 0x61f};
        for (unsigned k = 0; k < sizeof code / sizeof code[0]; k++)
            b8(env, p + code[k], 0x28);
        b8(env, p + 0x604, 0);
        b8(env, p + 0x606, 0x15);
        b8(env, p + 0x605, 0x64);
        b8(env, p + 0x61c, 0x11);
        b8(env, p + 0x61d, 0x26);
        b8(env, p + 0x61e, 5);
        if (env->split(env->user) == 0)
            gp_hud_mirror_init(env);
    }
}

void gp_hud_radar_arrows(GpHudEnv* env, int32_t id, uint32_t data) {
    uint32_t k = (uint32_t)(id - 0x50A);
    if (k > 3)
        return;
    static const uint32_t pair[4] = {0, 3, 1, 2};   /* the jump table's order */
    env->image_load(env->user, data, 0x004fb11cu + pair[k] * 0x10, 1);
    env->image_load(env->user, data, 0x004fb124u + pair[k] * 0x10, 1);
}

#define HB 0x004f6a3cu   /* the view 0 HUD block (4 bytes before env->d, never written) */

void gp_hud_dash_icons(GpHudEnv* env, const uint8_t* data) {
    if (data == 0 || data[0] != 1 || data[0x14] < 5)
        return;
    uint8_t buf[0x50];
    /* weapon icons */
    b16(env, HB + 0xD8, env->images(env->user, w_u32(data, 0x18), buf, 0, 10));
    const uint8_t* im = buf;
    for (int32_t i = 0; i < w_i16(H(env, HB + 0xD8), 0); i++, im += 8) {
        uint32_t p = HB + 0xDC + 0x18 * (uint32_t)i;
        b8(env, p + 0x07, 0x7C);
        b8(env, p + 0x10, im[0]);
        b16(env, p + 0x0C, 0x1E);
        b16(env, p + 0x0E, 7);
        b8(env, p + 0x11, im[1]);
        b8(env, p + 0x12, im[0] + 0x1E);
        b8(env, p + 0x13, im[1] + 7);
        w_s32(H(env, p + 0x14), 0, w_u16(im, 4));
    }
    /* the selected weapon icon */
    env->images(env->user, w_u32(data, 0x1C), buf, 0, 1);
    uint32_t q = HB + 0x740;
    b8(env, q + 0x07, 0x7C);
    b8(env, q + 0x10, buf[0]);
    b16(env, q + 0x0C, 0x1E);
    b16(env, q + 0x0E, 7);
    b8(env, q + 0x11, buf[1]);
    b8(env, q + 0x12, buf[0] + 0x1E);
    b8(env, q + 0x13, buf[1] + 7);
    b8(env, HB + 0x744, 0x24);
    b8(env, HB + 0x746, 0x24);
    b8(env, HB + 0x745, 0x96);
    w_s32(H(env, q + 0x14), 0, w_u16(buf, 4));
    /* the strip, loaded into the block */
    env->images(env->user, w_u32(data, 0x20), H(env, HB + 0x69C), HB + 0x69C, 10);
    int32_t slot = 0;   /* ORIGINAL BUG: uninitialised when the loop below does not run */
    uint32_t base = 0;
    for (int32_t k = 0; k < w_i16(H(env, HB + 0xD8), 0); k++) {
        for (uint32_t e = 0x18; e <= 0x30; e += 0x18) {
            b8(env, HB + base + e + 0x39B, 0x7C);
            b8(env, HB + base + e + 0x398, 0x24);
            b8(env, HB + base + e + 0x399, 0x96);
            b8(env, HB + base + e + 0x39A, 0x24);
        }
        w_s32(H(env, HB + base + 0x30 + 0x3C0), 0, w_u16(H(env, HB + 0x6A0), 0));
        base += 0x30;
        slot = 2;
    }
    for (uint32_t e = 0x18; e <= 0x30; e += 0x18) {
        b8(env, HB + e + 0x753, 0x7C);
        b8(env, HB + e + 0x750, 0x24);
        b8(env, HB + e + 0x751, 0x96);
        b8(env, HB + e + 0x752, 0x24);
    }
    w_s32(H(env, HB + (uint32_t)slot * 0x18 + 0x60 + 0x3C0), 0, w_u16(H(env, HB + 0x6A0), 0));
    /* the second icon row */
    b16(env, HB + 0xD8, env->images(env->user, w_u32(data, 0x24), buf, 0, 10));
    im = buf;
    for (int32_t i = 0; i < w_i16(H(env, HB + 0xD8), 0); i++, im += 8) {
        uint32_t r = HB + 0x18 * (uint32_t)i;
        b8(env, r + 0x24B, 0x7C);
        b8(env, r + 0x254, im[0] + 1);
        b8(env, r + 0x255, im[1] + 1);
        b16(env, r + 0x250, 0x20);
        b8(env, r + 0x256, *H(env, r + 0x254) + 0x20);
        b16(env, r + 0x252, 0x20);
        b8(env, r + 0x257, *H(env, r + 0x255) + 0x20);
        b8(env, r + 0x248, 0x80);
        b8(env, r + 0x249, 0x80);
        b8(env, r + 0x24A, 0x80);
        w_s32(H(env, r + 0x258), 0, w_u16(im, 4));
    }
    gp_hud_panels_init(env);
}
