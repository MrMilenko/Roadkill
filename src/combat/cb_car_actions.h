/*
 * cb_car_actions.h: four entry points that make human car i act at once (TM2.EXE 0x0044df18..
 * 0x0044e050). Each looks the car up with human_car_get 0x0045afbc(i) (a game address; its fields
 * are written through the WMem). While the action runs the car's u8 +0xA7 is 0 and is restored after.
 *
 *   0x0044df18 cb_car_fire_special_slot(i, slot): i16 +0x86E = 0, +0x152 = 0, u8 +0xE7 = 1, +0xEA = 1,
 *              +0x855 = slot, i16 +0x856 + 2 * slot (i8 slot) = 1, weaponFireSpecial 0x004401fc(car, 1).
 *   0x0044df78 cb_car_fire_guns(i, v): i16 +0x870 = 0, u8 +0x84F = v, i16 +0x152 = 0, 0x0046fea4 (eax =
 *              car), UpdateGuns 0x00443c9c(car, 1); then when u8 +0x9A or the saved +0xA7 is set,
 *              0x0046f860(9, i) (rumble_stop).
 *   0x0044dfe8 cb_car_flag_84d(i): u8 +0x84D = 1.
 *   0x0044dff8 cb_car_fire_0044342c(i, v): i16 +0x86E = 0, +0x152 = 0, u8 +0xE9 = 1, +0xEA = 1,
 *              i16 +0x878 = 3000, u8 +0x854 = v, 0x0044342c(car, 1).
 *
 * Verified against the original under emulation (tools/emu/check_combat_actions.sh).
 */
#ifndef TM2_CB_CAR_ACTIONS_H
#define TM2_CB_CAR_ACTIONS_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    uint32_t (*human_car_get)(void* user, int32_t i);          /* 0x0045afbc */
    void (*fire_special)(void* user, uint32_t car, int32_t a); /* 0x004401fc */
    void (*pre_guns)(void* user, uint32_t car);                /* 0x0046fea4 */
    void (*guns_update)(void* user, uint32_t car, int32_t a);  /* 0x00443c9c */
    void (*rumble)(void* user, int32_t a, int32_t i);          /* 0x0046f860 */
    void (*fire_44342c)(void* user, uint32_t car, int32_t a);  /* 0x0044342c */
    void* user;
} CbCarActionsEnv;

void cb_car_fire_special_slot(const CbCarActionsEnv* e, int32_t i, uint8_t slot);
void cb_car_fire_guns(const CbCarActionsEnv* e, int32_t i, uint8_t v);
void cb_car_flag_84d(const CbCarActionsEnv* e, int32_t i);
void cb_car_fire_0044342c(const CbCarActionsEnv* e, int32_t i, uint8_t v);

#endif /* TM2_CB_CAR_ACTIONS_H */
