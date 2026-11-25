/* gp_misc.c: see gp_misc.h */
#include "gameplay/gp_misc.h"

void gp_effects_reset(GpMiscEnv* env) {
    for (int k = 0; k < 40; k++)
        w_s32(env->effects, k * 0x38, 0);
}

void gp_effects_reset_all(GpMiscEnv* env) {
    gp_effects_reset(env);
    for (int i = 0; i < 16; i++) {
        uint8_t* s = env->slots + i * 0x4BC;
        uint32_t o = w_u32(s, 0x34);
        w_s16(s, 0x04, 0xFFFF);
        if (o != 0)
            w_s32(wmem_ptr(env->mem, o + 8, 4), 0, 0);
    }
    *env->flash_frames = 0;
}

void gp_flash_fade(GpMiscEnv* env) {
    int32_t f = (int32_t)((uint32_t)*env->flash_frames - 1u);   /* wraps like the original */
    *env->flash_frames = f;
    if (f >= 8)
        return;
    uint8_t* c = env->flash;
    for (int k = 0; k < 3; k++)
        c[k] = (uint8_t)(((int32_t)c[k] * 3 + 0x80) >> 2);
    for (int k = 0; k < 3; k++)
        c[0x18 + k] = c[k];
}

void gp_hud_msg_reset(GpMiscEnv* env) {
    env->msg[0] = env->msg_owner(env->user);
    env->msg[2] = 0x32;
    env->msg[4] = 0x32;
    env->msg[1] = env->msg_owner(env->user);
    env->msg[3] = 0x32;
}

void gp_hud_msg_post(GpMiscEnv* env, int32_t owner, int32_t prio) {
    if (prio < env->msg[2]) {
        env->msg[0] = owner;
        env->msg[2] = prio;
        env->msg[4] = 0x32;
        env->msg[1] = env->msg_owner(env->user);
        env->msg[3] = 0x32;
    } else {
        env->msg[3] = prio;
        env->msg[1] = owner;
    }
}

void gp_hud_color(int32_t kind, uint8_t* prim) {
    uint8_t a, b, c;   /* +4, +5, +6 */
    switch ((uint32_t)kind) {
    case 0: a = 0xFF; b = 0x00; c = 0x00; break;
    case 1: a = 0xFF; b = 0x2A; c = 0x00; break;
    case 2: a = 0xBA; b = 0x9B; c = 0x20; break;
    default: a = 0x00; b = 0x70; c = 0x0A; break;
    }
    prim[5] = b;
    prim[6] = c;
    prim[4] = a;
}

void gp_toggle_a(GpMiscEnv* env, int32_t i) { env->toggle_a[i] = (uint8_t)(1 - env->toggle_a[i]); }
void gp_toggle_b(GpMiscEnv* env, int32_t i) { env->toggle_b[i] = (uint8_t)(1 - env->toggle_b[i]); }

void gp_cycle(GpMiscEnv* env, int32_t i) {
    env->cycle[i]++;
    if ((uint8_t)env->multiplayer(env->user) != 0) {
        if (env->cycle[i] > 2)
            env->cycle[i] = 0;
    } else if (env->cycle[i] > 1) {
        env->cycle[i] = 0;
    }
}
