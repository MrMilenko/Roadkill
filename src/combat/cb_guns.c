/* cb_guns.c: see cb_guns.h */
#include "combat/cb_guns.h"

#include "math/tm2math.h"

static uint8_t* gp(const CbGunsEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbGunsEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static int16_t rd16(const CbGunsEnv* e, uint32_t a) { return w_i16(gp(e, a, 2), 0); }

static void muzzle(const CbGunsEnv* e, uint32_t body, const int32_t v[3], int32_t out[3]) {
    int32_t m[9];
    for (int k = 0; k < 9; k++)
        m[k] = (int32_t)rd32(e, body + 0x50 + 4 * (uint32_t)k);
    tm2_mat_vec_mul_row(m, v, out);
    for (int k = 0; k < 3; k++)
        out[k] = (int32_t)((uint32_t)out[k] + rd32(e, body + 0x20 + 4 * (uint32_t)k));
}

void cb_update_guns(const CbGunsEnv* e, uint32_t car, int is_human) {
    uint8_t human = (uint8_t)is_human;
    uint32_t stats, body, gun, wblk, ang;
    int32_t idx;
    int16_t owner, type;
    if (human) {
        stats = car + 0x138; body = car + 0x6C4; gun = car + 0x848; wblk = car + 0x9A; ang = car + 0x108;
        idx = rd16(e, car);
        owner = (int16_t)(idx + 1);
        type = rd16(e, e->human_get(e->user, idx) + 0x90);
    } else {
        stats = car + 0x4EC; body = car + 0x708; gun = car + 0x834; wblk = car + 0x68; ang = car + 0x98;
        idx = rd16(e, car + 4);
        owner = (int16_t)(idx + 0x32);
        type = rd16(e, e->ai_get(e->user, idx) + 0x30);
    }
    if (*gp(e, wblk, 1) == 0 && *gp(e, wblk + 0x0D, 1) == 0) {
        e->locate(e->user, body + 0x20, e->split(e->user) == 0 ? 0 : -1);
        if (human) {
            if (idx == 0) {
                e->play_human(e->user, 9, 0, 0, 0x63, 0);
            } else {
                int32_t pan = e->pan(e->user);
                int32_t vol = e->volume(e->user);
                e->play_human(e->user, 9, vol, pan, 0x63, idx);
            }
        } else if (e->split(e->user) == 0) {
            int32_t pan = e->pan(e->user);
            int32_t vol = e->volume(e->user);
            e->play_ai(e->user, 9, vol, pan, 0x63, idx);
        } else {
            int32_t vol = e->volume(e->user);
            e->play_ai(e->user, 9, vol, 0, 0x63, idx);
        }
    }
    uint8_t* g = gp(e, gun, 0x2A);
    if (w_i16(g, 0x28) > 0 || *gp(e, wblk + 0x0D, 1) != 0) {
        w_s16(g, 0x28, (uint16_t)(w_u16(g, 0x28) - 1));
        return;
    }
    if (human && idx == 0)
        e->rumble(e->user, 7, g[7]);
    int32_t v[3], out[3], dir[3];
    int32_t sx = rd16(e, stats + 0xD4);
    v[0] = g[7] ? -sx : sx;
    v[1] = rd16(e, stats + 0xD6);
    v[2] = rd16(e, stats + 0xD8);
    g[7] = (uint8_t)(1 - g[7]);
    muzzle(e, body, v, out);
    int32_t yaw = (int32_t)(rd32(e, body + 0x34) - (uint32_t)(int32_t)rd16(e, ang + 0x2C));
    dir[0] = tm2_sin(yaw);
    dir[1] = tm2_cos((int32_t)(rd32(e, body + 0x34) - (uint32_t)(int32_t)rd16(e, ang + 0x2C)));
    dir[2] = (int32_t)(0u - (uint32_t)tm2_sin((int32_t)(rd32(e, body + 0x2C) - (uint32_t)(int32_t)rd16(e, ang + 0x28))));
    if (g[8])
        e->bullet(e->user, owner, 4, dir, out, 3);
    else
        e->bullet(e->user, owner, 0, dir, out, 1);
    e->muzzle_fx(e->user, out, 0);
    if ((uint16_t)type == 0xE0 && g[7]) {
        v[0] = 0;
        v[1] = -v[1];
        muzzle(e, body, v, out);
        for (int k = 0; k < 3; k++)
            dir[k] = (int32_t)(0u - (uint32_t)dir[k]);
        e->bullet(e->user, owner, 4, dir, out, 3);
        e->muzzle_fx(e->user, out, 0);
    }
    w_s16(g, 0x28, human ? 2 : 5);
}
