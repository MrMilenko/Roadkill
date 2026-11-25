/* gp2_damage.c: see gp2_damage.h */
#include "gameplay/gp2_damage.h"

#include "math/tm2math.h"

static uint8_t* at(Gp2DamageEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }

void gp2_car_damage(Gp2DamageEnv* e, int32_t obj, int32_t amount, uint32_t pos, int32_t kind,
                    int16_t attacker) {
    uint32_t human_b = 0, ai_b = 0;
    int32_t rate = (amount > 2 ? 0x10 : 6) / (int32_t)tm2_frame_ticks60();
    uint8_t rgb[3];
    if (kind == 0) {
        rgb[0] = rgb[1] = rgb[2] = 0xFF;
    } else {
        rgb[0] = 0xFF;
        rgb[1] = rgb[2] = 0;
    }
    uint8_t att_kind = 0;
    if (attacker >= 0) {
        uint32_t ac = e->attacker_car(e->user, attacker, &att_kind);
        if (att_kind != 0 && w_i16(at(e, ac + 0x1B2, 2), 0) == 100)
            amount = (int32_t)((uint32_t)amount * 2);
    }
    uint8_t human = 0;
    int32_t idx = 0;
    if (e->find_by_id(e->user, obj, &human, &idx) == 0)
        return;

    uint32_t car_a, car_b, stats_a, body, flags_a, frozen_a, wobj, state_a, killer_a, last_a;
    uint16_t type;
    uint8_t visible;
    uint32_t held;
    if (human) {
        car_a = e->human_car(e->user, idx);
        car_b = e->human_car(e->user, idx);
        human_b = car_b;
        stats_a = car_b + 0x138;
        body = car_b + 0x6C4;
        flags_a = car_b + 0x9A;
        frozen_a = car_b + 0x848;
        wobj = e->human_obj(e->user, idx);
        uint8_t* c = at(e, car_b, 0x898);
        type = w_u16(c, 0x90);
        state_a = car_b + 0x13D;
        killer_a = car_b + 0x13F;
        visible = 1;
        last_a = car_b + 0x140;
        held = w_u32(c, 0xC8);
    } else {
        car_a = e->ai_car(e->user, idx);
        car_b = e->ai_car(e->user, idx);
        ai_b = car_b;
        stats_a = car_b + 0x4EC;
        body = car_b + 0x708;
        flags_a = car_b + 0x68;
        frozen_a = car_b + 0x834;
        wobj = e->ai_obj(e->user, idx);
        uint8_t* c = at(e, car_b, 0x880);
        visible = c[0x32];
        type = w_u16(c, 0x30);
        state_a = car_b + 0x4F1;
        killer_a = car_b + 0x4F3;
        last_a = car_b + 0x4F4;
        held = w_u32(c, 0x94);
    }
    uint8_t* S = at(e, stats_a, 0xA8);
    uint8_t* F = at(e, flags_a, 0x20);

    if (kind == 1 && amount > 10) {
        int32_t push = (int32_t)((uint32_t)amount * 0xF0) / 0x19 + 0x28;
        push = (int32_t)((uint32_t)push - (uint32_t)((int32_t)((uint32_t)w_i16(S, 0x7A) * (uint32_t)push) / 100));
        if (pos != 0)
            e->knock_back(e->user, car_a, human, pos, push);
    }

    if (F[0x0E]) {                                    /* frozen: only the pending freeze */
        if (F[0x1A] == 0)
            return;
        F[0x1A] = 0;
        uint32_t spr = w_u32(at(e, frozen_a + 0x44, 4), 0);
        if (spr != 0)
            w_s16(at(e, spr + 0x0C, 2), 0, 0);
        if (!human)
            return;
        e->freeze_burst(e->user, human_b + 0x6E4, 0);
        e->freeze_glow(e->user, human_b + 0x6E4, 0);
        return;
    }
    if (visible == 0)
        return;
    if (e->flag_502f94(e->user) != 0 && type != 0xF0)
        return;
    e->flicker(e->user, wobj, rate, rgb, amount);
    if (amount <= 0)
        return;
    if (amount > 7) {
        int32_t r = e->rand(e->user);
        if ((r & 7) < amount - 7 && F[0x15] == 0)
            e->spin_hit(e->user, car_a, human, 4, 0);
    }
    if (amount >= 0) {                                /* always true here */
        int32_t n = amount < 3 ? 3 : amount;
        if (n < 8)
            n = amount < 3 ? 3 : amount;
        else
            n = 8;
        e->debris(e->user, n, wobj + 0xE8);
    }
    if (w_i16(S, 0x3C) != 0)
        amount /= 2;
    if (!human && att_kind == 0)
        amount /= 4;
    if (w_i32(S, 0xA0) > 0 && last_a != 0 && attacker >= 0)
        *at(e, last_a, 1) = (uint8_t)attacker;

    if (w_u32(S, 0xA4) != 0) {
        if (e->singleplayer(e->user) == 0) {
            if (human) {
                if (idx == 0)
                    w_s32(S, 0xA0, (uint32_t)w_i32(S, 0xA0) - (uint32_t)amount);
            } else if (attacker >= 0) {
                w_s32(S, 0xA0, (uint32_t)w_i32(S, 0xA0) - (uint32_t)amount);
                w_s16(S, 0x8C, (uint32_t)w_i16(S, 0x8C) - (uint32_t)amount);
            }
        }
        if (w_i32(S, 0xA0) <= 0 && F[0] == 0 && kind == 0 && e->multiplayer(e->user) == 0 && amount < 5)
            w_s32(S, 0xA0, (uint32_t)w_i32(S, 0xA0) + (uint32_t)amount);
        if (w_i32(S, 0xA0) <= 0 && F[0] == 0) {
            e->body_hide(e->user, (int16_t)type, 0);
            e->hud_msg(e->user, e->alive_count(e->user), 0);
            if (killer_a != 0 && attacker >= 0)
                *at(e, killer_a, 1) = (uint8_t)attacker;
            if (held != 0) {
                w_s32(at(e, held + 8, 4), 0, 0);
                if (human)
                    w_s32(at(e, human_b + 0xC8, 4), 0, 0);
                else
                    w_s32(at(e, ai_b + 0x94, 4), 0, 0);
            }
        }
        if (w_i32(S, 0xA0) <= 0 && (int8_t)*at(e, state_a, 1) != 4) {
            if (e->event_type(e->user) == 6 && F[0x1F] != 0)
                goto tail;
            if (type == 0xF0) {
                kind = 1;
                amount = 6;
            }
            if (kind != 0 && amount > 5 && e->game_mode(e->user) < 5) {
                int16_t dir[3], spin[3] = {0, 0, 0};
                int32_t drift[3] = {0, 0, 0}, near[3];
                if (pos != 0) {
                    const uint8_t* p = at(e, pos, 10);
                    dir[0] = w_i16(p, 0);
                    dir[1] = w_i16(p, 4);
                    dir[2] = w_i16(p, 8);
                } else {
                    dir[0] = 0;
                    dir[1] = 0;
                    dir[2] = 0xFFF;
                }
                int32_t t = e->nearest_player(e->user, wobj + 0xE8, -1);
                uint32_t h = e->human_car(e->user, t);
                const uint8_t* hp = at(e, h + 0x6E4, 12);
                for (int i = 0; i < 3; i++)
                    near[i] = w_i32(hp, 4 * i);
                if (type == 0xF0) {
                    dir[0] = 0;
                    dir[1] = 0;
                    dir[2] = 0xFFF;
                    uint32_t node;
                    int32_t scale;
                    if (e->boss_defeated(e->user, h) != 0) {
                        node = e->target_field(e->user);
                        scale = 10;
                        w_s32(at(e, e->target_obj(e->user) + 8, 4), 0, 0);
                    } else {
                        node = w_u32(at(e, wobj + 0x15C, 4), 0);
                        scale = 0x1E;
                    }
                    if (e->game_mode(e->user) != 2)
                        e->burst(e->user, node, body + 0x20, dir, spin, drift, 0, scale, 100, 0);
                    *at(e, state_a, 1) = 3;
                    e->explosion(e->user, wobj + 0xE8, 0);
                    e->smoke(e->user, wobj + 0xE8, 0);
                } else {
                    uint32_t node = e->car_object(e->user, (int16_t)type);
                    e->break_model(e->user, wobj, node, near, dir, 10, 200);
                    *at(e, state_a, 1) = 4;
                }
                if (wobj != 0)
                    w_s32(at(e, wobj + 8, 4), 0, 0);
                goto tail;
            }
            if (F[0] == 0) {
                e->spin_hit(e->user, car_a, human, 4, 0);
                if ((int8_t)S[5] != 3) {
                    e->choice_state(e->user, (int16_t)type, 3, (int8_t)S[4]);
                    *at(e, state_a, 1) = 3;
                    e->smoke_small(e->user, body + 0x20, 0);
                }
            }
        }
    }
tail:
    if (F[0x0D] != 0 && F[0] == 0) {
        int32_t cut = (int32_t)((uint32_t)amount * 0x3C) / 10;
        w_s16(S, 0x1A, (uint32_t)(w_i16(S, 0x1A) - cut));
        if (w_i16(S, 0x1A) < 0)
            w_s16(S, 0x1A, 0);
    }
}
