/* cb_target.c: see cb_target.h */
#include "combat/cb_target.h"

#include "math/tm2math.h"

static int32_t iabs(int32_t v) {
    uint32_t s = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ s) - s);
}

static uint32_t rd32(const CbTargetEnv* e, uint32_t a) { return w_u32(wmem_ptr(e->mem, a, 4), 0); }

static uint32_t self_object(const CbTargetEnv* e, int32_t i, int human) {
    return human ? e->human_object(e->user, i) : e->ai_object(e->user, i);
}

/* local = M(shooter) * (cand - shooter pos) */
static void to_local(const CbTargetEnv* e, int32_t i, int human, uint32_t self_pos, uint32_t cand, int32_t local[3]) {
    int32_t d[3], m[9];
    for (int k = 0; k < 3; k++)
        d[k] = (int32_t)(rd32(e, cand + 4 * (uint32_t)k) - rd32(e, self_pos + 4 * (uint32_t)k));
    uint32_t mo = self_object(e, i, human) + 0xB0;
    for (int k = 0; k < 9; k++)
        m[k] = (int32_t)rd32(e, mo + 4 * (uint32_t)k);
    tm2_mat_vec_mul_col(m, d, local);
}

static void pick(const CbTargetEnv* e, int32_t i, uint32_t* out, int is_human, int rear) {
    uint8_t human = (uint8_t)is_human;
    int32_t best = 0;
    if (!human && (uint8_t)e->singleplayer(e->user) == 0) {
        *out = e->human_object(e->user, 0) + 0xE8;
        return;
    }
    uint32_t self_pos = self_object(e, i, human) + 0xE8;
    int32_t n = e->ai_count(e->user);
    *out = 0;
    for (int32_t j = 0; j < n; j++) {
        uint32_t cand = e->ai_object(e->user, j) + 0xE8;
        if (cand == 0)
            continue;
        if ((int32_t)rd32(e, e->ai_get(e->user, j) + 0x58C) < 1)
            continue;
        int32_t l[3];
        to_local(e, i, human, self_pos, cand, l);
        if (!rear) {
            if (l[1] < 12 || iabs(l[0]) > iabs(l[1]) || iabs(l[1]) > 0x1F40 || l[2] > 0x190)
                continue;
            if (best >= 1 && best <= l[1])
                continue;
            best = l[1];
        } else {
            if (l[1] > -12 || iabs(l[0]) > iabs(l[1]) || iabs(l[1]) > 0x1F40)
                continue;
            if (best >= 1 && iabs(l[1]) >= best)
                continue;
            best = iabs(l[1]);
        }
        *out = cand;
    }
    if (best >= 1)
        return;
    n = e->human_count(e->user);
    *out = 0;
    for (int32_t j = 0; j < n; j++) {
        uint32_t cand = e->human_object(e->user, j) + 0xE8;
        if (!rear) {
            if (cand == 0)
                continue;
            if (human && j == i)
                continue;
            if ((int32_t)rd32(e, e->human_get(e->user, j) + 0x1D8) < 1)
                continue;
        } else {
            if (human && j == i)
                continue;
            if (cand == 0)
                continue;
        }
        int32_t l[3];
        to_local(e, i, human, self_pos, cand, l);
        if (!rear) {
            if (l[1] < 12 || iabs(l[0]) > iabs(l[1]) || iabs(l[1]) > 0x1F40)
                continue;
            if (best >= 1 && best <= l[1])
                continue;
            best = l[1];
        } else {
            if (l[1] > -12 || iabs(l[0]) > iabs(l[1]) || iabs(l[1]) > 0x1F40)
                continue;
            if (best >= 1 && iabs(l[1]) >= best)
                continue;
            best = iabs(l[1]);
        }
        *out = cand;
    }
}

void cb_target_pick_front(const CbTargetEnv* e, int32_t i, uint32_t* out, int is_human) { pick(e, i, out, is_human, 0); }
void cb_target_pick_rear(const CbTargetEnv* e, int32_t i, uint32_t* out, int is_human) { pick(e, i, out, is_human, 1); }
