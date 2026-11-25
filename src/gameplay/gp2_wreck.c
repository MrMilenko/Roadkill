/* gp2_wreck.c: see gp2_wreck.h */
#include "gameplay/gp2_wreck.h"

#include "math/tm2math.h"

static uint32_t isqrt(uint32_t v) {
    uint32_t root = 0, bit = 0x40000000;
    do {
        if ((int32_t)v >= (int32_t)bit && (int32_t)(v - bit) >= (int32_t)root) {
            v = v - bit - root;
            root = (root >> 1) | bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    } while (bit != 0);
    return root;
}

void gp2_car_wreck_update(Gp2WreckEnv* e, uint32_t car_a, uint8_t human) {
    uint8_t* car = wmem_ptr(e->mem, car_a, 0x898);
    uint32_t S_a = car_a + (human ? 0x138 : 0x4EC);
    uint32_t B_a = car_a + (human ? 0x6C4 : 0x708);
    uint8_t* S = wmem_ptr(e->mem, S_a, 0xB4);
    uint8_t* B = wmem_ptr(e->mem, B_a, 0x34);
    uint8_t* F = car + (human ? 0x9A : 0x68);
    uint8_t* F2 = car + (human ? 0x848 : 0x834);
    int16_t type = w_i16(car, human ? 0x90 : 0x30);
    int32_t key = w_i16(car, human ? 0x00 : 0x04);
    uint8_t near_ = 1;
    if (!human) {
        uint8_t th = 0;
        uint32_t t = e->target_car(e->user, key, &th) + (th ? 0x6C4 : 0x708);
        const uint8_t* tb = wmem_ptr(e->mem, t, 0x28);
        uint32_t dx = w_u32(tb, 0x20) - w_u32(B, 0x20);
        uint32_t dy = w_u32(tb, 0x24) - w_u32(B, 0x24);
        uint32_t r = isqrt(dx * dx + dy * dy);
        w_s32(car, 0x28, r);
        near_ = (int32_t)r < 0x3020;
        car[0x32] = near_;
        if (F[0x1A] == 0 && F[0x0B] == 0 && F[0x21] == 0) {
            F[0x1A] = 1;
            uint32_t spr = e->flame(e->user, B_a + 0x20, 1);
            w_s32(F2, 0x44, spr);
            if (spr != 0)
                w_s16(wmem_ptr(e->mem, spr + 0x0C, 2), 0, 500);
        }
        w_s16(S, 0x22, 0);
    }
    if (e->rand(e->user) < 0x800 && F[0x1A] != 0) {
        uint32_t v = e->jitter(e->user, B_a + 0x20, 0x333, 0x333, 0x333);
        uint8_t* vp = wmem_ptr(e->mem, v, 12);
        w_s32(vp, 8, w_u32(vp, 8) + 0x78);
        if (e->rand(e->user) < 0x4000)
            e->smoke_small(e->user, v, 0);
        else
            e->smoke_a2(e->user, v, 0);
    }
    w_s16(S, 0x26, (uint32_t)(w_u16(S, 0x26) - (uint16_t)tm2_frame_ticks60()));
    int32_t t = w_i16(S, 0x26);
    int32_t pos[3];
    if (t > 0) {
        int32_t roll = w_i32(B, 0x30);
        if (F[0x11] != 0) {
            if (roll > -0x400) {
                int32_t v = (int32_t)((uint32_t)w_i16(S, 0x62) * (uint32_t)roll) / -0x400 + w_i16(S, 0x60);
                w_s16(B, 0x12, (uint32_t)(w_u16(B, 0x12) - (uint32_t)v));
                w_s16(B, 0x0A, (uint32_t)(w_i16(B, 0x0A) / 2));
            } else {
                w_s16(B, 0x12, 0);
            }
        } else {
            if (roll < 0x400) {
                int32_t v = (int32_t)((uint32_t)w_i16(S, 0x62) * (uint32_t)roll) / 0x400 + w_i16(S, 0x60);
                w_s16(B, 0x12, (uint32_t)(w_u16(B, 0x12) + (uint32_t)v));
                w_s16(B, 0x0A, (uint32_t)(w_i16(B, 0x0A) / 2));
            } else {
                w_s16(B, 0x12, 0);
            }
        }
        if (F[0x10] != 0) {
            if (w_i16(B, 0x0A) < w_i32(S, 0xB0))
                F[0x10] = 0;
            return;
        }
        if (F[0x1A] == 0 && F[0x0B] == 0 && F[0x21] == 0) {
            F[0x1A] = 1;
            w_s32(B, 0x28, w_u32(B, 0x28) - 0x28);
            w_s32(F2, 0x44, e->flame(e->user, B_a + 0x20, 0));
            w_s32(B, 0x28, w_u32(B, 0x28) + 0x28);
            if (w_u32(F2, 0x44) != 0)
                w_s16(wmem_ptr(e->mem, w_u32(F2, 0x44) + 0x0C, 2), 0, 500);
        }
        w_s16(S, 0x22, 0);
        if (w_i16(S, 0x16) >= 3)
            return;
        if (e->frame(e->user) % 4 != 0)
            return;
        if ((e->rand(e->user) & 1) == 0)
            return;
        w_s16(S, 0x16, (uint32_t)(w_i16(S, 0x16) + 1));
        F[0x13] = 1;
        e->rand(e->user);                       /* three offsets, overwritten below */
        e->rand(e->user);
        e->rand(e->user);
        pos[0] = w_i32(B, 0x20);
        pos[1] = w_i32(B, 0x24);
        pos[2] = (int32_t)(w_u32(B, 0x28) + 0x78);
        if (e->rand(e->user) & 1)
            e->expl12(e->user, pos, 0);
        else
            e->expl16(e->user, pos, 0);
        e->smoke(e->user, pos, 0);
        return;
    }
    if (!near_ || t < -0x4B0) {
        if (human)
            return;
        car[0x32] = 1;
        uint32_t obj = e->object_from_owner(e->user, type);
        e->damage(e->user, w_i32(wmem_ptr(e->mem, obj + 0x158, 4), 0), 0x0F, 0, 2, 0);
        pos[0] = w_i32(B, 0x20);
        pos[1] = w_i32(B, 0x24);
        pos[2] = (int32_t)(w_u32(B, 0x28) + 0x78);
        e->expl16(e->user, pos, 0);
        pos[2] = (int32_t)((uint32_t)pos[2] + (uint32_t)w_i16(S, 0x42));
        e->smoke(e->user, pos, 0);
        uint32_t spr = w_u32(F2, 0x44);
        if (spr != 0)
            w_s16(wmem_ptr(e->mem, spr + 0x0C, 2), 0, 0);
        F[0x1A] = 0;
        return;
    }
    if (t < -0x3C0) {
        if (human || F[0x26] != 0)
            return;
        pos[0] = w_i32(B, 0x20);
        pos[1] = w_i32(B, 0x24);
        pos[2] = (int32_t)(w_u32(B, 0x28) + 0x18);
        e->expl12(e->user, pos, 0);
        pos[2] = (int32_t)((uint32_t)pos[2] + (uint32_t)w_i16(S, 0x42));
        e->smoke(e->user, pos, 0);
        F[0x26] = 1;
        return;
    }
    w_s16(B, 0x08, 0);
    int32_t r = w_i16(B, 0x0A);
    w_s16(B, 0x10, 0);
    w_s16(B, 0x12, 0);
    w_s16(B, 0x14, 0);
    w_s16(B, 0x0C, 0);
    w_s16(B, 0x0A, (uint32_t)(r - r / 16));
    if (e->game_mode(e->user) == 7 && human && key == 0) {
        if (w_i16(S, 0x26) >= -0x12C)
            return;
        e->car_setup(e->user, car_a, type, 1);
        e->net_send(e->user, e->net_msg(e->user, 0, w_u32(car, 0x894)));
        e->net_flush(e->user);
        return;
    }
    int32_t a = w_i32(B, 0x30), target;
    if (a > 0x471) {
        if (a >= 0x78E)
            return;
        target = a < 0x600 ? 0x400 : 0x7F4;
    } else if (a > 0x71) {
        if (a >= 0x38E)
            return;
        target = a >= 0x200 ? 0x400 : 0;
    } else if (a < -0x71) {
        if (a <= -0x38E)
            return;
        target = a <= -0x200 ? 0x400 : 0;
    } else {
        return;                                 /* the (-0x78E, -0x471] case below -0x71 never runs */
    }
    w_s32(B, 0x30, (uint32_t)e->approach(e->user, a, target, 1));
}
