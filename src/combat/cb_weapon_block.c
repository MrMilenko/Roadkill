/* cb_weapon_block.c: see cb_weapon_block.h */
#include "combat/cb_weapon_block.h"

uint32_t cb_weapon_block_init(const CbWeaponBlockEnv* env, const uint8_t* car, uint32_t block, int ai_layout) {
    uint8_t* b = wmem_ptr(env->mem, block, 0x48);
    uint32_t type = (uint32_t)(w_i32(car, (uint8_t)ai_layout ? 0x8E : 0x2E) >> 16);
    b[6] = 0;
    b[4] = 0;
    w_s16(b, 0x34, 0);
    w_s16(b, 0x28, 0);
    w_s16(b, 0x26, 0);
    w_s16(b, 0x36, 0);
    b[7] = 0;
    b[5] = 0;
    for (int k = 0; k < 10; k++)
        w_s16(b, 0x0E + 2 * k, 0);
    b[0x0D] = 0;
    w_s16(b, 0x0E, 2);
    w_s16(b, 0x10, 2);
    w_s16(b, 0x18, 1);
    w_s16(b, 0x2E, 0);
    w_s16(b, 0x2A, 99);
    w_s16(b, 0x32, 200);
    w_s16(b, 0x22, 5);
    b[0x0C] = 0x6B;
    w_s16(b, 0x30, 3000);
    w_s16(b, 0x1E, 0);
    uint32_t ret = type;
    int lim = -1;
    int sp = 0;
    switch (type) {
    case 0x10: lim = 5; sp = 0x438; break;
    case 0x20: lim = 5; sp = 0xE10; break;
    case 0x30: lim = 3; sp = 0xCE4; break;
    case 0x40: lim = 5; sp = 0xE10; break;
    case 0x50: lim = 3; sp = 0x1068; break;
    case 0x60: lim = 5; sp = 0x834; break;
    case 0x70: lim = 5; sp = 0x12C0; break;
    case 0x80: lim = 4; sp = 0x834; break;
    case 0x90: lim = 5; sp = 0x834; break;
    case 0xA0: lim = 3; sp = 0x438; break;
    case 0xB0: lim = 5; sp = 0x834; break;
    case 0xD0: lim = 5; sp = 0x4B0; break;
    case 0xE0: lim = 5; sp = 0x438; break;
    case 0xF0: lim = 5; sp = 0x834; break;
    case 0xC0:
        w_s16(b, 0x2C, 3000);
        ret = (uint32_t)env->slam_state_init(env->user, block);
        w_s16(b, 0x24, 0x18);
        b[0x0B] = 3;
        break;
    default:
        b[0x0B] = 5;
        w_s16(b, 0x24, 10);
        break;
    }
    if (lim >= 0) {
        w_s16(b, 0x24, 0x18);
        b[0x0B] = (uint8_t)lim;
        w_s16(b, 0x2C, (uint32_t)sp);
    }
    uint32_t o = w_u32(b, 0x44);
    if (o)
        w_s16(wmem_ptr(env->mem, o + 0x0C, 2), 0, 0);
    o = w_u32(b, 0x40);
    if (o)
        w_s16(wmem_ptr(env->mem, o + 0x0C, 2), 0, 0);
    return ret;
}

void cb_car_turbo_set(uint8_t* car, int on, void (*stats_derive)(void* user, uint8_t* stats), void* user) {
    if ((uint8_t)on) {
        w_s16(car, 0x174, 1);
        w_s16(car, 0x1B2, 100);
        w_s16(car, 0x18A, (uint16_t)(w_u16(car, 0x18A) + 0x3C));
    } else {
        w_s16(car, 0x174, 0);
        int a, t;
        switch (w_u16(car, 0x90)) {
        case 0x10: a = 0x1E; t = 0x5C; break;
        case 0x20: a = 0x23; t = 0x60; break;
        case 0x30: a = 0x19; t = 0x7F; break;
        case 0x40: a = 0x1E; t = 0x65; break;
        case 0x50: a = 0x05; t = 0x7D; break;
        case 0x60: a = 0x0F; t = 0x79; break;
        case 0x70: a = 0x19; t = 0x6F; break;
        case 0x80: a = 0x0D; t = 0x83; break;
        case 0xA0: a = 0x0E; t = 0x8D; break;
        case 0xB0: a = 0x1E; t = 0x67; break;
        case 0xC0: a = 0x23; t = 0x5C; break;
        case 0xD0: a = 0x19; t = 0x71; break;
        case 0xE0: a = 0x3C; t = 0x7F; break;
        case 0xF0: a = 0x64; t = 0x7F; break;
        default: a = 0x1C; t = 0x77; break;
        }
        w_s16(car, 0x1B2, (uint32_t)a);
        w_s16(car, 0x18A, (uint32_t)t);
    }
    /* the top speed as stored (read back through i32 +0x188 >> 16) */
    int32_t top = w_i32(car, 0x188) >> 16;
    w_s16(car, 0x18A, (uint32_t)top);
    w_s16(car, 0x18C, (uint32_t)(top * 3 / 4));
    w_s16(car, 0x192, (uint32_t)(top * 2 / 3));
    stats_derive(user, car + 0x138);
}
