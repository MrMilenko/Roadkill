/* gp_landmark.c: see gp_landmark.h */
#include "gameplay/gp_landmark.h"

#include "core/tm2rand.h"
#include "math/tm2math.h"
#include "physics/grp.h"

#define TABLE_A 0x004f004cu
#define TABLE_B 0x004f0420u
#define REC 0xC4u
#define BURN 0x004f07f4u

/* host pointer of game address a inside the block (callers keep a inside it) */
static uint8_t* G(GpLmEnv* env, uint32_t a) { return env->d + (a - GP_LM_BASE); }
static uint32_t part_of(GpLmEnv* env, uint32_t obj) { return w_u16(wmem_ptr(env->mem, obj + 0x10, 2), 0); }

void gp_landmarks_reset(GpLmEnv* env) {
    for (int t = 0; t < 2; t++) {
        uint32_t base = t ? TABLE_B : TABLE_A;
        for (uint32_t r = 0; r < 5; r++) {
            uint8_t* p = G(env, base + r * REC);
            w_s16(p, 0x0C, 0xFFFF);
            p[0x00] = 0;
            p[0x01] = 0;
            for (int k = 4; k <= 9; k++)
                p[k] = 0;
        }
    }
    *G(env, BURN + 5) = 0;
    *G(env, BURN) = 0;
    w_s32(G(env, BURN), 0x0C, 0);
    for (uint32_t k = 0; k < 5; k++) {
        w_s32(G(env, 0x004f087cu + k * 0x14), 0, 0);
        w_s32(G(env, 0x004f0880u + k * 0x14), 0, 0);
        w_s32(G(env, 0x004f082cu + k * 0x10), 0, 0);
    }
}

void gp_landmark_a_add(GpLmEnv* env, uint32_t obj, const int32_t pos[3], int32_t r) {
    uint32_t rec = TABLE_A + (uint32_t)r * REC;
    *G(env, rec) = 1;
    w_s32(G(env, rec + 0x24 + part_of(env, obj) * 4), 0, obj);
    for (uint32_t k = 0; k < 3; k++)   /* the part is read again before each store */
        w_s32(G(env, rec + 0x38 + 4 * k + part_of(env, obj) * 0x0C), 0, (uint32_t)pos[k]);
}

uint32_t gp_landmark_a_addr(int32_t r) { return TABLE_A + (uint32_t)r * REC; }

void gp_landmark_b_mark(GpLmEnv* env) {
    for (uint32_t r = 0; r < 5; r++) {
        uint8_t* p = G(env, TABLE_B + r * REC);
        if (p[0] != 0)
            w_s16(p, 0x0C, 6);
    }
}

void gp_landmark_b_add(GpLmEnv* env, uint32_t obj, int32_t unused, int32_t r) {
    (void)unused;
    static const uint32_t off[3] = {0, 0xFFFFFFD6u, 0xF0};
    uint32_t rec = TABLE_B + (uint32_t)r * REC;
    *G(env, rec) = 1;
    w_s32(G(env, rec + 0x24 + part_of(env, obj) * 4), 0, obj);
    for (uint32_t k = 0; k < 3; k++)
        w_s32(G(env, rec + 0x38 + 4 * k + part_of(env, obj) * 0x0C), 0, off[k]);
}

void gp_burn_set(GpLmEnv* env, uint32_t obj, const int32_t pos[3]) {
    uint8_t* b = G(env, BURN);
    w_s32(b, 0x0C, obj);
    w_s32(b, 0x10, (uint32_t)pos[0]);
    w_s32(b, 0x14, (uint32_t)pos[1]);
    b[0x01] = 1;
    w_s32(b, 0x18, (uint32_t)pos[2]);
}

/* +-rand() / 0x1999: the sign from a first rand() (odd: positive) */
static int16_t spin(void) {
    int32_t v = (tm2_rand() & 1) ? tm2_rand() : -tm2_rand();
    return (int16_t)(v / 0x1999);
}

/* 0x00411a0c */
static void burn_start(GpLmEnv* env) {
    uint8_t* b = G(env, BURN);
    if (w_u32(b, 0x0C) == 0)
        return;
    w_s16(b, 0x06, 0);
    b[0x03] = 0x1E;
    b[0x00] = 1;
    b[0x02] = 0;
    b[0x04] = 0;
    w_s16(b, 0x0A, (uint32_t)((tm2_rand() >> 9) + 0x14));
    w_s16(b, 0x30, (uint32_t)(tm2_rand() / 8));
    int32_t r = tm2_rand() / 8;
    w_s16(b, 0x34, 0xFFF);
    w_s16(b, 0x32, (uint32_t)r);
    r = (tm2_rand() >> 9) + 100;
    b[0x05] = 0;
    w_s16(b, 0x08, (uint32_t)r);
    w_s16(b, 0x1C, (uint16_t)spin());
    w_s16(b, 0x1E, (uint16_t)spin());
    w_s32(b, 0x24, 0);
    w_s32(b, 0x28, 0);
    w_s32(b, 0x2C, 0xFFFFFFFAu);
    w_s16(b, 0x20, 0);
}

void gp_burn_countdown(GpLmEnv* env, int8_t n) {
    uint8_t* b = G(env, BURN);
    b[0x01] = (uint8_t)(b[0x01] - (uint8_t)n);
    if ((int8_t)b[0x01] > 0)
        return;
    burn_start(env);
}

void gp_burn_point_set(GpLmEnv* env, const int32_t pos[3], int32_t i) {
    for (uint32_t k = 0; k < 3; k++)
        w_s32(G(env, 0x004f0830u + (uint32_t)i * 0x10 + 4 * k), 0, (uint32_t)pos[k]);
}

void gp_debris_point_set(GpLmEnv* env, const int32_t pos[3], int32_t i) {
    for (uint32_t k = 0; k < 3; k++)
        w_s32(G(env, 0x004f0884u + (uint32_t)i * 0x14 + 4 * k), 0, (uint32_t)pos[k]);
}

void gp_mount_set(GpLmEnv* env, uint32_t obj) {
    w_s32(G(env, 0x004f08ecu), 0, obj);
    w_s32(G(env, 0x004f08fcu), 0, 0);
    w_s32(G(env, 0x004f0900u), 0, 0xFFFFE800u);
    w_s32(G(env, 0x004f0904u), 0, 0xF0);
    w_s16(G(env, 0x004f0908u), 0, 0);
    w_s16(G(env, 0x004f090au), 0, 0);
    w_s16(G(env, 0x004f090cu), 0, 0);
}

void gp_mount_part_set(GpLmEnv* env, uint32_t v, int32_t i) { w_s32(G(env, 0x004f08f0u + 4 * (uint32_t)i), 0, v); }

void gp_trail_frames_set(GpLmEnv* env, uint32_t v) { *env->trail_frames = v; }

static int32_t wabs(int32_t v) {
    uint32_t m = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ m) - m);
}
static int32_t wsub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

typedef void (*BreakFn)(GpLmEnv* env, int32_t r);

/* 0x00410890 / 0x00411334: records 1 .. nrec - 1, part positions 1 .. nparts */
static void blast(GpLmEnv* env, uint32_t table, int32_t nrec, int nparts, BreakFn brk, const int32_t pos[3]) {
    int32_t zone = grp_find_zone(env->grp, pos[0], pos[1], pos[2]);
    for (int32_t r = 1; r < nrec; r++) {
        uint8_t* rec = G(env, table + (uint32_t)r * REC);
        if (rec[0] == 0)
            continue;
        if (w_i16(rec, 0x0C) == zone) {
            brk(env, r);
            continue;
        }
        for (int p = 1; p <= nparts; p++) {
            const uint8_t* q = rec + 0x38 + p * 0x0C;
            int32_t dx = wabs(wsub(pos[0], w_i32(q, 0)));
            int32_t dy = wabs(wsub(pos[1], w_i32(q, 4)));
            int32_t dz = wabs(wsub(pos[2], w_i32(q, 8)));
            int32_t m = dy < dz ? dz : dy;
            int32_t d = dx < m ? m : dx;
            if ((d >> 3) < 100) {
                brk(env, r);
                break;
            }
        }
    }
}

void gp_landmark_a_blast(GpLmEnv* env, const int32_t pos[3]) { blast(env, TABLE_A, 5, 4, gp_landmark_a_break, pos); }
void gp_landmark_b_blast(GpLmEnv* env, const int32_t pos[3]) { blast(env, TABLE_B, 3, 2, gp_landmark_b_break, pos); }

/* shared start of 0x00410b34 / 0x004114c0: 0 when the record does not break */
static uint8_t* break_parts(GpLmEnv* env, uint32_t table, int32_t r, int nparts, int32_t height) {
    uint8_t* rec = G(env, table + (uint32_t)r * REC);
    if (rec[1] != 0 || rec[4] != 0 || rec[0] == 0)
        return 0;
    w_s16(rec, 0x0A, 0);
    rec[0x03] = 0x1E;
    rec[0x01] = 1;
    rec[0x02] = 0;
    rec[0x04] = 0;
    for (int k = 0; k < nparts; k++) {
        int32_t v = tm2_rand() >> 9;
        if (v <= 0x14)
            v = 0x14;
        else
            v = tm2_rand() >> 9;
        w_s16(rec, 0x1A + 2 * k, (uint32_t)v);
        w_s16(rec, 0xA4 + 8 * k, (uint32_t)(tm2_rand() / 8));
        v = tm2_rand() / 8;
        w_s16(rec, 0xA8 + 8 * k, 0xFFF);
        w_s16(rec, 0xA6 + 8 * k, (uint32_t)v);
        w_s16(rec, 0x10 + 2 * k, (uint32_t)((tm2_rand() >> 9) + height));
        rec[0x06 + k] = 0;
    }
    return rec;
}

/* fall vectors of records 1..4: i16 at +0x7C, +0x7E, +0x80, +0x84, +0x86, +0x88, +0x8C, +0x8E,
 * +0x90, +0x94, +0x96, +0x98 (table B: the first 6) */
static const int16_t fall_a[4][12] = {
    {-7, 0, 0, 7, 0, 0, -15, 0, 0, -20, 0, 0},
    {0, 7, 0, 0, -7, 0, 0, 15, 0, 0, 20, 0},
    {7, 0, 0, -7, 0, 0, 15, 0, 0, 20, 0, 0},
    {0, -7, 0, 0, 7, 0, 0, -15, 0, 0, -20, 0},
};
static const int16_t fall_b[4][6] = {
    {0, 0, 0, 0, 0, 0},     /* record 1: random, see below */
    {0, 7, 0, 0, -7, 0},
    {7, 0, 0, -7, 0, 0},
    {0, -7, 0, 0, 7, 0},
};
static const uint8_t fall_off[12] = {0x7C, 0x7E, 0x80, 0x84, 0x86, 0x88, 0x8C, 0x8E, 0x90, 0x94, 0x96, 0x98};

void gp_landmark_a_break(GpLmEnv* env, int32_t r) {
    uint8_t* rec = break_parts(env, TABLE_A, r, 4, 0x28);
    if (!rec)
        return;
    w_s16(rec, 0x16, (uint32_t)((tm2_rand() >> 9) + 0x3C));
    if ((uint32_t)(r - 1) <= 3)
        for (int j = 0; j < 12; j++)
            w_s16(rec, fall_off[j], (uint16_t)fall_a[r - 1][j]);
    env->section_disable(env->user, w_i16(rec, 0x0C));
}

void gp_landmark_b_break(GpLmEnv* env, int32_t r) {
    uint8_t* rec = break_parts(env, TABLE_B, r, 2, 0x14);
    if (!rec)
        return;
    if (r == 1) {
        int32_t s = (tm2_rand() & 1) ? 7 : -7;
        w_s16(rec, 0x84, (uint32_t)-s);
        w_s16(rec, 0x7C, (uint32_t)s);
        w_s16(rec, 0x80, 0);
        w_s16(rec, 0x86, 0);
        w_s16(rec, 0x88, 0);
        w_s16(rec, 0x7E, 0);
    } else if ((uint32_t)(r - 1) <= 3) {
        for (int j = 0; j < 6; j++)
            w_s16(rec, fall_off[j], (uint16_t)fall_b[r - 1][j]);
    }
    env->section_disable(env->user, w_i16(rec, 0x0C));
}

#define FX_EXPLOSION12 0x004508f4u
#define FX_EXPLOSION16 0x00450984u
#define FX_EXPLOSION24 0x00450a10u

static void small_blast(GpLmEnv* env, uint32_t pos, int32_t r) {
    uint32_t p = env->jitter(env->user, pos, r, r, r);
    env->fx(env->user, (tm2_rand() & 1) ? FX_EXPLOSION12 : FX_EXPLOSION16, p, 0);
}

void gp_burn_update(GpLmEnv* env) {
    uint8_t* b = G(env, BURN);
    if (b[0] == 0)
        return;
    w_s16(b, 0x06, (uint32_t)(w_u16(b, 0x06) + 1));
    int8_t c = (int8_t)b[0x03];
    b[0x03] = (uint8_t)(c - 1);
    if (c < 5)
        b[0x03] = 5;
    if (tm2_rand() % (int8_t)b[0x03] == 0) {
        small_blast(env, BURN + 0x10, 0x52);
        if (w_i16(b, 0x08) > w_i16(b, 0x06))
            for (uint32_t k = 1; k <= 4; k++)
                small_blast(env, 0x004f082cu + k * 0x10 + 4, 0x80);
    }
    if (w_i16(b, 0x08) == w_i16(b, 0x06)) {
        uint32_t obj = w_u32(b, 0x0C);
        env->place(env->user, obj, BURN + 0x10, BURN + 0x30, BURN + 0x1C, BURN + 0x24, w_i16(b, 0x0A), 0x14, 0xC8, 0);
        wmem_ptr(env->mem, w_u32(b, 0x0C) + 0x12, 1)[0] = 0;
        env->sound(env->user, 0xBBC, 0, 1, 0xC8);
        env->flash(env->user, 3, 0xFF, 0xC8, 0xC8);
        for (int32_t k = 1; k <= 4; k++) {
            env->sound(env->user, 0xBBB, k, 1, 0x80);
            uint32_t p = env->jitter(env->user, 0x004f082cu + (uint32_t)k * 0x10 + 4, 0x80, 0x80, 0x80);
            env->fx(env->user, FX_EXPLOSION24, p, 0);
        }
    }
    if (w_i16(b, 0x08) <= w_i16(b, 0x06))
        w_s32(b, 0x18, (uint32_t)w_i32(b, 0x18) + (uint32_t)w_i32(b, 0x2C));
    int32_t s = w_i16(b, 0x08) + w_i16(b, 0x0A);
    int32_t f = w_i16(b, 0x06);
    if (f >= s && f < s + 10) {
        if (tm2_rand() % 3 == 0) {
            uint32_t p = env->jitter(env->user, BURN + 0x10, 0x52, 0x52, 0x52);
            env->fx(env->user, FX_EXPLOSION24, p, 0);
        }
        b[0x05] = 1;
    }
    if (b[0x05] != 0 && b[0x04] == 0) {
        uint32_t p = env->jitter(env->user, BURN + 0x10, 0x147, 0x147, 0x37);
        uint8_t* q = wmem_ptr(env->mem, p, 12);
        w_s32(q, 8, (uint32_t)w_i32(q, 8) - 0x190);
        env->fx(env->user, FX_EXPLOSION24, p, 0);
        env->flash(env->user, 3, 0xFF, 0xC8, 0xC8);
        env->sound(env->user, 0xBBD, 0, 1, 0xC8);
        b[0x04] = 1;
        b[0x00] = 0;
    }
    if (b[0x05] != 0) {
        if ((int8_t)b[0x02] == 0)
            b[0x02] = (uint8_t)(b[0x06] + 0x14);
        if (w_i16(b, 0x06) >= (int8_t)b[0x02])
            b[0x00] = 0;
    }
}

void gp_landmark_b_update(GpLmEnv* env) {
    for (int32_t r = 1; r < 3; r++) {
        uint32_t ra = TABLE_B + (uint32_t)r * REC;
        uint8_t* rec = G(env, ra);
        if (rec[1] == 0)
            continue;
        w_s16(rec, 0x0A, (uint32_t)(w_u16(rec, 0x0A) + 1));
        int8_t c = (int8_t)rec[3];
        rec[3] = (uint8_t)(c - 1);
        if (c < 5)
            rec[3] = 5;
        for (uint32_t k = 0; k < 2; k++) {
            uint32_t pos = ra + 0x44 + 0x0C * k;
            if (tm2_rand() % (int8_t)rec[3] == 0 && rec[6 + k] == 0) {
                uint32_t p = env->jitter(env->user, pos, 0x51, 0x51, 0x147);
                uint8_t* q = wmem_ptr(env->mem, p, 12);
                w_s32(q, 8, w_u32(q, 8) + 0xA0);
                env->fx(env->user, (tm2_rand() & 1) ? FX_EXPLOSION12 : FX_EXPLOSION16, p, 0);
            }
            if (w_i16(rec, 0x10 + 2 * k) == w_i16(rec, 0x0A)) {
                env->place(env->user, w_u32(rec, 0x28 + 4 * k), pos, ra + 0xA4 + 8 * k, ra + 0x7C + 8 * k, 0,
                           w_i16(rec, 0x1A + 2 * k), 0x14, 0xC8, 1);
                wmem_ptr(env->mem, w_u32(rec, 0x28 + 4 * k) + 0x12, 1)[0] = 0;
            }
            int32_t s0 = w_i16(rec, 0x1A + 2 * k) + w_i16(rec, 0x10 + 2 * k);
            int32_t f = w_i16(rec, 0x0A);
            if (f >= s0 && f < s0 + 10) {
                if (tm2_rand() % 3 == 0) {
                    uint32_t p = env->jitter(env->user, pos, 0x51, 0x51, 0x147);
                    env->fx(env->user, FX_EXPLOSION24, p, 0);
                }
                if (env->split(env->user) == 0)
                    env->locate(env->user, pos);
                else
                    env->locate_split(env->user, pos, -1);
                for (int i = 0; i < 2; i++) {
                    int32_t vol = env->volume(env->user);
                    uint32_t pan = env->pan(env->user);
                    env->play(env->user, i ? 0x13 : 4, pan >> 1, vol, 0x63);
                }
                rec[6 + k] = 1;
            }
        }
        if (rec[6] != 0 && rec[7] != 0) {
            for (uint32_t k = 0; k < 2; k++) {
                uint32_t p = env->jitter(env->user, ra + 0x44 + 0x0C * k, 0x147, 0x147, 0x37);
                uint8_t* q = wmem_ptr(env->mem, p, 12);
                w_s32(q, 8, w_u32(q, 8) - 0x190);
                env->fx(env->user, FX_EXPLOSION24, p, 0);
            }
            env->flash(env->user, 3, 0xFF, 0xC8, 0xC8);
            env->sound(env->user, 0xFA5, r, 1, 0xC8);
            rec[4] = 1;
        }
        if (rec[6] != 0 && rec[7] != 0) {
            if ((int8_t)rec[2] == 0)
                rec[2] = (uint8_t)(rec[0x0A] + 0x14);
            if (w_i16(rec, 0x0A) >= (int8_t)rec[2])
                rec[1] = 0;
        }
    }
}

static void crash_sounds(GpLmEnv* env, uint32_t pos) {
    if (env->split(env->user) == 0)
        env->locate(env->user, pos);
    else
        env->locate_split(env->user, pos, -1);
    for (int i = 0; i < 2; i++) {
        int32_t vol = env->volume(env->user);
        uint32_t pan = env->pan(env->user);
        env->play(env->user, i ? 0x13 : 4, pan >> 1, vol, 0x63);
    }
}

static void small_fx(GpLmEnv* env, uint32_t pos, int32_t a, int32_t b, int32_t c) {
    uint32_t p = env->jitter(env->user, pos, a, b, c);
    env->fx(env->user, (tm2_rand() & 1) ? FX_EXPLOSION12 : FX_EXPLOSION16, p, 0);
}

void gp_landmark_a_update(GpLmEnv* env) {
    for (int32_t r = 1; r < 5; r++) {
        uint32_t ra = TABLE_A + (uint32_t)r * REC;
        uint8_t* rec = G(env, ra);
        if (rec[1] == 0)
            continue;
        w_s16(rec, 0x0A, (uint32_t)(w_u16(rec, 0x0A) + 1));
        int8_t c = (int8_t)rec[3];
        rec[3] = (uint8_t)(c - 1);
        if (c < 5)
            rec[3] = 5;
        for (uint32_t k = 0; k < 3; k++) {
            uint32_t pos = ra + 0x44 + 0x0C * k;
            if (tm2_rand() % (int8_t)rec[3] == 0 && rec[6 + k] == 0)
                small_fx(env, pos, 0x37, 0x37, 0x147);
            if (w_i16(rec, 0x10 + 2 * k) == w_i16(rec, 0x0A)) {
                env->place(env->user, w_u32(rec, 0x28 + 4 * k), pos, ra + 0xA4 + 8 * k, ra + 0x7C + 8 * k, 0,
                           w_i16(rec, 0x1A + 2 * k), 0x14, 0xC8, 1);
                wmem_ptr(env->mem, w_u32(rec, 0x28 + 4 * k) + 0x12, 1)[0] = 0;
            }
            int32_t s0 = w_i16(rec, 0x1A + 2 * k) + w_i16(rec, 0x10 + 2 * k);
            int32_t f = w_i16(rec, 0x0A);
            if (f >= s0 && f < s0 + 10) {
                if (tm2_rand() % 3 == 0) {
                    uint32_t p = env->jitter(env->user, pos, 0x37, 0x37, 0x147);
                    env->fx(env->user, FX_EXPLOSION24, p, 0);
                }
                crash_sounds(env, pos);
                rec[6 + k] = 1;
            }
        }
        if (rec[6] != 0 && rec[7] != 0 && rec[8] != 0 && rec[4] == 0) {
            for (uint32_t k = 0; k < 2; k++) {
                uint32_t p = env->jitter(env->user, ra + 0x44 + 0x0C * k, 0x147, 0x147, 0x37);
                uint8_t* q = wmem_ptr(env->mem, p, 12);
                w_s32(q, 8, w_u32(q, 8) - 0x190);
                env->fx(env->user, FX_EXPLOSION24, p, 0);
            }
            uint32_t p = env->jitter(env->user, ra + 0x68, 0x147, 0x147, 0x37);
            env->fx(env->user, FX_EXPLOSION24, p, 0);
            env->flash(env->user, 3, 0xFF, 0xC8, 0xC8);
            env->sound(env->user, 0xFA0, r, 1, 0xC8);
            rec[4] = 1;
        }
        /* the top (part 4 at +0x68) */
        if (tm2_rand() % (int8_t)rec[3] == 0 && rec[9] == 0)
            small_fx(env, ra + 0x68, 0x147, 0x147, 0x37);
        int16_t f16 = w_i16(rec, 0x0A);
        if (f16 >= w_i16(rec, 0x16) && f16 >= w_i16(rec, 0x12) && f16 >= w_i16(rec, 0x14)) {
            env->place(env->user, w_u32(rec, 0x34), ra + 0x68, ra + 0xBC, ra + 0x94, 0, w_i16(rec, 0x20), 0x14, 0xC8, 0);
            wmem_ptr(env->mem, w_u32(rec, 0x34) + 0x12, 1)[0] = 0;
        }
        int32_t s0 = w_i16(rec, 0x20) + w_i16(rec, 0x16);
        int32_t f = w_i16(rec, 0x0A);
        if (f >= s0 && f < s0 + 10) {
            if (tm2_rand() % 3 == 0) {
                uint32_t p = env->jitter(env->user, ra + 0x68, 0x147, 0x147, 0x37);
                env->fx(env->user, FX_EXPLOSION24, p, 0);
            }
            rec[9] = 1;
            crash_sounds(env, ra + 0x68);
        }
        if (rec[6] != 0 && rec[7] != 0 && rec[8] != 0 && rec[9] != 0) {
            if ((int8_t)rec[2] == 0)
                rec[2] = (uint8_t)(rec[0x0A] + 0x14);
            if (w_i16(rec, 0x0A) >= (int8_t)rec[2])
                rec[1] = 0;
        }
    }
}

#define MOUNT 0x004f08dcu
#define FX_SMOKE_A 0x00450aa0u
#define FX_SMOKE_B 0x0045109cu
#define PT 0x004f003cu

/* a jittered point of the object (matrix at +4) plus the mount offset, into 0x004f003c */
static void mount_point(GpLmEnv* env) {
    uint8_t* m = G(env, MOUNT);
    uint32_t p = env->jitter(env->user, 0x004e66d4u, 0x51, 0x51, 0x0F);
    const uint8_t* q = wmem_ptr(env->mem, p, 12);
    const uint8_t* o = wmem_ptr(env->mem, w_u32(m, 0x10), 0x34);
    int32_t mat[9], v[3] = {w_i32(q, 0), w_i32(q, 4), w_i32(q, 8)}, out[3];
    for (int i = 0; i < 9; i++)
        mat[i] = w_i32(o, 4 + 4 * i);
    tm2_mat_vec_mul_row(mat, v, out);
    uint8_t* pt = G(env, PT);
    for (int i = 0; i < 3; i++)
        w_s32(pt, 4 * i, (uint32_t)out[i] + w_u32(m, 0x20 + 4 * i));
}

void gp_mount_update(GpLmEnv* env) {
    uint8_t* m = G(env, MOUNT);
    if (m[4] == 0)
        return;
    w_s16(m, 0x0C, (uint32_t)(w_u16(m, 0x0C) + 1));
    m[5] = (uint8_t)(m[5] - 1);
    if ((int8_t)m[5] < 2)
        m[5] = 2;
    if (w_i16(m, 0x0E) < w_i16(m, 0x0C)) {
        int32_t start = w_i16(m, 0x0E), frame = w_i16(m, 0x0C);
        uint8_t* o = wmem_ptr(env->mem, w_u32(m, 0x10), 0x34);
        if (frame < start + 0x0C) {
            int32_t vz = w_i16(m, 0x0A);
            w_s32(o, 0x2C, w_u32(o, 0x2C) + 5);
            w_s32(o, 0x30, w_u32(o, 0x30) + (uint32_t)vz);
            w_s16(m, 0x0A, (uint32_t)(w_u16(m, 0x0A) - 0x0F));
        } else if (frame < start + 0x10) {
            w_s32(o, 0x30, w_u32(o, 0x30) + (uint32_t)w_i16(m, 0x0A));
            w_s16(m, 0x0A, (uint32_t)(w_u16(m, 0x0A) + 4));
        }
        w_s16(m, 0x30, (uint32_t)tm2_angle_interp(w_i16(m, 0x30), 0x5B, 5));
        uint32_t ax = (uint32_t)(w_u16(m, 0x2C) + w_u16(m, 0x08));
        if (w_i16(m, 0x0C) % 5 == 0) {
            w_s16(m, 0x2C, ax);
            w_s16(m, 0x08, (uint32_t)(w_u16(m, 0x08) + 1));
        } else {
            w_s16(m, 0x2C, ax);
        }
        if (w_i16(m, 0x2C) > 0x444) {
            m[4] = 0;
            w_s16(m, 0x2C, 0x444);
            m[6] = 1;
            env->topple_a(env->user);
            env->topple_b(env->user);
            for (int i = 0; i < 8; i++) {
                mount_point(env);
                env->fx(env->user, FX_SMOKE_A, PT, 0);
                env->fx(env->user, FX_SMOKE_B, PT, 0);
            }
            for (uint32_t a = 0x004e6674u; a != 0x004e66b0u; a += 0x0C) {
                env->fx(env->user, FX_SMOKE_A, a, 0);
                env->fx(env->user, FX_SMOKE_B, a, 0);
            }
            env->flash(env->user, 3, 0xFF, 0xC8, 0xC8);
            env->sound(env->user, 0xC80, 0, 1, 0x63);
            env->play(env->user, 0x3E, 0, -0x280, 0x63);
            env->play(env->user, 2, 0, -0x280, 0x63);
            env->play(env->user, 0x13, 0, 0x280, 0x63);
            env->play(env->user, 0x1E, 0, 0x280, 0x63);
        }
        int16_t a[3] = {w_i16(m, 0x2C), w_i16(m, 0x2E), w_i16(m, 0x30)};
        int32_t mat[9];
        tm2_build_matrix(a, mat);
        uint8_t* ow = wmem_ptr(env->mem, w_u32(m, 0x10) + 4, 36);
        for (int i = 0; i < 9; i++)
            w_s32(ow, 4 * i, (uint32_t)mat[i]);
    }
    if (tm2_rand() % (int8_t)m[5] == 0 && m[6] == 0) {
        for (int i = 0; i < 2; i++) {
            uint32_t p = env->jitter(env->user, 0x004e66b0u, 0x29, 0x51, 0x51);
            w_s32(G(env, 0x004f0048u), 0, p);
            env->fx(env->user, FX_SMOKE_A, p, 0);
            env->fx(env->user, FX_SMOKE_B, w_u32(G(env, 0x004f0048u), 0), 0);
        }
        mount_point(env);
        env->fx(env->user, (tm2_rand() & 1) ? FX_EXPLOSION16 : FX_EXPLOSION24, PT, 0);
        env->fx(env->user, FX_SMOKE_B, PT, 0);
        if (tm2_rand() & 1) {
            mount_point(env);
            env->fx(env->user, FX_SMOKE_A, PT, 0);
            env->fx(env->user, FX_SMOKE_B, PT, 0);
        }
    }
    if (w_i16(m, 0x0E) == w_i16(m, 0x0C)) {
        uint8_t* o = wmem_ptr(env->mem, w_u32(m, 0x10), 0x34);
        w_s32(o, 0x30, w_u32(o, 0x30) + 200);
        for (uint32_t a = 0x004e6644u; a != 0x004e6674u; a += 0x0C) {
            env->fx(env->user, FX_SMOKE_A, a, 0);
            env->fx(env->user, FX_SMOKE_B, a, 0);
        }
        for (uint32_t i = 0; i < 3; i++) {
            int32_t r = tm2_rand() / 1024;
            env->place(env->user, w_u32(m, 0x14 + 4 * i), MOUNT + 0x20, 0x004e66bcu + 8 * i, 0x004f0034u,
                       0x004f0010u + 0x0C * i, r, 0x4B, 0xC8, 0);
        }
    }
}
