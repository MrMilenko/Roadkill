/*
 * car_hit.c: collision hit records into per part hit summaries, ported from TM2.EXE
 * (see car_hit.h).
 */
#include "car_hit.h"

#include <stddef.h>

#include "../math/tm2math.h"

static int32_t i32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static void w32(uint8_t* b, int o, int32_t v) { for (int i = 0; i < 4; i++) b[o + i] = (uint8_t)((uint32_t)v >> (8 * i)); }
static void w16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)((uint32_t)v >> 8); }
static int32_t iabs(int32_t v) { return v < 0 ? (int32_t)(0u - (uint32_t)v) : v; }
static int32_t div4(int32_t v) { return v / 4; }   /* the original's signed shift with rounding toward zero */

void car_hits_resolve(uint8_t* car, int player, int quiet, const CarHitEnv* env) {
    uint8_t* base = car + (player ? 0x21c : 0xcc);
    uint8_t* ctl = car + (player ? 0x9a : 0x68);
    uint8_t* stats = car + (player ? 0x138 : 0x4ec);
    uint8_t* body = car + (player ? 0x6c4 : 0x708);
    uint8_t* turbo = car + (player ? 0x848 : 0x834);
    uint8_t* steer = car + (player ? 0xcc : 0x40);
    int32_t car_id = i16(car, player ? 0x90 : 0x30);
    int first = 1;      /* cleared once a special id is consumed */
    for (int i = 0; i < 6; i++) {
        uint8_t* rec = base + 0x18 + 0x20 * i;     /* +4 state */
        uint8_t* sum = base + 0x19c + 0x24 * i;
        if (i32(rec, 4) == 0) continue;
        int consumed = 0;
        int32_t ida = i16(rec, 0x22), idb = i16(rec, 0x20);
        if (ida == 0xbba) {
            w32(rec, 4, 0);
            consumed = 1;
        } else if (ida >= 0x191 && ida <= 0x1e0) {
            w32(rec, 4, 0);
            consumed = 1;
            if (first && !quiet && env->special) env->special(env->user, car, player, ida, idb);
        } else if (ida == 0x29a) {
            w32(rec, 4, 0);
            consumed = 1;
            if (first && ctl[0x1a] == 0 && ctl[0xe] == 0 && (steer[3] == 0 || i16(turbo, 0x32) < 0x32)) {
                ctl[0x1a] = 1;
                w16(stats, 0x22, 0);
                int32_t pos[3] = { i32(body, 0x20), i32(body, 0x24), i32(body, 0x28) };
                w32(turbo, 0x44, (int32_t)(env->effect ? env->effect(env->user, pos) : 0));
            }
        } else if (ida >= 0x2b6 && ida <= 0x2b9) {
            consumed = 1;
            if (first) {
                int any = 0, all = 1;
                for (int k = 0; k < 4; k++) {
                    if (base[0x19e + 0x24 * k]) any = 1;
                    else all = 0;
                }
                if (player ? (all || car_id == 0xe0) : any) {
                    if (env->event) env->event(env->user, ida - 0x2b6, car, player);
                    for (int k = 0; k < 4; k++) base[0x19e + 0x24 * k] = 0;
                }
            }
        } else if (ida == 500 || ida == 0x208) {
            w32(rec, 4, 0);
            consumed = 1;
            if (first && env->pickup) env->pickup(env->user, ida, idb, 1);
        } else if (ida == 0xe10) {
            if (!player) { w32(rec, 4, 0); consumed = 1; }
        } else if (car_id == 0xf0) {
            int r0 = i32(base, 0x1c) != 0, r1 = i32(base, 0x3c) != 0, r2 = i32(base, 0x5c) != 0, r3 = i32(base, 0x7c) != 0;
            if (r0 && r1 && r2 && r3) {
                if (!(env->check && env->check(env->user, ida)) && env->event) env->event(env->user, 2, car, 0);
                w32(rec, 4, 0);
                consumed = 1;
            } else if (r0 && r2) {
                if (!(env->check && env->check(env->user, ida))) {
                    if (env->set_state) env->set_state(env->user, car, 0, 4, 1);
                    int32_t a = tm2_wadd(i32(body, 0x34), 0x400);
                    tm2_angle_wrap32(&a);
                    w32(body, 0x40, a);
                }
            } else if (r1 && r3) {
                if (!(env->check && env->check(env->user, ida))) {
                    if (env->set_state) env->set_state(env->user, car, 0, 4, 1);
                    int32_t a = tm2_wadd(i32(body, 0x34), -0x400);
                    tm2_angle_wrap32(&a);
                    w32(body, 0x40, a);
                }
            } else if (i == 5) {
                w32(rec, 4, 0);
                consumed = 1;
            }
            w16(base, 0x10, 0);
        }
        if (consumed) {
            sum[2] = 1;
            first = 0;
            sum[1] = 0;
            continue;
        }
        sum[2] = 0;
        int drop = 0;
        int32_t thresh = player ? (tm2_frame_ticks60() * 0x2f8) / 100 << 5 : 0;
        if (i32(rec, 0xc) == 0) w32(rec, 0xc, 1);
        int32_t nx = i32(rec, 8), ny = i32(rec, 0xc), nz = i32(rec, 0x10);
        int32_t travel = i32(body, 0xf4);
        int32_t ang = tm2_atan2(nx, ny);
        if (nz < 0) {
            int32_t h = tm2_wadd(tm2_wmul(ny, ny), tm2_wmul(nx, nx)), v = tm2_wmul(nz, nz);
            if (tm2_wadd(v, -h) != 0 && h <= v) { drop = 1; ang = travel; }
        }
        if (i == 5) ang = 0x800 - travel;
        tm2_angle_wrap32(&ang);
        w16(sum, 0xa, ang);
        int32_t d = travel - ang;
        if (i16(body, 0x1a) < 0) d += 0x800;
        tm2_angle_wrap32(&d);
        int32_t bounce = d * -2 + 0x800;
        tm2_angle_wrap32(&bounce);
        w16(sum, 0xc, bounce);
        sum[3] = (uint8_t)i;
        w32(sum, 0x18, nx); w32(sum, 0x1c, ny); w32(sum, 0x20, nz);
        w16(sum, 0xe, i32(rec, 0x14));
        uint32_t ent = (uint32_t)i32(rec, 0x18);
        int32_t type = ent && env->entity_type ? env->entity_type(env->user, ent) : 0;
        if (ent == 0 || type > 5999 || type == 9) {
            sum[1] = 1; sum[0] = 1;
            w16(sum, 4, 0);
            w32(sum, 0x18, -nx); w32(sum, 0x1c, -ny); w32(sum, 0x20, -nz);
            int32_t speed = iabs(i16(body, 0xa));
            if (thresh < speed) {
                int32_t s = tm2_sin(div4(bounce));
                int32_t v = iabs(tm2_wmul(speed, s));
                w16(sum, 4, (int16_t)((int64_t)(tm2_wmul(v, 6) / i32(stats, 0xb4)) / 0xfff));
                int32_t n = (i16(stats, 0x7a) * i16(sum, 4)) / 0x21;
                if (!player) {
                    n += 4;
                    if (env->pickup) env->pickup(env->user, ida, idb, (int16_t)n);
                }
                if (n > 0) {
                    if (env->wall_damage) env->wall_damage(env->user, type, ida, idb, n, body + 0x38);
                    if (div4(i32(stats, 0xb4)) < i16(body, 0xa) && ida == 500) {
                        w16(sum, 4, 0);
                        w16(sum, 6, div4(i16(body, 0xa)));
                        w16(sum, 0xc, 0);
                        w16(sum, 8, i32(body, 0x40));
                        w16(sum, 0x10, i32(body, 0x40));
                    }
                }
            }
        } else {
            sum[1] = 0; sum[0] = 0;
            if (env->hit_entity && env->hit_entity(env->user, car, player, i)) drop = 1;
        }
        if (drop) w32(rec, 4, 0);
    }
}

static void copy24(uint8_t* d, const uint8_t* s) { for (int i = 0; i < 0x24; i++) d[i] = s[i]; }

void car_hits_gather(uint8_t* car, int player, int32_t points, const CarHitApplyEnv* env) {
    uint8_t* B = car + (player ? 0x21c : 0xcc);
    uint8_t* body = car + (player ? 0x6c4 : 0x708);
    uint8_t* ctl = car + (player ? 0x9a : 0x68);
    uint8_t* act = B + 0x34c;
    act[0] = 0;
    for (int32_t k = 0; k < points; k++) {
        uint8_t* sum = B + 0x19c + 0x24 * k;
        if (i32(B, 0x1c + 0x20 * k) == 0 || sum[2] != 0 || sum[1] == 0) continue;
        if (act[0] == 0 || act[2] != 0) {
            copy24(act, sum);
            act[0] = 1;
        } else if (act[1] == 0) {
            if (env->pass_car_hit) env->pass_car_hit(env->user, car, player, act, sum);
            copy24(act, sum);
        } else {
            if ((int8_t)act[3] < (int8_t)sum[3]) act[3] = sum[3];
            w16(act, 4, (i16(act, 4) + i16(sum, 4)) / 2);
            w16(act, 6, (i16(sum, 6) + i16(act, 6)) / 2);
            for (int o = 8; o <= 0xc; o += 4) {
                int32_t u = i16(act, o) - i16(sum, o);
                w16(act, o, (iabs(u) < 0x801 ? i16(sum, o) + i16(act, o) : -u) / 2);
            }
            if (i16(act, 0xe) < i16(sum, 0xe)) w16(act, 0xe, i16(sum, 0xe));
            for (int o = 0x18; o <= 0x20; o += 4) w32(act, o, tm2_wadd(i32(act, o), i32(sum, o)) / 2);
        }
    }
    if (act[0]) {
        int32_t v = iabs(i16(car, 0x6dc)) + iabs(i16(car, 0x6de)) + iabs(i16(car, 0x6e0));
        int32_t s5 = (v < 1000 || 1999 < v) ? 99 : (v * 0x30) / 2000 + 0x33;
        int32_t s3 = v < 1000 ? (v * 0x31) / 1000 + 0x32 : 99;
        int32_t pos[3] = { i32(body, 0x20), i32(body, 0x24), i32(body, 0x28) };
        if (env->crash) env->crash(env->user, pos, ctl[0] == 0, s5, s3);
    }
    if ((act[0] == 0 || act[2] != 0) && points > 0 && B[0x274] != 0 && env->car_contacts)
        env->car_contacts(env->user, car, player);
    B[2] = act[0];
    if (act[0] == 0) w16(B, 0x10, i16(B, 0x10) + 1);
    else w16(B, 0x10, 0);
}

static void norm16(uint8_t* b, int o) {
    int16_t a = i16(b, o);
    tm2_angle_wrap16(&a);
    w16(b, o, a);
}

void car_hit_turn(uint8_t* car, int player, int car_hit, int32_t angle, int32_t index, const CarHitApplyEnv* env) {
    uint8_t* stats = car + (player ? 0x138 : 0x4ec);
    uint8_t* steer = car + (player ? 0xcc : 0x40);
    uint8_t* ctl = car + (player ? 0x9a : 0x68);
    uint8_t* body = car + (player ? 0x6c4 : 0x708);
    uint8_t* B = car + (player ? 0x21c : 0xcc);
    if (!player) car[0x447] = 1;
    int32_t h = tm2_wadd(i32(body, 0x40), angle);
    tm2_angle_wrap32(&h);
    w32(body, 0x40, h);
    ctl[1] = 1;
    w32(body, 0x4c, 0);
    int32_t a = iabs(angle);
    int kind = a < 0xe3 ? 1 : a < 0x200 ? 2 : a < 0x600 ? 3 : 5;
    if (env->crash_state) env->crash_state(env->user, car, kind, player);
    int idx_ok = index == 0 || index == 1 || index == 4;
    B[4] = (player && ctl[6] && i16(body, 0xa) < (tm2_frame_ticks60() * 0x23a / 100) * 0x20 && idx_ok) ? 1 : 0;
    if (idx_ok || steer[0xe]) {
        if (B[4] == 0 && 0x2a9 < a) {
            if (0x554 < a) w16(B, 8, i16(body, 0x40) - 0x800);
            else w16(B, 8, (int16_t)i32(body, 0x40) - (int16_t)(angle / 2));
        } else {
            w32(body, 0x40, i32(body, 0x40) - angle / 3);
            w16(B, 8, i32(body, 0x40));
        }
        norm16(B, 8);
        B[3] = 1;
    }
    int32_t d = i32(body, 0x34) - i32(body, 0x40);
    tm2_angle_wrap32(&d);
    if (B[4] && ctl[6] && ((d < 0 && steer[0xf]) || (0 < d && steer[0x10]))) {
        w16(B, 8, i16(body, 0x40) - 0x800);
        norm16(B, 8);
    }
    if (player && ctl[6]) {
        int32_t s = (ctl[7] == 0) == (steer[0xf] == 0) ? 0x200 : -0x200;
        w16(B, 8, i16(body, 0x34) + s);
        norm16(B, 8);
        B[3] = 1;
    }
    if (car_hit && (i16(stats, 0x7a) * 3) / 0x23 + 3 < i16(B, 0x350)) {
        int32_t ad = iabs(d);
        if (ad < 0x6ab && 0x155 < ad) {
            ctl[0x12] = 1;
            w16(body, 2, i32(body, 0x40));
            w16(stats, 0x20, 0);
            if (d < 1) { w16(body, 0x12, -1); w16(body, 4, i16(body, 0x40) - 0x400); }
            else { w16(body, 0x12, 1); w16(body, 4, i16(body, 0x40) + 0x400); }
        }
    }
    int32_t sp = i16(body, 0xa), drop;
    if (iabs(sp) < (tm2_frame_ticks60() * 0x2f8 / 100) * 0x20) drop = 0;
    else if (a < 0x200) drop = div4(sp);
    else if (a < 0x400) drop = sp / 3;
    else drop = sp / 2;
    w16(body, 0xa, i16(body, 0xa) - drop);
    if (i16(B, 6) < 1) w16(B, 6, 0xb4);
    if (B[0x34d]) w16(B, 0x12, 0);
}

void car_hit_push(uint8_t* car, int player, const CarHitApplyEnv* env) {
    uint8_t* B = car + (player ? 0x21c : 0xcc);
    uint8_t* stats = car + (player ? 0x138 : 0x4ec);
    uint8_t* ctl = car + (player ? 0x9a : 0x68);
    uint8_t* body = car + (player ? 0x6c4 : 0x708);
    uint8_t* P = car + (player ? 0x58c : 0x5d0);
    int32_t idx = player ? i16(car, 0) : i16(car, 4);
    if (B[0x34c] && B[0x34e]) B[0x34c] = 0;
    if (!B[0x34c]) return;
    int32_t normal[3] = { i32(B, 0x364), i32(B, 0x368), i32(B, 0x36c) };
    if (B[0x34d] == 0) {
        B[5] = 1;
        w32(body, 0x40, i16(B, 0x354));
        w16(body, 0xa, i16(B, 0x352));
        int32_t amount = i16(B, 0x350);
        car_hit_turn(car, player, 1, i16(B, 0x358), -1, env);
        if (env->damage) env->damage(env->user, idx, player, amount, normal, i16(B, 0x360));
        w16(B, 8, i16(body, 0x34) + i16(B, 0x35c));
        norm16(B, 8);
        B[3] = 1;
        w16(B, 0xa, 0x1e - (int16_t)((int64_t)(i16(B, 0x350) * 0x1e) / 0xc));
    } else {
        int32_t amount = i16(B, 0x350);
        B[5] = 0;
        if (0 < amount && player && env->damage) env->damage(env->user, idx, player, amount, normal, -1);
        if (!player) {
            w16(car, 0x468, i16(car, 0x468) + 1);
            if (3 < i16(car, 0x468)) car[0x3b] = 1;
            if (car[0x3f]) w16(car, 0x46a, 0x1e);
            if (i16(B, 0x358) < 0x6ab) { if (i16(B, 0x358) < -0x6aa) w16(B, 0x358, -0x6aa); }
            else w16(B, 0x358, 0x6aa);
        }
        car_hit_turn(car, player, 0, i16(B, 0x358), (int8_t)B[0x34f], env);
    }
    int16_t ang[3] = { 0, 0, i16(B, 0x356) };
    int32_t m[9];
    tm2_build_matrix(ang, m);
    int32_t push = (i16(body, 0x1a) << 2) / i32(stats, 0xb4) + 2;
    int32_t v[3] = { 0, -(i16(B, 0x35a) + push), 0 }, out[3];
    tm2_mat_vec_mul_row(m, v, out);
    for (int i = 0; i < 3; i++) w32(body, 0x20 + 4 * i, tm2_wadd(i32(body, 0x20 + 4 * i), out[i]));
    int32_t off[3] = { 0, 0, 0 }, spark[3];
    const uint8_t* pr;
    switch (B[0x34f]) {
    case 0: pr = P + 0x18; off[0] = pr[7] ? -push : push; goto corner;
    case 1: pr = P + 0x60; off[0] = pr[7] ? push : -push; goto corner;
    case 2: pr = P + 0xf0; off[0] = pr[7] ? push : -push; goto corner;
    case 3: pr = P + 0xa8; off[0] = pr[7] ? -push : push;
    corner:
        spark[0] = i32(pr, 0x3c); spark[1] = i32(pr, 0x40); spark[2] = i32(pr, 0x44) + i16(stats, 0x42);
        break;
    case 4:
        spark[0] = (i32(P, 0x54) + i32(P, 0x9c)) / 2;
        spark[1] = (i32(P, 0x58) + i32(P, 0xa0)) / 2;
        spark[2] = (i32(P, 0x5c) + i32(P, 0xa4)) / 2 + i16(stats, 0x42);
        break;
    default:
        off[1] = push * -2;
        spark[0] = 0; spark[1] = 0;
        spark[2] = i32(body, 0x28) + i16(stats, 0x46);
        if (ctl[0xb]) {
            for (int k = 0; k < 4; k++) w16(P, 0x18 + 0x48 * k + 0x14, (int16_t)(-0x280 / (int64_t)tm2_frame_ticks60()));
            w16(body, 0xa, i16(body, 0xa) / 2);
        }
    }
    int32_t sp = iabs(i16(body, 0xa)), lim = i32(stats, 0xb4);
    if (lim <= sp) lim = sp;
    w16(B, 0xe, (sp * 99) / lim);
    int32_t bm[9];
    for (int i = 0; i < 9; i++) bm[i] = i32(body, 0x50 + 4 * i);
    tm2_mat_vec_mul_row(bm, off, out);
    for (int i = 0; i < 3; i++) w32(body, 0x20 + 4 * i, tm2_wadd(i32(body, 0x20 + 4 * i), out[i]));
    if ((tm2_frame_ticks60() * 0x17c / 100) * 0x20 < i16(body, 0x1a) && env->sparks) env->sparks(env->user, spark);
}

void car_collision_points(const uint8_t* car, int player, uint8_t* e) {
    const uint8_t* s = car + (player ? 0x138 : 0x4ec);
    int32_t w = i16(s, 0x48), front = i16(s, 0x4e), rear = i16(s, 0x50), z = 0x18 - i16(s, 0x42);
    w32(e, 0xf4, 6);
    const int32_t pts[6][3] = { { -w / 2, front, z }, { w / 2, front, z }, { w / 2, rear, z }, { -w / 2, rear, z },
                                { 0, front, z }, { 0, front, i16(s, 0x46) } };
    for (int k = 0; k < 6; k++)
        for (int i = 0; i < 3; i++) w16(e, 0xf8 + 8 * k + 2 * i, pts[k][i]);
}

void car_hit_step(uint8_t* car, int player, const CarHitApplyEnv* env) {
    uint8_t* B = car + (player ? 0x21c : 0xcc);
    uint8_t* body = car + (player ? 0x6c4 : 0x708);
    if (B[2] == 0) {
        if (2 < i16(B, 0x10)) {
            for (int i = 0; i < 3; i++) w16(body, 0x18 + 2 * i, i16(body, 8 + 2 * i));
            for (int i = 0; i < 3; i++) w32(body, 0xe0 + 4 * i, i32(body, 0x20 + 4 * i));
            for (int i = 0; i < 3; i++) w32(body, 0xec + 4 * i, i32(body, 0x38 + 4 * i));
            for (int i = 0; i < 12; i++) w32(body, 0xf8 + 4 * i, i32(body, 0x50 + 4 * i));
        }
    } else car_hit_push(car, player, env);
}

static int32_t wsub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

static void hit_end(uint8_t* B) {
    B[3] = 0;
    B[4] = 0;
    w16(B, 6, 0);
    B[2] = 0;
}

void car_hit_recover(uint8_t* car, int player, const CarRecoverEnv* env) {
    uint8_t* body = car + (player ? 0x6c4 : 0x708);
    uint8_t* ctl = car + (player ? 0x9a : 0x68);
    uint8_t* stats = car + (player ? 0x138 : 0x4ec);
    uint8_t* B = car + (player ? 0x21c : 0xcc);
    uint8_t* P = car + (player ? 0x58c : 0x5d0);
    int32_t idx = i16(car, player ? 2 : 6);
    if (i16(B, 6) != 0) {
        int32_t step = (tm2_frame_ticks60() * (i16(B, 0x12) * 7)) / 100;
        if (step == 0 && (i16(B, 0x12) != 0 || (B[5] && i16(B, 0xa) < 10))) step = 2;
        if (step != 0) {
            const int32_t* v = B[5] == 0 ? NULL : env->pair_push + i16(B, 0x14) * 45 + idx * 3;
            int32_t vx = v ? v[0] : i32(B, 0x364), vy = v ? v[1] : i32(B, 0x368);
            int32_t dx = tm2_wmul(vx, step) / 0xfff, dy = tm2_wmul(vy, step) / 0xfff;
            int32_t tx = tm2_wadd(dx, i32(P, 0xc)), ty = tm2_wadd(dy, i32(P, 0x10));
            int32_t lim = (tm2_frame_ticks60() * 0xa64) / 100;
            if (lim < tx) { dx = wsub(lim, i32(P, 0xc)); step = lim; }
            else if (tx < -lim) { dx = wsub(-lim, i32(P, 0xc)); step = lim; }
            if (lim < ty) { dy = wsub(lim, i32(P, 0x10)); step = lim; }
            else if (ty < -lim) { dy = wsub(-lim, i32(P, 0x10)); step = lim; }
            w32(body, 0x20, tm2_wadd(i32(body, 0x20), dx));
            w32(body, 0x24, tm2_wadd(i32(body, 0x24), dy));
            w16(B, 0x12, i16(B, 0x12) - step);
            if ((tm2_frame_ticks60() * 0x5f * 8) / 100 < step) {
                ctl[1] = 1;
                w32(body, 0x4c, 0);
                if (ty == 0) ty = 1;
                w32(body, 0x40, tm2_atan2(tx, ty));
            } else if (ctl[1] == 0) {
                w16(B, 0x12, 0);
            }
            int32_t pos[3] = { i32(body, 0x20), i32(body, 0x24), tm2_wadd(i32(body, 0x28), step / 2) };
            int32_t up = wsub(env->ground_height ? env->ground_height(env->user, pos) : 0, i32(body, 0x28));
            if (0 < up) {
                w32(body, 0x28, tm2_wadd(i32(body, 0x28), up));
                for (int k = 0; k < 4; k++) w32(P, 0x5c + 0x48 * k, tm2_wadd(i32(P, 0x5c + 0x48 * k), up));
            }
        }
        if (player && ctl[6]) {
            if (i16(B, 6) < 0x78) hit_end(B);
            else w16(body, 0x14, 0);
        }
        if (B[4]) {
            w32(body, 0x34, tm2_angle_interp(i32(body, 0x34), i16(B, 8), 9));
        } else if (B[3]) {
            int32_t d = wsub(i32(body, 0x40), i32(body, 0x34));
            tm2_angle_wrap32(&d);
            int32_t a = iabs(d), rate;
            if (B[5] == 0) rate = a < 0x155 ? 9 : a < 0x238 ? 5 : 0x38d < a ? 1 : 3;
            else rate = a < 0x4e4 ? 5 : a < 0x672 ? 3 : 1;
            w32(body, 0x34, tm2_angle_interp(i32(body, 0x34), i16(B, 8), rate));
        }
        w16(B, 6, i16(B, 6) - tm2_frame_ticks60());
        if (i16(B, 6) < 1) hit_end(B);
        else if (B[5] == 0 && i16(B, 0x12) == 0 &&
                 (iabs(i16(body, 0xa)) < i32(stats, 0xb0) || iabs(wsub(i16(B, 8), i32(body, 0x34))) < 0x38))
            hit_end(B);
    }
    if (i16(B, 0xa) < 0x7d01) w16(B, 0xa, i16(B, 0xa) + tm2_frame_ticks60());
}
