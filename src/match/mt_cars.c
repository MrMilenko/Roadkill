/*
 * mt_cars.c: small car system accessors (see mt_cars.h).
 */
#include "mt_cars.h"

#include <string.h>

#include "../math/tm2math.h"

#define HS 0x898
#define AS 0x880

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static int32_t wmul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }

void mt_set_human_count(const MtCars* c, int32_t n) { *c->human_count = n; }
uint32_t mt_ai_object(const MtCars* c, int32_t i) { return c->ai_objects[i]; }

void mt_spawn_point_set(const MtCars* c, int32_t i, const int32_t* pos, const int16_t* ang) {
    if (!c->spawn_objects[i]) return;
    uint8_t* o = c->resolve(c->user, c->spawn_objects[i]);
    wr32(o + 0xe8, pos[0]);
    wr32(o + 0xec, pos[1]);
    wr32(o + 0xf0, pos[2]);
    memcpy(o + 0xe0, &ang[0], 2);
    memcpy(o + 0xe2, &ang[1], 2);
    memcpy(o + 0xe4, &ang[2], 2);
    int16_t a[3];
    int32_t m[9];
    memcpy(a, o + 0xe0, 6);
    tm2_build_matrix(a, m);
    memcpy(o + 0xb0, m, sizeof m);
}

uint8_t mt_flag_c2c4(const MtCars* c) { return *c->flag_c2c4; }
uint8_t mt_flag_2f7c(const MtCars* c) { return *c->flag_2f7c; }
uint8_t mt_flag_2f88(const MtCars* c) { return *c->flag_2f88; }
uint8_t mt_flag_2f94(const MtCars* c) { return *c->flag_2f94; }
uint32_t mt_car_value(const MtCars* c, int32_t i) { return c->car_value[(int16_t)i]; }
void mt_nop_45b3f8(void) {}
int32_t mt_watched_player(const MtCars* c) { return *c->watched_player; }
int32_t mt_watched_ai(const MtCars* c) { return *c->watched_ai; }
int32_t mt_state_2f78(const MtCars* c) { return *c->state_2f78; }

void mt_human_set_id(const MtCars* c, int32_t i, int32_t id) { wr32(c->human + wmul(i, HS) + 0x894, id); }

uint8_t* mt_human_by_id(const MtCars* c, int32_t id) {
    int32_t n = *c->human_count;
    if (n > 0) {
        int32_t lim = wmul(n, HS);
        int32_t off = 0;
        do {
            if (rd32(c->human + off + 0x894) == id) return c->human + off;
            off += HS;
        } while (off < lim);
    }
    return c->human;
}

int32_t mt_counter_d4(const MtCars* c, int32_t i) { return c->counter_d4[i]; }
int32_t mt_counter_b4(const MtCars* c, int32_t i) { return c->counter_b4[i]; }
void mt_counter_b4_inc(const MtCars* c, int32_t i) { c->counter_b4[i] = (int32_t)((uint32_t)c->counter_b4[i] + 1); }

static void set_by_id(const MtCars* c, int32_t* tbl, int32_t id, int32_t v) {
    int32_t n = *c->human_count;
    if (n <= 0) return;
    int32_t lim = wmul(n, 4);
    int32_t k = 0, off = 0;
    do {
        if (rd32(c->human + off + 0x894) == id) tbl[k / 4] = v;
        k += 4;
        off += HS;
    } while (k < lim);
}
void mt_counter_b4_set_by_id(const MtCars* c, int32_t id, int32_t v) { set_by_id(c, c->counter_b4, id, v); }
void mt_counter_d4_set_by_id(const MtCars* c, int32_t id, int32_t v) { set_by_id(c, c->counter_d4, id, v); }

int32_t mt_cars_alive(const MtCars* c) {
    int32_t alive = 0;
    if (c->is_multiplayer(c->user)) {
        int32_t n = *c->human_count;
        if (n > 1) {
            int32_t lim = wmul(n, HS), off = HS;
            do {
                if (rd32(c->human + off + 0x1d8) > 0) alive++;
                off += HS;
            } while (off < lim);
        }
    } else {
        int32_t n = *c->ai_count;
        if (n > 0) {
            int32_t lim = wmul(n, AS), off = 0;
            do {
                if (rd32(c->ai + off + 0x58c) > 0) alive++;
                off += AS;
            } while (off < lim);
        }
    }
    return alive;
}

int32_t mt_car_type_index(const MtCars* c, int32_t type) {
    int32_t i;
    for (i = 0; i < 14; i++)
        if ((int16_t)type == c->type_table[i]) return i;
    return i;
}

int16_t mt_pair_i16(const MtCars* c, int32_t a, int32_t b) { return c->pair_i16[wmul(a, 15) + b]; }
int32_t mt_pair_i32(const MtCars* c, int32_t a, int32_t b) { return c->pair_i32[wmul(a, 15) + b]; }

void mt_set_game_mode(const MtCars* c, int32_t mode) {
    *c->game_mode = mode;
    *c->singleplayer = mode == 0;
}
uint8_t mt_singleplayer(const MtCars* c) { return *c->singleplayer; }
