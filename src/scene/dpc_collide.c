/*
 * dpc_collide.c: point against level collision surfaces, ported from TM2.EXE (see dpc_collide.h).
 */
#include "dpc_collide.h"

#include "../math/tm2math.h"

typedef struct { uint32_t node; int32_t pos[3]; int32_t a, b, shift; } Frame;

static uint8_t* at(const DpcCollideCtx* c, uint32_t p, size_t need) {
    uint32_t o = p - c->addr_base;
    if ((size_t)o > c->size || c->size - o < need) return 0;
    return c->buf + o;
}
static int32_t rd32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static uint16_t rd16(const uint8_t* b, int o) { return (uint16_t)(b[o] | b[o + 1] << 8); }
static void rd_mat(const uint8_t* b, int o, int32_t m[9]) { for (int i = 0; i < 9; i++) m[i] = rd32(b, o + 4 * i); }

static int planes_pos(const uint8_t* v, int mo, int oo, const int32_t p[3], int32_t r[3]) {
    int32_t m[9];
    rd_mat(v, mo, m);
    tm2_mat_vec_mul_col(m, p, r);
    (void)oo;
    return 0;
}

/* 0x0048dc3c */
int32_t dpc_volume_test(const uint8_t* n, const int32_t p[3], DpcCollision* r, int flag) {
    int32_t q[3];
    planes_pos(n, 0x10, 0x34, p, q);
    if (tm2_wadd(rd32(n, 0x34), q[0]) > 0 && tm2_wadd(rd32(n, 0x38), q[1]) > 0 && tm2_wadd(rd32(n, 0x3c), q[2]) > 0) {
        int in = 1;
        if (3 < n[4]) {
            planes_pos(n, 0x40, 0x64, p, q);
            if (tm2_wadd(rd32(n, 0x64), q[0]) < 1) in = 0;
            else if (4 < n[4] && (tm2_wadd(rd32(n, 0x68), q[1]) < 1 || (5 < n[4] && tm2_wadd(rd32(n, 0x6c), q[2]) < 1))) in = 0;
        }
        if (in) r->hit = 1;
    }
    if (r->hit == 1 && flag) {
        r->stop = -2;
        r->hit = 0;
        return r->stop;
    }
    return r->hit == 1 ? 1 : 0;
}

/* 0x0048d010 */
int dpc_group_sphere(const int32_t c[3], int32_t rad, const int32_t p[3], int32_t slack) {
    int32_t dx = tm2_wadd(c[0], -p[0]), dy = tm2_wadd(c[1], -p[1]), dz = tm2_wadd(c[2], -p[2]);
    int32_t d = tm2_wadd(tm2_wadd(tm2_wmul(dx, dx), tm2_wmul(dy, dy)), tm2_wmul(dz, dz));
    return !(rad < 0xb505 && (d < 0 || tm2_wadd(tm2_wmul(rad, rad), slack) <= d));
}

/* type 11 volume test with the point as it is (the scene walk negates it) */
static int inside(const DpcCollideCtx* c, uint32_t vp, const int32_t p[3]) {
    const uint8_t* v = at(c, vp, 0x64);
    if (!v) return 0;
    int32_t q[3];
    planes_pos(v, 4, 0x28, p, q);
    if (!(tm2_wadd(rd32(v, 0x28), q[0]) > 0 && tm2_wadd(rd32(v, 0x2c), q[1]) > 0 && tm2_wadd(rd32(v, 0x30), q[2]) > 0)) return 0;
    if (v[0] < 4) return 1;
    planes_pos(v, 0x34, 0x58, p, q);
    if (!(tm2_wadd(rd32(v, 0x58), q[0]) > 0)) return 0;
    if (v[0] < 5) return 1;
    if (!(tm2_wadd(rd32(v, 0x5c), q[1]) > 0)) return 0;
    if (v[0] < 6) return 1;
    return tm2_wadd(rd32(v, 0x60), q[2]) > 0;
}

void dpc_trigger(const DpcCollideCtx* c, uint32_t np, uint32_t mode) {
    for (;;) {
        uint8_t* n = at(c, np, 0x20);
        if (!n) return;
        if (n[0] != 3) {
            if (n[0] != 9) return;
            if (mode == 0) n[7] = 0;
            if (mode == 2) n[7] = (uint8_t)(n[6] - 1);
            if (mode == 1) { n[7] = (uint8_t)(n[7] + 1); if (n[6] <= n[7]) n[7] = 0; }
            if (rd32(n, 0xc) == 0 || n[9] != n[7]) return;
            mode = n[8];
            np = (uint32_t)rd32(n, 0xc);
            continue;
        }
        if (mode == 0) { n[0xc] = 0; n[0xe] = 0; n[0xf] = 0; }
        else if (mode == 1) {
            uint16_t v = (uint16_t)(rd16(n, 6) + 1);
            n[0xe] = (uint8_t)v; n[0xf] = (uint8_t)(v >> 8);
            if (n[0xa] <= 3 && c->anim) c->anim(c->user, np, n[0xa]);
        } else if (mode == 2) { n[0xe] = 0; n[0xf] = 0; n[0xc] = (uint8_t)(n[0xb] - 1); }
        if (mode == 1 || rd32(n, 0x1c) == 0 || n[5] != n[0xc]) return;
        mode = n[0x13];
        np = (uint32_t)rd32(n, 0x1c);
    }
}

#define STACK 200

void dpc_collide(const DpcCollideCtx* c, uint32_t root, const int32_t p[3], int32_t shift, DpcCollision* r) {
    static Frame st[STACK + 1];
    int32_t depth = 1;
    st[0] = (Frame){ root, { p[0], p[1], p[2] }, 0, 0, shift };
#define PUSH(nd, A, B, S) do { Frame* f_ = &st[depth]; f_->node = (nd); f_->pos[0] = e->pos[0]; f_->pos[1] = e->pos[1]; \
        f_->pos[2] = e->pos[2]; f_->a = (A); f_->b = (B); f_->shift = (S); if (depth < STACK) depth++; } while (0)
    while (r->stop == 0) {
        depth--;
        if (depth < 0) return;
        Frame cur = st[depth];
        Frame* e = &cur;
        uint8_t* n = at(c, e->node, 0x14);
        if (!n) continue;
        switch (n[0]) {
        case 1: {
            if (n[0x15] & 1) break;
            int32_t ctr[3] = {rd32(n, 4), rd32(n, 8), rd32(n, 0xc)};
            if (!dpc_group_sphere(ctr, rd32(n, 0x10), e->pos, c->slack)) break;
            int32_t A = rd16(n, 2) ? rd16(n, 2) : e->a, B = rd16(n, 2) ? rd16(n, 0x16) : e->b;
            for (int i = 0; i < n[0x14]; i++) PUSH((uint32_t)rd32(n, 0x18 + 4 * i), A, B, e->shift);
            break;
        }
        case 2: {
            int sh = e->shift & 31;
            int32_t dx = (rd32(n, 4) - e->pos[0]) >> sh, dy = (rd32(n, 8) - e->pos[1]) >> sh, dz = (rd32(n, 0xc) - e->pos[2]) >> sh;
            uint32_t d = (uint32_t)dx * (uint32_t)dx + (uint32_t)dy * (uint32_t)dy + (uint32_t)dz * (uint32_t)dz;
            for (int32_t i = 0; i < rd32(n, 0x10); i++) {
                const uint8_t* ent = at(c, (uint32_t)rd32(n, 0x14 + 4 * i), 12);
                if (!ent) break;
                if ((uint32_t)rd32(ent, 4) <= d && d < (uint32_t)rd32(ent, 0)) {
                    PUSH((uint32_t)rd32(ent, 8), e->a, e->b, e->shift);
                    break;
                }
            }
            break;
        }
        case 3: {
            int32_t A = rd16(n, 2) ? rd16(n, 2) : e->a, B = rd16(n, 2) ? n[4] : e->b;
            PUSH((uint32_t)rd32(n, 0x20 + 4 * n[0xc]), A, B, e->shift);
            break;
        }
        case 4: {
            if (n[0x13] & 1) break;
            for (int k = 0; k < 3; k++) e->pos[k] = tm2_wadd(e->pos[k], -rd32(n, 4 + 4 * k));
            int32_t A = rd16(n, 2) ? rd16(n, 2) : e->a, B = rd16(n, 2) ? rd16(n, 0x10) : e->b;
            for (int i = 0; i < n[0x12]; i++) PUSH((uint32_t)rd32(n, 0x14 + 4 * i), A, B, e->shift);
            break;
        }
        case 5: {
            if (!at(c, e->node, 0x38) || (n[0x37] & 1)) break;
            int32_t t[3], m[9];
            for (int k = 0; k < 3; k++) t[k] = tm2_wadd(e->pos[k], -rd32(n, 0x28 + 4 * k));
            rd_mat(n, 4, m);
            tm2_mat_vec_mul_col(m, t, e->pos);
            int32_t A = rd16(n, 2) ? rd16(n, 2) : e->a, B = rd16(n, 2) ? rd16(n, 0x34) : e->b;
            for (int i = 0; i < n[0x36]; i++) PUSH((uint32_t)rd32(n, 0x38 + 4 * i), A, B, e->shift);
            break;
        }
        case 8: {
            if (!at(c, e->node, 0x70) || n[7] == 1) break;
            r->stop = dpc_volume_test(n, e->pos, r, n[5] == 0xfe);
            if (r->hit) {
                if (rd16(n, 2) == 0) { r->id_a = (int16_t)e->a; r->id_b = (int16_t)e->b; }
                else { r->id_b = n[6]; r->id_a = (int16_t)rd16(n, 2); }
                if (rd32(n, 0xc)) dpc_trigger(c, (uint32_t)rd32(n, 0xc), n[0xb]);
            }
            break;
        }
        case 9: {
            int32_t A = rd16(n, 2) ? rd16(n, 2) : e->a, B = rd16(n, 2) ? rd16(n, 4) : e->b;
            PUSH((uint32_t)rd32(n, 0x10 + 4 * n[7]), A, B, e->shift);
            break;
        }
        case 0xb: {
            if (n[4] & 1) break;
            int32_t A = rd16(n, 2) ? rd16(n, 2) : e->a, B = rd16(n, 2) ? n[5] : e->b;
            int out = 1;
            for (int i = 0; out && i < n[6]; i++)
                if (inside(c, (uint32_t)rd32(n, 0x10 + 4 * i), e->pos)) out = 0;
            uint32_t child = (uint32_t)rd32(n, out ? 8 : 0xc);
            if (child) PUSH(child, A, B, e->shift);
            break;
        }
        case 0xc: {
            int32_t ns = rd16(n, 4), d = e->shift - ns;
            if (d < 0) { int s = (int)((uint32_t)(0 - d) & 31); for (int k = 0; k < 3; k++) e->pos[k] = (int32_t)((uint32_t)e->pos[k] << s); }
            else if (d > 0) for (int k = 0; k < 3; k++) e->pos[k] >>= (d & 31);
            for (int i = 0; i < rd16(n, 6); i++) PUSH((uint32_t)rd32(n, 8 + 4 * i), e->a, e->b, ns);
            break;
        }
        case 0xf:
            /* 0x0048e25c (height field test via 0x004bcb70): no type 15 nodes on the disc, not ported */
            break;
        default:
            break;
        }
    }
#undef PUSH
}
