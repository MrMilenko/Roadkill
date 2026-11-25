/*
 * ai_slots.c: car model slots, ported from TM2.EXE (see ai_slots.h).
 */
#include "ai_slots.h"

void car_register_model(const CarSlots* s, int32_t model, int16_t type) {
    int32_t n = *s->count;
    if (n >= CAR_SLOTS_MAX) return;
    s->type[n] = type;
    s->model[n] = model;
    s->model_ref(s->user, model);
    n = *s->count;
    s->status[n] = 2;
    *s->count = n + 1;
}

void car_bind_ai_by_type(const CarSlots* s, int32_t arg, int16_t type) {
    int32_t slot = -1;
    for (int32_t i = 0; i < *s->count; i++) {
        if (s->type[i] == type) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return;
    if (s->status[slot] == 2) *s->ai_count += 1;
    s->bind(s->user, arg, slot, type);
}
