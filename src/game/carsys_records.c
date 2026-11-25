/*
 * carsys_records.c: car record accessors, ported from TM2.EXE 0x0045a8bc..0x0045b3dc.
 * See carsys_records.h for the data they reach.
 */
#include "carsys_records.h"

#include <string.h>

#include "../math/tm2math.h"
#include "car.h"

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }

static uint8_t* human_at(const CarRecords* r, int32_t i) { return r->human + (intptr_t)i * CARSYS_HUMAN_SIZE; }
static uint8_t* ai_at(const CarRecords* r, int32_t i) { return r->ai + (intptr_t)i * CARSYS_AI_SIZE; }

/* |a - b| with the original's 32 bit wrap (cdq / xor / sub) */
static int32_t wabs_diff(int32_t a, int32_t b) {
    uint32_t d = (uint32_t)a - (uint32_t)b;
    return (int32_t)((int32_t)d < 0 ? 0u - d : d);
}

uint8_t carsys_boss_defeated(const CarRecords* r) { return *r->boss_defeated; }

int32_t carsys_human_nearest(const CarRecords* r, const int32_t* pos, int32_t exclude) {
    int32_t best = 0x7FFF, found = 0;
    for (int32_t i = 0; i < *r->human_count; i++) {
        const uint8_t* c = human_at(r, i);
        if (rd32(c + 0x1D8) <= 0) continue;
        int32_t d = (int32_t)((uint32_t)wabs_diff(pos[0], rd32(c + 0x6E4)) + (uint32_t)wabs_diff(pos[1], rd32(c + 0x6E8)));
        if (d < best && i != exclude) {
            best = d;
            found = i;
        }
    }
    return found;
}

int32_t carsys_ai_nearest_alive(const CarRecords* r, const int32_t* pos) {
    int32_t best = 0x7FFF, found = 0;
    for (int32_t i = 0; i < *r->ai_count; i++) {
        const uint8_t* c = ai_at(r, i);
        if (rd32(c + 0x58C) <= 0) continue;
        int32_t d = (int32_t)((uint32_t)wabs_diff(pos[0], rd32(c + 0x728)) + (uint32_t)wabs_diff(pos[1], rd32(c + 0x72C)));
        if (d < best) {
            best = d;
            found = i;
        }
    }
    return found;
}

void carsys_human_pos(const CarRecords* r, int32_t i, int32_t* out) {
    const uint8_t* c = human_at(r, i);
    for (int k = 0; k < 3; k++) out[k] = rd32(c + 0x6E4 + 4 * k);
}

void carsys_ai_pos(const CarRecords* r, int32_t i, int32_t* out) {
    const uint8_t* c = ai_at(r, i);
    for (int k = 0; k < 3; k++) out[k] = rd32(c + 0x728 + 4 * k);
}

void carsys_human_angles(const CarRecords* r, int32_t i, int16_t* out) {
    const uint8_t* c = human_at(r, i);
    for (int k = 0; k < 3; k++) out[k] = rd16(c + 0x6F0 + 4 * k);
}

uint8_t* carsys_human_object(const CarRecords* r, int32_t i) { return r->human_objects[i]; }

void carsys_hud_speed_reset(const CarRecords* r, int32_t i) { r->hud_speed[(int16_t)i] = 0; }

/* (speed >> 16) / 32 * 100 / (ticks60 * 19), C division as the original's sar / idiv */
static int32_t speedo(int32_t speed) {
    int32_t s = (speed >> 16) / 32 * 100;
    return s / ((int32_t)tm2_frame_ticks60() * 19);
}

void carsys_hud_speed_smooth(const CarRecords* r, int32_t i) {
    int32_t k = (int16_t)i;
    int32_t speed = speedo(rd32(human_at(r, k) + 0x6CC));
    int32_t gap = wabs_diff(speed, r->hud_speed[k]);
    int32_t rate = gap > 15 ? 13 : gap > 10 ? 9 : gap > 5 ? 5 : 3;
    r->hud_speed[k] = tm2_angle_step(r->hud_speed[k], speed, rate);
}

int32_t carsys_display_speed(const CarRecords* r, int32_t i) {
    const uint8_t* c = human_at(r, i);
    if (c[0x106] == 7) return speedo(rd32(ai_at(r, *r->watched_ai) + 0x710));
    return speedo(rd32(c + 0x6CC));
}

int32_t carsys_ai_display_speed(const CarRecords* r, int32_t i) { return speedo(rd32(ai_at(r, i) + 0x710)); }

int32_t carsys_object_count(const CarRecords* r) { return *r->slot_count; }
int32_t carsys_human_count(const CarRecords* r) { return *r->human_count; }
uint8_t* carsys_human_get(const CarRecords* r, int32_t i) { return human_at(r, i); }

uint8_t* carsys_ai_target(const CarRecords* r, int32_t i, uint8_t* human) {
    int32_t t = r->ai_targets[i];
    if (t == -1) {
        *human = 1;
        return human_at(r, carsys_human_nearest(r, (const int32_t*)(const void*)(ai_at(r, i) + 0x728), -1));
    }
    *human = 0;
    return ai_at(r, t);
}

uint8_t* carsys_from_slot(const CarRecords* r, int32_t i, uint8_t* human) {
    int32_t k = r->slot_index[i];
    if (k < 0) return NULL;
    if (r->slot_kind[i] == 0) {
        *human = 1;
        return human_at(r, k);
    }
    if (r->slot_kind[i] == 1) {
        *human = 0;
        return ai_at(r, k);
    }
    return NULL;
}

int32_t carsys_ai_count(const CarRecords* r) { return *r->ai_count; }

uint8_t* carsys_ai_get(const CarRecords* r, int32_t i) {
    if (i < 0 || i > 8) i = 0;
    return ai_at(r, i);
}

uint8_t* carsys_ai_watched(const CarRecords* r) { return ai_at(r, *r->watched_ai); }

uint8_t* carsys_object_from_owner(const CarRecords* r, int32_t owner) {
    int32_t i = 0;
    while (i < *r->slot_count && r->slot_owner[i] != (int16_t)owner) i++;
    return r->slot_objects[i];
}

uint8_t carsys_find_by_object(const CarRecords* r, const uint8_t* object, uint8_t* human, int32_t* index) {
    uint8_t found = 0;
    for (int32_t i = 0; i < *r->slot_count && !found; i++) {
        if (r->slot_objects[i] != object) continue;
        if (r->slot_kind[i] == 0) {
            *human = 1;
            *index = r->slot_index[i];
            found = 1;
        } else if (r->slot_kind[i] == 1) {
            *human = 0;
            *index = r->slot_index[i];
            found = 1;
        }
    }
    return found;
}

int32_t carsys_object_id(const CarRecords* r, int32_t i, uint8_t human) {
    int32_t slot = human ? r->human_slot[i] : r->ai_slot[i];
    return rd32(r->slot_objects[slot] + 0x158);
}

uint8_t carsys_find_by_id(const CarRecords* r, int32_t id, uint8_t* human, int32_t* index) {
    uint8_t found = 0;
    for (int32_t i = 0; i < *r->slot_count && !found; i++) {
        if (rd32(r->slot_objects[i] + 0x158) != id) continue;
        if (r->slot_kind[i] == 0) {
            found = 1;
            *human = 1;
        } else if (r->slot_kind[i] == 1) {
            *human = 0;
            found = 1;
        }
        *index = r->slot_index[i];
    }
    return found;
}

void carsys_stats_speed_percent(uint8_t* stats, int32_t percent) {
    wr16(stats + 0x76, (int16_t)percent);
    wr16(stats + 0x78, (int16_t)percent);
    car_stats_derive(stats);    /* 0x0043a93c falls through into 0x0043a944 */
}

void carsys_ai_set_difficulty(const CarRecords* r, uint32_t level) {
    *r->difficulty = (int32_t)level;
    int32_t pct = level == 0 ? 0x4B : level == 2 ? 0x8C : 0x78;
    for (int32_t i = 0; i < *r->ai_count; i++) carsys_stats_speed_percent(ai_at(r, i) + 0x4EC, pct);
}

int32_t carsys_ai_difficulty(const CarRecords* r) { return *r->difficulty; }
