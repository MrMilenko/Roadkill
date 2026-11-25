/* gp_steer.c: see gp_steer.h */
#include "gameplay/gp_steer.h"

#include "core/tm2rand.h"
#include "math/tm2math.h"

#define OBJ_SIZE 0x160u

static int32_t iabs(int32_t v) {
    uint32_t m = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ m) - m);
}
static int32_t wsub(uint32_t a, uint32_t b) { return (int32_t)(a - b); }

/* the original's "max axis" distance: x against the larger of y, z */
static int32_t axis_dist(int32_t dx, int32_t dy, int32_t dz) {
    int32_t t = iabs(dy) < iabs(dz) ? dz : dy;
    return iabs(dx) < iabs(t) ? iabs(t) : iabs(dx);
}

static void wrap16(uint8_t* o, int off) {
    if (w_i16(o, off) > 0xFFF)
        w_s16(o, off, (uint32_t)(w_u16(o, off) - 0x1000));
    if (w_i16(o, off) < 0)
        o[off + 1] = (uint8_t)(o[off + 1] + 0x10);
}

void gp_proj_steer(ProjEnv* env, uint8_t* s) {
    int16_t rate = (int16_t)((w_i16(s, 0x08) * 5 * 4) / tm2_frame_fps());
    uint8_t* o = wmem_ptr(env->mem, w_u32(s, 0x34), OBJ_SIZE);
    const uint8_t* t = wmem_ptr(env->mem, w_u32(s, 0x38), 12);
    int32_t dx = wsub(w_u32(t, 0), w_u32(o, 0xE8));
    int32_t dy = wsub(w_u32(t, 4), w_u32(o, 0xEC));
    int32_t dz = wsub(w_u32(t, 8), w_u32(o, 0xF0));
    int32_t yaw = (tm2_atan2(dx, dy) + 0x1000) % 0x1000;
    int32_t mn = iabs(dx) < iabs(dy) ? dx : dy;
    int32_t mx = iabs(dx) < iabs(dy) ? dy : dx;
    int32_t pitch = tm2_atan2(dz, (iabs(mn) >> 1) + iabs(mx));
    pitch = (0x1000 - pitch) % 0x1000;
    if (pitch > 0x400 && pitch < 0xC00)
        pitch = (0x1400 - pitch) % 0x1000;
    int16_t type = w_i16(s, 0x02);

    int32_t cur = w_i16(o, 0xE4);
    if (cur - yaw > 0x800)
        yaw += 0x1000;
    if (yaw - cur > 0x800)
        yaw -= 0x1000;
    if (type == 0x29 && iabs(cur - yaw) > 0x155)
        w_s32(s, 0x38, 0);
    int left = 0, right = 0;
    if (rate < iabs(cur - yaw)) {
        left = yaw <= cur;
        if (left)
            w_s16(o, 0xE4, (uint32_t)(w_u16(o, 0xE4) - (uint32_t)rate));
        else
            w_s16(o, 0xE4, (uint32_t)(w_u16(o, 0xE4) + (uint32_t)rate));
        right = !left;
    } else {
        w_s16(o, 0xE4, (uint32_t)yaw);
    }
    if (type == 0x29) {
        int32_t r = tm2_rand() % (rate * 4);
        w_s16(o, 0xE4, (uint32_t)(w_u16(o, 0xE4) + (uint32_t)r - (uint32_t)(rate * 2)));
        int32_t d = axis_dist(wsub(w_u32(s, 0x28), w_u32(o, 0xE8)), wsub(w_u32(s, 0x2C), w_u32(o, 0xEC)),
                              wsub(w_u32(s, 0x30), w_u32(o, 0xF0)));
        if ((d >> 3) > 300) {
            w_s16(s, 0x0A, (uint32_t)(w_u16(s, 0x0A) - 1));
            if (w_i16(s, 0x0A) < 2)
                w_s16(s, 0x0A, 2);
            w_s32(o, 0x15C, (tm2_rand() & 1) ? env->models[2] : env->models[1]);
        }
    }
    if (type == 0x31) {
        int32_t r = tm2_rand() % (rate * 4);
        w_s16(o, 0xE4, (uint32_t)(w_u16(o, 0xE4) + (uint32_t)r - (uint32_t)(rate * 2)));
    }
    wrap16(o, 0xE4);

    cur = w_i16(o, 0xE0);
    if (cur - pitch > 0x800)
        pitch += 0x1000;
    if (pitch - cur > 0x800)
        pitch -= 0x1000;
    if (rate < iabs(cur - pitch)) {
        if (cur < pitch)
            w_s16(o, 0xE0, (uint32_t)(w_u16(o, 0xE0) + (uint32_t)rate));
        else
            w_s16(o, 0xE0, (uint32_t)(w_u16(o, 0xE0) - (uint32_t)rate));
    } else {
        w_s16(o, 0xE0, (uint32_t)pitch);
    }
    if (type == 0x29 || type == 0x31) {
        int32_t r = tm2_rand() % (rate * 2);
        w_s16(o, 0xE0, (uint32_t)(w_u16(o, 0xE0) + (uint32_t)(r - rate)));
    }
    wrap16(o, 0xE0);

    if (type == 0x33) {
        int32_t d = axis_dist(wsub(w_u32(s, 0x28), w_u32(o, 0xE8)), wsub(w_u32(s, 0x2C), w_u32(o, 0xEC)),
                              wsub(w_u32(s, 0x30), w_u32(o, 0xF0)));
        uint32_t tobj = w_u32(s, 0x3C);
        const uint8_t* to = wmem_ptr(env->mem, tobj, OBJ_SIZE);
        if ((d >> 3) > 300) {
            uint32_t ta = w_u32(s, 0x38);
            if (ta != 0 && w_u32(wmem_ptr(env->mem, ta, 4), 0) != w_u32(to, 0xE8)) {
                w_s16(s, 0x08, 0x1E);
                uint32_t dmg = w_u16(s, 0x0A);
                w_s16(s, 0x06, 0x32);
                w_s16(s, 0x0A, dmg << 2);
            }
            w_s32(s, 0x38, tobj + 0xE8);
        }
        const uint8_t* tp = wmem_ptr(env->mem, w_u32(s, 0x38), 8);
        if (w_u32(to, 0xE8) == w_u32(tp, 0) && w_u32(tp, 4) == w_u32(to, 0xEC)) {
            int32_t d2 = axis_dist(wsub(w_u32(to, 0xE8), w_u32(o, 0xE8)), wsub(w_u32(to, 0xEC), w_u32(o, 0xEC)),
                                   wsub(w_u32(to, 0xF0), w_u32(o, 0xF0)));
            if ((d2 >> 3) < 0x14) {
                int32_t idx = (int8_t)s[0];
                uint8_t* e = env->slots + idx * PROJ_SLOT_SIZE;
                w_s16(e, 0x04, 0xFFFF);
                w_s32(wmem_ptr(env->mem, w_u32(e, 0x34), OBJ_SIZE), 0x08, 0);
            }
            w_s16(o, 0xE4, (uint32_t)tm2_angle_step(w_i16(o, 0xE4), yaw, 9));
        }
        int32_t target = left ? 0x400 : right ? -0x400 : 0;
        w_s16(o, 0xE2, (uint32_t)tm2_angle_step(w_i16(o, 0xE2), target, (left || right) ? 0x0D : 5));
    }
    int16_t a[3] = {w_i16(o, 0xE0), w_i16(o, 0xE2), w_i16(o, 0xE4)};
    int32_t m[9];
    tm2_build_matrix(a, m);
    for (int i = 0; i < 9; i++)
        w_s32(o, 0xB0 + 4 * i, (uint32_t)m[i]);
}
