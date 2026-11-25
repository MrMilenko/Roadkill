/*
 * ai_slots.h: car model slots (which car types are loaded), ported from TM2.EXE 0x00457428 and
 * 0x00458340. Verified under emulation (tools/emu/check_ai_slots.sh).
 */
#ifndef TM2_AI_SLOTS_H
#define TM2_AI_SLOTS_H

#include <stdint.h>

#define CAR_SLOTS_MAX 15

typedef struct {
    int16_t* type;      /* 0x00503028: car type per slot */
    int32_t* model;     /* 0x00503048: model handle per slot */
    uint8_t* status;    /* 0x00502fdc: 2 = registered for an AI car */
    int32_t* count;     /* 0x00503084 */
    int32_t* ai_count;  /* 0x0050c2b0 */
    /* 0x00492e50(model, 0): model load / reference, not ported */
    void (*model_ref)(void* user, int32_t model);
    /* 0x004583c8(arg, slot, type): binds the slot's model, not ported */
    void (*bind)(void* user, int32_t arg, int32_t slot, int32_t type);
    void* user;
} CarSlots;

/* 0x00457428: appends (model, type) as a new slot with status 2 while fewer than 15 (count >= 0;
 * the original would write before the tables for a negative count). */
void car_register_model(const CarSlots* s, int32_t model, int16_t type);

/* 0x00458340: finds the first slot of car type `type`; if found, counts one more AI car when its
 * status is 2, then binds it (arg passed through). */
void car_bind_ai_by_type(const CarSlots* s, int32_t arg, int16_t type);

#endif /* TM2_AI_SLOTS_H */
