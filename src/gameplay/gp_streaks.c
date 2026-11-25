/* gp_streaks.c: see gp_streaks.h */
#include "gameplay/gp_streaks.h"

void gp_streaks_draw(GpStreakEnv* env, uint32_t view, uint32_t arg) {
    uint8_t l[0x44] = {0};
    env->light_init(env->user, l, 1);
    env->light_mode(env->user, l, 0, 0);
    if (*env->flash_frames > 0)
        env->flash_fade(env->user, w_u32(wmem_ptr(env->mem, view, 4), 0));
    env->pass_a(env->user, view, arg);
    env->pass_b(env->user);
    env->pass_c(env->user, view, arg);
    for (int k = 0; k < 40; k++) {
        const uint8_t* e = env->streaks + k * 0x38;
        if (w_i32(e, 0x28) <= 0)
            continue;
        env->light_dir(env->user, l, 0, w_i32(e, 0x00), w_i32(e, 0x04), w_i32(e, 0x08));
        env->light_dir(env->user, l, 1, w_i32(e, 0x0C), w_i32(e, 0x10), w_i32(e, 0x14));
        if (e[0x34] != 0) {
            env->light_col(env->user, l, 0, 0xFF, 0xFF, 0xFF);
            env->light_col(env->user, l, 1, 0xFF, 0xFF, 0xFF);
        } else {
            int32_t c = (w_i32(e, 0x28) & 0x1F) + 0xDF;
            env->light_col(env->user, l, 0, c & 0xFF, c, c & 0xFF);
            int32_t d = c - 0x3C;
            env->light_col(env->user, l, 1, d & 0xFF, (d >> 1) & 0xFF, 0);
        }
        env->draw(env->user, view, l);
    }
}
