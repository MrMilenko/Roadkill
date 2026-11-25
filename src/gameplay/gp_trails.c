/* gp_trails.c: see gp_trails.h */
#include "gameplay/gp_trails.h"

void gp_missile_trails_init(GpTrailEnv* env) {
    for (int i = 0; i < 16; i++) {
        uint8_t* s = env->slots + i * 0x4BC;
        for (int j = 0; j < 18; j++) {
            uint8_t* q = s + 0x15C + 0x30 * j;
            q[0x01] = 1;
            q[0x02] = 0x0C;
            q[0x03] = 9;
            w_s16(q, 0x04, 0);
            w_s16(q, 0x06, 1);
            w_s16(q, 0x08, 2);
            w_s16(q, 0x0A, 3);
            q[0x1B] = 0x2E;
            w_s32(q, 0x10, 0x10842000u);
            q[0x00] = (uint8_t)((q[0x00] | 4) & 4);
            q[0x18] = 0x80;
            q[0x19] = 0x80;
            q[0x1A] = 0x80;
        }
    }
    for (int i = 0; i < 16; i++) {
        uint8_t* s = env->slots + i * 0x4BC;
        uint32_t sa = env->slots_addr + (uint32_t)i * 0x4BC;
        w_s32(s, 0x104, 0x12);
        w_s16(s, 0x10A, 1);
        w_s32(s, 0x11C, 0x7FFF);
        s[0xF4] = 0;
        w_s16(s, 0xF6, 0);
        w_s32(s, 0xFC, 0);
        w_s32(s, 0x10C, 0);
        w_s16(s, 0x108, 0);
        w_s32(s, 0x110, 0);
        w_s32(s, 0x114, 0);
        w_s32(s, 0xF8, sa + 0x48);
        w_s32(s, 0x10C, 0);
        w_s32(s, 0x100, sa + 0x15C);
        uint8_t b = s[0x10D];
        w_s32(s, 0x118, 0);
        s[0x10D] = (uint8_t)(b | 0x20);
        uint32_t o = env->obj_new(env->user);
        w_s32(s, 0xF0, o);
        w_s32(wmem_ptr(env->mem, o, 0x160), 0x15C, sa + 0xF4);
        w_s32(wmem_ptr(env->mem, w_u32(s, 0xF0), 0x160), 0x08, 0);
        w_s32(wmem_ptr(env->mem, w_u32(s, 0xF0), 0x160), 0xE8, 0);
        w_s32(wmem_ptr(env->mem, w_u32(s, 0xF0), 0x160), 0xEC, 0);
        w_s32(wmem_ptr(env->mem, w_u32(s, 0xF0), 0x160), 0xF0, 0);
        env->obj_add(env->user, w_u32(s, 0xF0));
    }
}
