/*
 * carsys_world.c: car system helpers reaching world / session globals (see carsys_world.h).
 */
#include "carsys_world.h"

#include <string.h>

#include "../math/tm2math.h"

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static uint32_t rdu32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }

void carsys_fx_sprites_update(const CarsysWorld* w) {
    *w->fx_cooldown = (int32_t)((uint32_t)*w->fx_cooldown + 1u);
    w->particles_update(w->user);
    for (int i = 0; i < 150; i++) {
        uint8_t* r = w->fx_sprites + i * 0x30;
        if ((int8_t)r[0x0A] < 0) continue;
        r[0x0A]++;
        for (int k = 0; k < 3; k++)
            wr32(r + 0x14 + 4 * k, (int32_t)((uint32_t)rd32(r + 0x14 + 4 * k) + (uint32_t)rd32(r + 0x20 + 4 * k)));
        if ((int8_t)r[0x0A] < (int8_t)r[0x0B]) continue;
        int16_t loops = rd16(r + 0x0C);
        wr16(r + 0x0C, (int16_t)(loops - 1));
        r[0x0A] = loops > 0 ? 0 : 0xFF;
        if (rd16(r + 0x0C) == 1) {
            uint32_t t = rdu32(r + 0x10);
            if (t == w->smoke_tables[0] || t == w->smoke_tables[1] || t == w->smoke_tables[2]) r[0x0B] += 0x10;
        }
    }
}

int32_t carsys_car_is_selectable(const CarsysWorld* w, uint8_t car) {
    if (car == 12 && w->unlock[0] == 0) return 0;
    if (car == 13 && w->unlock[1] == 0) return 0;
    for (uint8_t i = 0; i < w->session[0]; i++)
        if ((int32_t)(int8_t)w->session[4 + i] == (int32_t)car) return 0;
    for (uint8_t i = 0; i < w->session[1]; i++)
        if ((int32_t)(int8_t)w->session[0x0C + i] == (int32_t)car) return 0;
    return 1;
}

void carsys_track_vehicle_place(const CarsysWorld* w, int32_t which) {
    static const int32_t pos[2][3] = {{-0x2D40, -0xC80, -0x40}, {-0x2E00, -0x4B0, -0x40}};
    uint8_t* slot = w->platforms + (which ? 0x38 : 0);
    uint32_t handle = rdu32(slot + 0x34);
    if (!handle) return;
    uint8_t* ent = w->resolve(w->user, handle);
    int k = which ? 1 : 0;
    for (int j = 0; j < 3; j++) wr32(ent + 0xE8 + 4 * j, pos[k][j]);
    int16_t ang[3] = {0, 0, (int16_t)(k ? 0x800 : 0)};
    memcpy(ent + 0xE0, ang, 6);
    int32_t m[9];
    tm2_build_matrix(ang, m);
    memcpy(ent + 0xB0, m, sizeof m);
    wr32(slot, 0);
}

uint8_t carsys_area_jump_set(int32_t a) {
    return (uint8_t)((a > 3 && a < 12) || (a > 13 && a < 16) || (a > 16 && a < 19));
}

void carsys_dark_tooth_jump_select(const CarsysWorld* w, uint8_t* ai) {
    ai[0x451] = 0;
    ai[0x450] = 0;
    int32_t seg = rd16(ai + 0x458);
    if (seg != 2 && seg != 0x28 && seg != 0x0B && seg != 0x2C) return;
    int32_t x = rd32(ai + 0x728), y = rd32(ai + 0x72C);
    int in = 0;
    if (y > -0x2260 && y < -0x15E0) {
        if (x > 0x10E0 && y < 0x14A0) in = 1;
        else if (x > -0x14A0 && y < -0x10E0) in = 1;
    }
    if (!in) return;
    uint8_t human;
    const uint8_t* t = carsys_ai_target(w->rec, rd16(ai + 4), &human);
    int32_t area = rd16(t + (human ? 0xC6 : 0x45E));
    if (carsys_area_jump_set(area) && area > 7) ai[0x451] = 1;
    if (ai[0x451] == 0) ai[0x450] = 1;
}

void carsys_pickup_set_visible(const CarsysWorld* w, int32_t id, int32_t index, uint8_t value) {
    if (id < 0x191 || id > 0x1AE || index < 0 || index >= 0x32) return;
    uint8_t* o = w->pickups[index];
    if (o[0] == 1) o[0x14] = value;
    else if (o[0] == 4) o[0x12] = value;
}

void carsys_player_one_reset(const CarsysWorld* w) {
    for (int i = 0; i < 8; i++) w->player_one_flags[i] = 0;
    uint8_t* car = carsys_human_get(w->rec, 0);
    wr32(car + 0x1DC, 1);
    wr32(car + 0x848, 0);
    car[0x851] = 0;
    car[0x850] = 0;
    w->car_reset_43ac5c(w->user, car);
}

void carsys_bind_ai_slot(const CarsysWorld* w, int32_t index, int32_t slot) {
    const CarRecords* r = w->rec;
    uint8_t* obj = r->slot_objects[slot];
    r->ai_slot[index] = slot;
    r->slot_index[slot] = index;
    w->ai_objects[index] = obj;
    uint8_t* car = r->ai + (intptr_t)index * CARSYS_AI_SIZE;
    wr16(car + 6, (int16_t)slot);
    /* 0x0042a018(car) is an empty function */
    wr16(car + 4, (int16_t)index);
    w->stats_init(w->user, car, r->slot_owner[slot]);
    wr16(car + 4, (int16_t)index);
    r->slot_kind[slot] = 1;
    wr32(r->slot_objects[slot] + 0x158, index + 0x32);
    w->obj_register(w->user, r->slot_objects[slot]);
    w->car_init_4102f4(w->user, car);
}
