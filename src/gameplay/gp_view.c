/* gp_view.c: see gp_view.h */
#include "gameplay/gp_view.h"

#include "weapons/wmem.h"

static void copy_pos(uint8_t* car) {
    for (int k = 0; k < 3; k++)
        w_s32(car, 0x14 + 4 * k, w_u32(car, 0x6E4 + 4 * k));
}

void gp_car_set_view_mode(GpViewEnv* env, uint8_t* car, int8_t mode) {
    if (mode <= 4)
        car[0x107] = (uint8_t)mode;
    int placed = 0;
    if (mode <= 7) {
        static const int32_t zp[3] = {0, 0, 0};
        static const int16_t za[3] = {0, 0, 0};
        env->place(env->user, 0, zp, za);
        placed = 1;
    }
    int reset = 0;
    switch ((uint8_t)mode) {
    case 2: reset = placed && car[0x106] == 2; break;
    case 3:
    case 4:
        if (placed && car[0x106] == (uint8_t)mode)
            copy_pos(car);
        break;
    case 7:
        for (int k = 0; k < 3; k++)
            w_s32(car, 0x14 + 4 * k, w_u32(env->watched(env->user), 0x728 + 4 * k));
        break;
    default: reset = 1; break;
    }
    if (reset) {
        w_s16(car, 0x0C, 0);
        w_s16(car, 0x0E, 0);
        copy_pos(car);
        w_s16(car, 0x10, w_u16(car, 0x6F8));
    }
    env->view_pos(env->user, car + 0x14, w_i16(car, 0x00));
    env->view_rot(env->user, car + 0x0C, w_i16(car, 0x00));
    car[0x106] = (uint8_t)mode;
}
