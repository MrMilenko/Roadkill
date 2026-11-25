/*
 * cb2_launch.c: weaponLaunchProjectile, TM2.EXE 0x00444004 (see cb2_launch.h).
 */
#include "cb2_launch.h"

#include <string.h>

static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }

void cb2_weapon_launch(const Cb2LaunchEnv* e, uint8_t* car, uint32_t kind, uint8_t human) {
    uint8_t *stats, *body, *ws, *flags;
    const uint8_t* model;
    int32_t id;
    if (human) {
        stats = car + 0x138; body = car + 0x6c4; ws = car + 0x848; flags = car + 0x9a;
        model = e->model(e->user, 1, rd16(car));
        id = rd16(car);
    } else {
        stats = car + 0x4ec; body = car + 0x708; ws = car + 0x834; flags = car + 0x68;
        model = e->model(e->user, 0, rd16(car + 4));
        id = rd16(car + 4);
    }
    int32_t pos[3], ang[3], target = 0;
    pos[0] = (int32_t)rd32(body + 0x20);
    pos[1] = (int32_t)rd32(body + 0x24);
    pos[2] = (int32_t)(rd32(body + 0x28) + (uint32_t)(int32_t)rd16(stats + 0x44) + 8u);

    uint32_t fn;
    int32_t n, snd;
    int with_target = 1;
    switch (kind) {
    case 1: fn = 0x00413f4c; n = 7; snd = 0x0a; break;
    case 2: fn = 0x00414458; n = 10; snd = 0x0b; break;
    case 4: fn = 0x00414178; n = 0x0f; snd = 0x0c; with_target = 0; break;
    case 5: case 0x66: fn = ws[9] ? 0x00414618 : 0x00414530; n = ws[9] ? 0x1e : 10; snd = 0x0f; break;
    case 0x64: fn = 0x00414114; n = 7; snd = 0x0e; break;
    default: return;    /* index -1: nothing */
    }
    if (with_target) e->pick_target(e->user, ws[5] ? 1 : 0, id, &target, human);
    ang[0] = (int32_t)rd32(body + 0x2c);
    if (ws[5]) {
        ang[1] = 0;
        ang[2] = (int32_t)((rd32(body + 0x34) + 0x800u) & 0xfffu);
    } else {
        ang[1] = (int32_t)rd32(body + 0x30);
        ang[2] = (int32_t)rd32(body + 0x34);
    }
    int32_t index = (int16_t)e->launch(e->user, fn, rd32(model + 0x158), with_target ? target : 0, ang, pos, n);
    if ((human || car[0x32]) && index >= 0) {
        if (e->sound_mode(e->user)) e->sound_at(e->user, pos, 1);
        else e->sound_at(e->user, pos, 0);
        uint32_t c = e->sound_get(e->user, 0);          /* 0x0046f990 first */
        uint32_t b = e->sound_get(e->user, 1);
        e->sound_play(e->user, kind != 0x64, snd, b, c, 99, index + 1);
        if (kind == 0x66 && human && id == 0) e->hud(e->user, 0x35, 0);
    }
    if (index >= 0) {
        flags[9] = 1;
        uint8_t* p = e->projectile(e->user, index);
        int32_t d = rd16(stats + 0x42) - 0x18;
        if (d <= 0) d = 0;
        int16_t v = (int16_t)d;
        memcpy(p + 0x46, &v, 2);
    }
}
