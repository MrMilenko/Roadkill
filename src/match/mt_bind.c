/*
 * mt_bind.c: human car to object slot binding (see mt_bind.h).
 */
#include "mt_bind.h"

#include <string.h>

static void w16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void w32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }

void mt_bind_human_slot(const MtBind* b, int32_t i, int32_t key) {
    int32_t n = *b->slot_count, s;
    if (n <= 0) return;
    for (s = 0; 2 * s < 2 * n; s++)
        if ((int16_t)key == b->slot_owner[s]) break;
    if (2 * s >= 2 * n) return;
    int16_t owner = b->slot_owner[s];
    uint8_t* car = b->human + (uint32_t)i * 0x898u;
    b->human_key[i] = owner;
    b->human_slot[i] = s;
    b->human_objects[i] = b->slot_objects[s];
    b->slot_index[s] = i;
    w16(car + 2, (int16_t)s);
    b->human_key[i] = (int16_t)key;
    w16(car, (int16_t)i);
    b->car_init_entity(b->user, car);
    if (b->is_multiplayer(b->user)) {
        if (*b->keep_208 == 0) {
            int32_t v;
            memcpy(&v, b->human + 0x208, 4);
            w32(car + 0x208, v > 0 ? v - 1 : v + 1);
        }
    }
    b->car_setup(b->user, car, b->slot_owner[s], b->handicap[i]);
    if (*b->singleplayer == 0) b->car_set_state(b->user, car, 2);
    b->slot_kind[s] = 0;
    w32(b->resolve(b->user, b->slot_objects[s]) + 0x158, i + 1);
    w16(car, (int16_t)i);
    b->object_activate(b->user, b->slot_objects[s], 1);
}
