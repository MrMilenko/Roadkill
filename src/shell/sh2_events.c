/*
 * sh2_events.c: match event effects and the car state snapshot (see sh2_events.h).
 */
#include "sh2_events.h"
#include "../core/tm2rand.h"

static uint32_t callv(const Sh2Env* e, uint32_t fn, int n, const uint32_t* a) {
    return e->sh.call ? e->sh.call(e->sh.user, fn, n, a) : 0;
}
static uint32_t call(const Sh2Env* e, uint32_t fn, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t a[4] = {a0, a1, a2, a3};
    return callv(e, fn, n, a);
}
#define C0(fn) call(e, fn, 0, 0, 0, 0, 0)
#define C1(fn, a) call(e, fn, 1, a, 0, 0, 0)
#define C2(fn, a, b) call(e, fn, 2, a, b, 0, 0)
#define C3(fn, a, b, c) call(e, fn, 3, a, b, c, 0)
#define C4(fn, a, b, c, d) call(e, fn, 4, a, b, c, d)
#define R8(a) sh_r8(&e->sh, a)
#define R16(a) sh_r16(&e->sh, a)
#define R32(a) sh_r32(&e->sh, a)
#define W8(a, v) sh_w8(&e->sh, a, v)
#define W16(a, v) sh_w16(&e->sh, a, v)
#define W32(a, v) sh_w32(&e->sh, a, v)
#define I16(a) ((int32_t)(int16_t)R16(a))

static void big_explosion(const Sh2Env* e, uint32_t pos) {
    C2(0x00450aa0u, pos, 0);            /* fx_explosion48 */
    C2(0x004510e0u, pos, 0);            /* fx_smoke_a8 */
}
static void flash(const Sh2Env* e) {
    C4(0x00415bc4u, 5, 0xff, 200, 200); /* screen_flash_start */
}
static void debris(const Sh2Env* e, uint32_t node, uint32_t pos, uint32_t dir, uint32_t spin, uint32_t hold, uint32_t scale) {
    uint32_t a[9] = {node, pos, dir, spin, 0, 0, hold, scale, 0};
    callv(e, 0x0048a738u, 9, a);        /* debrisBurstAt, 5 stack words */
}
/* sound 4 with the volume / 3 (unsigned) and the priority 0x63 */
static void end_sound(const Sh2Env* e) {
    uint32_t b = C0(0x0046f990u);
    uint32_t v = C0(0x0046f988u) / 3u;
    C4(0x0046f8b0u, 4, v, b, 0x63);
}
static void music_cue(const Sh2Env* e, uint32_t pos) {
    if (C0(0x0046cc54u) == 0) C1(0x0046f974u, pos);
    else C2(0x0046f980u, pos, 0xffffffffu);
}
/* rate - 1 with a floor: the old value below floor gives floor */
static int32_t step_rate(const Sh2Env* e, uint32_t a, int32_t floor) {
    int32_t old = I16(a);
    W16(a, (uint32_t)(old - 1));
    if (old < floor) W16(a, (uint32_t)floor);
    return I16(a);
}

void sh2_event_destroyed_fx(const Sh2Env* e) {
    const uint32_t ev = 0x00622ab0u, pos = ev + 0x10;
    if (!R8(ev) || R8(ev + 1)) return;
    W16(ev + 4, R16(ev + 4) + 1u);
    if (I16(ev + 4) == 1) big_explosion(e, pos);
    int32_t rate = step_rate(e, ev + 6, 2);
    if (tm2_rand() % rate == 0) {
        uint32_t p = C4(0x0044a40cu, pos, 0xc, 0xc, 0x29);
        if (tm2_rand() & 1) {
            C2(0x00450a10u, p, 0);
            C2(0x0045109cu, p, 0);
        } else {
            big_explosion(e, p);
        }
    }
    if (R16(ev + 8) == R16(ev + 4)) {
        flash(e);
        big_explosion(e, pos);
        debris(e, R32(ev + 0xc), pos, ev + 0x1c, ev + 0x24, 0x28, 0x78);
        C3(0x00470320u, 0x1fc, (uint32_t)I16(ev + 2), 1);
    }
    if (I16(ev + 4) == I16(ev + 8) + 0x3c) {
        music_cue(e, pos);
        end_sound(e);
        W8(ev + 1, 1);
    }
}

void sh2_event_special_fx(const Sh2Env* e) {
    const uint32_t ev = 0x00622adau, pos = ev + 0xe;   /* +2 active, +3 phase, +4 id, +6 id 2, +8 counter, +0xa rate, +0xc trigger */
    uint8_t own[8];
    uint8_t* dir = sh_local_host(&e->sh, sh_local(&e->sh, 0), own, sizeof own);
    Sh2Env* me = (Sh2Env*)e;
    for (int i = 0; i < 8; i++) dir[i] = R8(0x004e7ff4u + (uint32_t)i);
    if (!R8(ev + 2) || (int8_t)R8(ev + 3) >= 2) return;
    W16(ev + 8, R16(ev + 8) + 1u);
    if (I16(ev + 8) == 1) big_explosion(e, pos);
    int32_t rate = step_rate(e, ev + 0xa, 2);
    if (tm2_rand() % rate == 0) {
        uint32_t p = C4(0x0044a40cu, pos, 0x14, 0x14, 10);
        if (tm2_rand() % 2 == 0) {
            C2(0x00450a10u, p, 0);
            C2(0x0045109cu, p, 0);
        } else {
            big_explosion(e, p);
        }
    }
    if (R16(ev + 0xc) != R16(ev + 8)) return;
    flash(e);
    big_explosion(e, pos);
    if ((int8_t)R8(ev + 3) == 0) {
        int32_t id = I16(ev + 4);
        uint32_t obj = C1(0x00470b14u, (uint32_t)id);
        if (R8(obj + 6) > 2) {
            W8(obj + 7, 2);
            me->local[0] = dir;
            debris(e, obj, 0x006226c8u + (uint32_t)id * 20u + 8u, sh_local(&e->sh, 0), 0, 0x3c, 0xb4);
            me->local[0] = 0;
        }
        W8(obj + 7, 1);
        C3(0x00470320u, 0x207, (uint32_t)I16(ev + 4), 1);
        W16(ev + 8, 0);
        W16(ev + 0xa, 0x14);
        W32(ev + 0x16, 0x15e0);
        W8(ev + 2, 0);
        W16(ev + 0xc, 0x50);
        W32(ev + 0x12, 0x4d50);
        W32(ev + 0xe, 0x3ff8);
        W8(ev + 3, 1);
    } else {
        C3(0x00470320u, 0x1fd, (uint32_t)I16(ev + 6), 1);
        W8(ev + 3, 2);
    }
    music_cue(e, pos);
    end_sound(e);
}

static void target_end_flag(const Sh2Env* e, int k) {
    if (k == 0) W8(0x004fdc79u, 0);
    else W8(0x004fdc77u, 0);
}

void sh2_target_player_events(const Sh2Env* e) {
    if (C0(0x0045c338u) == 2 || C0(0x0045c338u) == 4) {
        for (int k = 0; k < 2; k++) {
            uint32_t r = 0x00622af4u + 0x2cu * (uint32_t)k, pos = r + 0x10;
            if (!R8(r) || R8(r + 1)) continue;
            flash(e);
            C2(0x00450aa0u, pos, 0);
            C2(0x004510e0u, pos, 0);
            C3(0x00470320u, 0x1fe, (uint32_t)I16(r + 2), 1);
            C2(0x0046f980u, pos, 0xffffffffu);
            end_sound(e);
            W8(r, 0);
            W8(r + 1, 1);
            target_end_flag(e, k);
        }
        return;
    }
    for (int k = 0; k < 2; k++) {
        uint32_t r = 0x00622af4u + 0x2cu * (uint32_t)k, pos = r + 0x10;
        if (!R8(r) || R8(r + 1)) continue;
        W16(r + 4, R16(r + 4) + 1u);
        if (I16(r + 4) == 1) big_explosion(e, pos);
        int32_t rate = step_rate(e, r + 6, 5);
        if (tm2_rand() % rate == 0) {
            uint32_t p = C4(0x0044a40cu, pos, 0x52, 0x52, 0x30);
            if (tm2_rand() & 1) {
                C2(0x00450984u, p, 0);
                C2(0x0045109cu, p, 0);
            } else {
                C2(0x00450a10u, p, 0);
                C2(0x004510e0u, p, 0);
            }
        }
        if (R16(r + 8) == R16(r + 4)) {
            flash(e);
            big_explosion(e, pos);
            debris(e, R32(r + 0xc), pos, r + 0x1c, r + 0x24, 0x28, 0x78);
            C3(0x00470320u, 0x1fe, (uint32_t)I16(r + 2), 1);
        }
        if (I16(r + 4) == I16(r + 8) + 0x1e) {
            music_cue(e, pos);
            end_sound(e);
            W8(r, 0);
            W8(r + 1, 1);
            target_end_flag(e, k);
        }
    }
}

void sh2_car_net_state(const Sh2Env* e, uint32_t car, uint8_t on) {
    const uint32_t s = 0x00622d24u;
    if (!on) return;
    W16(s + 0, R16(car + 0x6f8));
    W16(s + 2, R16(car + 0x1d8));
    W32(s + 8, R32(car + 0x6e4));
    W32(s + 0xc, R32(car + 0x6e8));
    W32(s + 0x10, R32(car + 0x6ec));
    /* 32 bit division, then 16 bit ones (idiv cx): quotients kept to 16 bits */
    int16_t q = (int16_t)((I16(car + 0x6ce) * 100) / (int32_t)R32(car + 0x1ec));
    W16(s + 4, (uint32_t)q);
    int16_t fps = (int16_t)C0(0x004931d4u);
    W16(s + 4, (uint32_t)(int16_t)((int32_t)(int16_t)R16(s + 4) / fps));
    q = (int16_t)((I16(car + 0x6c4) * 100) / I16(car + 0x19c));
    W16(s + 6, (uint32_t)q);
    fps = (int16_t)C0(0x004931d4u);
    W16(s + 6, (uint32_t)(int16_t)((int32_t)(int16_t)R16(s + 6) / fps));
    static const uint16_t src[14] = {0xcc, 0xcd, 0xce, 0xcf, 0xd2, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0x154, 0xb4};
    uint32_t w = R16(s + 0x14) & 0xc000u;
    for (int i = 0; i < 14; i++) w |= (uint32_t)(R8(car + src[i]) & 1) << i;
    W16(s + 0x14, w);
}

/* node field store through actor +0x48, read again each time */
static void node_w32(const Sh2Env* e, uint32_t actor, uint32_t off, uint32_t v) {
    W32(R32(actor + 0x48) + off, v);
}
static void node_w16(const Sh2Env* e, uint32_t actor, uint32_t off, uint32_t v) {
    W16(R32(actor + 0x48) + off, v);
}
static void zero_rot(const Sh2Env* e, uint32_t actor) {
    node_w16(e, actor, 0xe0, 0);
    node_w16(e, actor, 0xe2, 0);
    node_w16(e, actor, 0xe4, 0);
}
/* row k of n rows at tab (stride bytes); x / y + rand() / 0x199; with own: the actor's position
 * from the row's second half. Returns k. */
static uint32_t place_jitter(const Sh2Env* e, uint32_t actor, uint32_t tab, int32_t n, uint32_t stride, int own) {
    uint32_t k = (uint32_t)(tm2_rand() % n);
    uint32_t row = tab + k * stride;
    int32_t r = tm2_rand();
    node_w32(e, actor, 0xe8, (uint32_t)(r / 0x199) + R32(row));
    r = tm2_rand();
    node_w32(e, actor, 0xec, (uint32_t)(r / 0x199) + R32(row + 4));
    node_w32(e, actor, 0xf0, R32(row + 8));
    zero_rot(e, actor);
    if (own) {
        W32(actor + 0xc, R32(row + 0xc));
        W32(actor + 0x10, R32(row + 0x10));
        W32(actor + 0x14, R32(row + 0x14));
    }
    return k;
}
static uint32_t place_fixed(const Sh2Env* e, uint32_t actor, uint32_t row, int own) {
    node_w32(e, actor, 0xe8, R32(row));
    node_w32(e, actor, 0xec, R32(row + 4));
    node_w32(e, actor, 0xf0, R32(row + 8));
    zero_rot(e, actor);
    if (own) {
        W32(actor + 0xc, R32(row + 0xc));
        W32(actor + 0x10, R32(row + 0x10));
        W32(actor + 0x14, R32(row + 0x14));
    }
    return 0;
}

void sh2_target_actor_place(const Sh2Env* e, uint32_t actor) {
    uint8_t et = (uint8_t)(C0(0x00474508u) - 1);
    int32_t type = I16(actor + 4);
    int big = type == 0x202 || type == 0x205;
    uint32_t k = 0;
    switch (et + 1) {
    case 1:
        if (big) {
            k = (uint32_t)(tm2_rand() % 3);
            place_fixed(e, actor, 0x004e7d58u + k * 24u, 1);
        } else {
            k = place_jitter(e, actor, 0x004e7da0u, 5, 12, 0);
        }
        break;
    case 3:
        k = big ? place_jitter(e, actor, 0x004e7cc8u, 4, 24, 1) : place_jitter(e, actor, 0x004e7d28u, 4, 12, 0);
        break;
    case 5:
        if (I16(actor + 6) == 1) place_fixed(e, actor, 0x004e7e48u, 0);
        else k = place_jitter(e, actor, 0x004e7e60u, 5, 12, 0);
        break;
    case 6:
        if (big) {
            k = (uint32_t)tm2_rand() & 1;
            place_fixed(e, actor, 0x004e7ddcu + k * 24u, 1);
        } else {
            k = place_jitter(e, actor, 0x004e7e0cu, 4, 12, 0);
        }
        break;
    case 8:
        k = big ? place_jitter(e, actor, 0x004e7e9cu, 5, 24, 1) : place_jitter(e, actor, 0x004e7f14u, 10, 12, 0);
        break;
    case 11:
        k = place_jitter(e, actor, 0x004e7f8cu, 8, 12, 0);
        break;
    case 2:
    case 4:
        break;
    default:
        return;
    }
    W8(actor + 1, 1);
    W16(actor + 8, 0);
    W8(actor + 2, 0);
    W8(actor, k);
}
