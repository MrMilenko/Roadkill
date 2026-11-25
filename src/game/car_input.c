/*
 * car_input.c: player car input, ported from TM2.EXE 0x00417734 (see car_input.h).
 */
#include "car_input.h"

#include "../math/tm2math.h"

static int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static int32_t i32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static void w16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }
static void w32(uint8_t* b, int o, int32_t v) {
    uint32_t u = (uint32_t)v;
    b[o] = (uint8_t)u; b[o + 1] = (uint8_t)(u >> 8); b[o + 2] = (uint8_t)(u >> 16); b[o + 3] = (uint8_t)(u >> 24);
}
static int32_t iabs(int32_t v) { return (int32_t)(v < 0 ? 0u - (uint32_t)v : (uint32_t)v); }

static uint8_t in(CarInputEnv* e, int id) { return e->input ? e->input(e->user, id, 0) : 0; }
static void ev(CarInputEnv* e, int id, uint8_t* car, int32_t a, int32_t b, int32_t c) {
    if (e->event) e->event(e->user, id, car, a, b, c);
}

/* 0x0044f100 */
void car_combo_reset(uint8_t* w) {
    w32(w, 0x2c, 0);
    w[0x54] = 0;
    w32(w, 0x30, -1);
}

void car_combo_update(uint8_t* w, CarInputEnv* e) {
    int32_t t = tm2_frame_ticks60();
    int32_t slot = i32(w, 0);
    for (int k = 0; k < 4; k++) w[0x24 + k] = e->input ? e->input(e->user, 0x51 + k, slot) : 0;
    for (int k = 0; k < 4; k++) {
        int hold = 4 + 4 * k, rel = 0x14 + 4 * k;
        if (!w[0x24 + k]) {
            w32(w, hold, 0);
            w32(w, rel, i32(w, rel) + t);
            w[0x28 + k] = 0;
            continue;
        }
        w32(w, rel, 0);
        int32_t h = i32(w, hold) + t;
        w32(w, hold, h);
        if (h < 0x14) {
            if (!w[0x28 + k]) {
                w[0x28 + k] = 1;
                int32_t n = i32(w, 0x2c);
                if (n < 8) {
                    w32(w, 0x2c, n + 1);
                    w32(w, 0x30 + 4 * n, k);
                    w32(w, 0x30 + 4 * (n + 1), -1);
                }
                w[0x54] = 1;
            }
        } else {
            if (w[0x28 + k]) car_combo_reset(w);
            w[0x28 + k] = 0;
        }
    }
    if (w[0x54]) {
        int idle = 1;
        for (int k = 0; k < 4; k++) if (i32(w, 0x14 + 4 * k) < 0x15) { idle = 0; break; }
        if (idle) car_combo_reset(w);
    }
}

/* 0x0045b5a8 */
void car_watch_player(CarInputEnv* e, int32_t v) {
    if (e->watch_ai_flag) {
        e->watch_ai_flag = 0;
        ev(e, CAR_IN_SET_STATE, 0, 2, 0, 0);
    }
    if (v >= e->player_count) v = 0;
    else if (v < 0) v = e->player_count - 1;
    e->watch_player = v;
}

/* 0x0045b564 */
void car_watch_ai(CarInputEnv* e, int32_t v) {
    e->watch_ai_flag = 1;
    ev(e, CAR_IN_SET_STATE, 0, 7, 0, 0);
    if (e->watch_ai >= e->ai_count) v = 0;
    else if (e->watch_ai < 0) v = e->ai_count - 1;
    e->watch_ai = v;
}

/* 0x00429fd0 (old name vehicle_clear_input), on the control block at car +0xCC */
static void clear_controls(uint8_t* p) {
    static const uint8_t zero[] = { 0xe, 0x10, 0xf, 0x13, 0x12, 0, 1, 3, 0x17, 0x18, 0x1b, 0x1c, 0x1d, 0x1f, 4, 5, 0xd };
    for (unsigned i = 0; i < sizeof zero; i++) p[zero[i]] = 0;
    p[0x1a] = 1;
}

static void combo_code(uint8_t* car, uint8_t code) {
    car[0x854] = code;
    car[0xe9] = 1;
    car[0xee] = 0;
    w16(car, 0xf0, 0);
    w16(car, 0xf2, 0);
}

static int seq_is(const uint8_t* w, const int32_t* s, int n) {
    for (int i = 0; i < n; i++) if (i32(w, 0x30 + 4 * i) != s[i]) return 0;
    return 1;
}

static void read_controls(uint8_t* car, CarInputEnv* e) {
    car[0xd0] = in(e, 0x11);
    car[0xd1] = in(e, 0x12);
    car[0xd3] = in(e, 0x3e);
    car[0xd5] = in(e, 0x49);
    car[0xd6] = in(e, 0x4a);
    car[0xd7] = in(e, 0x4b);
    car[0xe2] = in(e, 0x4c);
    car[0xd4] = 0;
    if (in(e, 0x3c)) car[0xd3] = 1;
    else if (in(e, 0x3d)) car[0xd3] = 2;
    if (e->watch_player == 0) {
        if (in(e, 0x40)) car[0xd4] = 2;
        if (in(e, 0x41)) car[0xd4] = 3;
        if (in(e, 0x43)) car[0xd4] = 4;
    }

    /* spectator camera */
    if (!e->spectate) {
        if (car[0xd8]) car[0xd8] = 0;
        if (e->watch_player != 0) {
            car_watch_player(e, 0);
            car[0xd8] = 1;
            ev(e, CAR_IN_SET_STATE, car, car[0x107], 0, 0);
        }
    } else if (!car[0xd8]) {
        if (in(e, 0x47)) {
            if (e->mode < 5) car_watch_ai(e, e->watch_ai + 1);
            else car_watch_player(e, e->watch_player + 1);
            car[0xd8] = 1;
        } else if (in(e, 0x48)) {
            if (e->mode < 5) car_watch_ai(e, e->watch_ai - 1);
            else car_watch_player(e, e->watch_player - 1);
            car[0xd8] = 1;
        } else if (in(e, 0x46)) {
            car_watch_player(e, 0);
            car[0xd8] = 1;
        } else {
            car[0xd8] = 0;
        }
        if (car[0xd8] && !(uint8_t)e->watch_ai_flag) {
            if (e->watch_player == 0) {
                ev(e, CAR_IN_SET_STATE, car, car[0x107], 0, 0);
            } else {
                uint8_t keep = car[0x107];
                ev(e, CAR_IN_SET_STATE, car, 2, 0, 0);
                car[0x107] = keep;
            }
        }
    } else if (!in(e, 0x47) && !in(e, 0x48) && !in(e, 0x46)) {
        car[0xd8] = 0;
    }

    car[0xcf] = in(e, 0x29) && !car[0xa7] && !car[0xb7];
    car[0xdd] = in(e, 0x24);
    car[0xda] = in(e, 0x27);
    car[0xdf] = in(e, 0x25) || (car[0xdd] && car[0xda]) || (car[0xda] && i16(car, 0x6ce) >= 1);
    car[0xe0] = car[0xdd] || in(e, 0x26) || car[0xda];
    car[0xde] = car[0xe0] && !car[0xa7] && !car[0xac] && !car[0xa6] && !car[0xb7];
    car[0xdc] = in(e, 0x22);
    car[0xdb] = in(e, 0x23);
    car[0xcc] = in(e, 0x28) && car[0xdc];
    car[0xcd] = in(e, 0x28) && car[0xdb];
    car[0xce] = car[0xcc] || car[0xcd];
    car[0xa0] = car[0xdc] || car[0xdb];
    car[0xe5] = 0;
    car[0xe3] = in(e, 0x2d);
    car[0xe4] = in(e, 0x2c);
    for (int i = 7; i >= 0; i--)
        if (in(e, 0x2e + i)) car[0xe5] = (uint8_t)(100 + i);
    car[0xe7] = in(e, 0x2b);
    car[0xe8] = in(e, 0x2a);

    /* combos */
    uint8_t* w = car + 0x7f0;
    car_combo_update(w, e);
    int32_t n = i32(w, 0x2c);
    static const int32_t S100[] = { 2, 3, 0 }, S_SPEC[] = { 2, 3, 1 }, S65[] = { 3, 2, 1 }, S66[] = { 3, 2, 0 },
                         S67[] = { 0, 0, 3 }, S68[] = { 0, 0, 2 }, S69[] = { 3, 1, 2, 0 }, S6A[] = { 0, 1, 0, 0 },
                         SHEAL[] = { 1, 0, 3, 2, 0, 0, 1, 1 };
    int reset = 0;
    if (n == 3) {
        if (seq_is(w, S100, 3)) { combo_code(car, 100); reset = 1; }
        else if (seq_is(w, S_SPEC, 3)) {
            car[0xe7] = 1;
            car[0xee] = 0;
            w16(car, 0xf0, 0);
            w16(car, 0xf2, 0);
            car[0x84d] = 1;
            if (car[0x855] != 7 && car[0x855] != 3) ev(e, CAR_IN_SOUND, car, 0x38, 0, 0);
            reset = 1;
        }
        else if (seq_is(w, S65, 3)) { combo_code(car, 0x65); reset = 1; }
        else if (seq_is(w, S66, 3)) { combo_code(car, 0x66); reset = 1; }
        else if (seq_is(w, S67, 3)) { combo_code(car, 0x67); reset = 1; }
        else if (seq_is(w, S68, 3)) { combo_code(car, 0x68); reset = 1; }
    } else if (n == 4) {
        if (seq_is(w, S69, 4)) { combo_code(car, 0x69); reset = 1; }
        else if (seq_is(w, S6A, 4) && car[0xe8]) { combo_code(car, 0x6a); reset = 1; }
    } else if (n == 8 && seq_is(w, SHEAL, 8)) {
        int32_t k = (i16(car, 0x856) + i16(car, 0x86a)) * 3;
        if (k > 0x1e) k = 0x1e;
        int32_t hp = i32(car, 0x1d8) + k, cap = i16(car, 0x172);
        w32(car, 0x1d8, hp < cap ? hp : cap);
        for (int i = 0; i < 8; i++) w16(car, 0x856 + 2 * i, 0);
        car[0xee] = 0;
        w16(car, 0xf0, 0);
        w16(car, 0xf2, 0);
        w16(car, 0x86a, 0);
        if (k < 1) { ev(e, CAR_IN_SOUND, car, 0x3c, 0, 0); ev(e, CAR_IN_SCREEN, car, 0x3f, 99, 0); }
        else { ev(e, CAR_IN_SOUND, car, 0x32, 0, 0); ev(e, CAR_IN_SCREEN, car, 0x18, 99, 0); }
        reset = 1;
    }
    if (reset) car_combo_reset(w);
    car[0xeb] = in(e, 0x2b);
}

void car_read_input(uint8_t* car, CarInputEnv* e) {
    if (i32(car, 0x1d8) < 1 || car[0xb9]) {
        clear_controls(car + 0xcc);
    } else {
        if (i16(car, 0) == 0) read_controls(car, e);
        car[0xd2] = i32(car, 0x1e8) < iabs(i16(car, 0x6ce)) && car[0xdf];
        if (car[0xd2] && (car[0xdc] || car[0xdb])) { car[0xdd] = 0; car[0xdf] = 0; }
        if (!car[0xb3]) {
            int32_t v = i16(car, 0x226);
            int set = 0;
            if (v >= 0xb5) set = 1;
            else if (v >= 0x1f) set = car[0xa0] || car[0xde] || car[0xb6];
            else if (v >= 0x10) set = car[0xde] || car[0xb6];
            else if (v >= 8) set = car[0xb6] != 0;
            if (set) car[0xb3] = 1;
        }
    }
    /* camera tracking toggle */
    int32_t a = i16(car, 0x90) + 6;
    if (!car[0xcf] || i16(car, 0x87a) < 1) {
        if (!car[0xb6]) return;
        car[0xb6] = 0;
        ev(e, CAR_IN_CAMERA, car, a, 0, 0);
    } else {
        if (car[0xb6]) return;
        car[0xb6] = 1;
        ev(e, CAR_IN_CAMERA, car, a, 0, 1);
    }
}
