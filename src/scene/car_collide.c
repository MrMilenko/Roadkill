/*
 * car_collide.c: entity collision points against the level and other entities, ported from
 * TM2.EXE (see car_collide.h).
 */
#include "car_collide.h"

#include <limits.h>

#include "../math/tm2math.h"

static uint8_t* at(const DpcCollideCtx* c, uint32_t p, size_t need) {
    uint32_t o = p - c->addr_base;
    if ((size_t)o > c->size || c->size - o < need) return 0;
    return c->buf + o;
}
static int32_t rd32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static int16_t rd16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static void wr32(uint8_t* b, int o, int32_t v) { for (int i = 0; i < 4; i++) b[o + i] = (uint8_t)((uint32_t)v >> (8 * i)); }
static void wr16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)((uint32_t)v >> 8); }
static uint32_t ru32(const DpcCollideCtx* c, uint32_t a) { const uint8_t* p = at(c, a, 4); return p ? (uint32_t)rd32(p, 0) : 0; }

static int mat_at(const DpcCollideCtx* c, uint32_t a, int32_t m[9]) {
    const uint8_t* p = at(c, a, 36);
    for (int i = 0; i < 9; i++) m[i] = p ? rd32(p, 4 * i) : 0;
    return p != 0;
}
static void col(const DpcCollideCtx* c, uint32_t ma, const int32_t v[3], int32_t o[3]) {
    int32_t m[9];
    mat_at(c, ma, m);
    tm2_mat_vec_mul_col(m, v, o);
}
static void row(const DpcCollideCtx* c, uint32_t ma, const int32_t v[3], int32_t o[3]) {
    int32_t m[9];
    mat_at(c, ma, m);
    tm2_mat_vec_mul_row(m, v, o);
}
/* math_mat_vec_mul_packed 0x0048cd3c: M^T * (i16 x, y, z) */
static void packed(const DpcCollideCtx* c, uint32_t ma, uint32_t va, int32_t o[3]) {
    const uint8_t* p = at(c, va, 6);
    int32_t v[3] = { p ? rd16(p, 0) : 0, p ? rd16(p, 2) : 0, p ? rd16(p, 4) : 0 };
    row(c, ma, v, o);
}
static void vec_at(const DpcCollideCtx* c, uint32_t a, int32_t v[3]) {
    const uint8_t* p = at(c, a, 12);
    for (int i = 0; i < 3; i++) v[i] = p ? rd32(p, 4 * i) : 0;
}
/* shift a value from frame scale to world scale: d = frame shift - world shift */
static int32_t to_frame(int32_t v, int32_t d) {
    if (d < 0) return v >> ((uint32_t)(0 - d) & 31);
    if (d > 0) return (int32_t)((uint32_t)v << (d & 31));
    return v;
}
static int32_t depth_scale(int32_t v, int32_t d) {
    if (d < 0) return (int32_t)((uint32_t)v << ((uint32_t)(0 - d) & 31));
    if (d > 0) return v >> (d & 31);
    return v;
}

/* the bit by bit integer square root the original inlines */
static uint32_t isqrt(int32_t n) {
    uint32_t r = 0, b = 0x40000000;
    int32_t v = n;
    do {
        while ((int32_t)b <= v && (int32_t)r <= (int32_t)((uint32_t)v - b)) {
            v = (int32_t)((uint32_t)v - b - r);
            r = r >> 1 | b;
            b >>= 2;
            if (b == 0) return r;
        }
        r >>= 1;
        b >>= 2;
    } while (b != 0);
    return r;
}

void collide_normalize(const int32_t v[3], int32_t out[3]) {
    int32_t n = tm2_wadd(tm2_wadd(tm2_wmul(v[1], v[1]), tm2_wmul(v[0], v[0])), tm2_wmul(v[2], v[2]));
    uint32_t s = isqrt(n);
    if (s == 0) s = 1;
    int32_t k = (int32_t)(0xfff0000ull / (uint64_t)(int64_t)(int32_t)s);
    for (int i = 0; i < 3; i++) out[i] = tm2_wmul(v[i], k) >> 16;
}

int collide_grid_reject(const CollideGrid* g, int32_t x, int32_t y, int32_t z) {
    if (!g || !g->bits) return 0;
    if (g->x <= x && x <= g->x + g->sx && g->y <= y && y <= g->y + g->sy && g->z <= z && z <= g->z + g->sz) {
        int32_t i = (x - g->x) / g->cell + g->row * ((y - g->y) / g->cell) + ((z - g->z) / g->cell) * g->layer;
        return (g->bits[i >> 5] & (1u << (i & 31))) == 0;
    }
    return 1;
}

typedef struct { uint32_t node; int32_t pos[3]; int32_t depth, a, b, shift; } WFrame;

#define STACK 200

/* plane values of point p against a volume's first three (+0x10, +0x34) or last three planes */
static void planes(const DpcCollideCtx* c, uint32_t node, int second, const int32_t p[3], int32_t out[3]) {
    const uint8_t* n = at(c, node, 0x80);
    int32_t q[3];
    col(c, node + (second ? 0x40 : 0x10), p, q);
    for (int i = 0; i < 3; i++) out[i] = tm2_wadd(rd32(n, (second ? 0x64 : 0x34) + 4 * i), q[i]);
}

/* 0x0049a1b4 */
static void walk(const DpcCollideCtx* c, CarCollideState* s, uint32_t holder, const int32_t pos[3],
                 uint32_t records, int32_t r2) {
    static WFrame st[STACK + 1];
    int32_t sp = 0;
    int32_t sv[6] = { 0 }, pv[6] = { 0 };     /* the original's frame slots, kept across nodes */
    int32_t last_depth = s->depth;
    uint32_t e = s->entity;
    const uint8_t* E = at(c, e, 0x170);
    int32_t count = E ? rd32(E, 0xf4) : 0;
    uint32_t mask = E ? (uint32_t)rd32(E, 0x16c) : 0;
#define PUSH(nd, D, A, B, S) do { WFrame* f_ = &st[sp]; f_->node = (nd); f_->pos[0] = f->pos[0]; f_->pos[1] = f->pos[1]; \
        f_->pos[2] = f->pos[2]; f_->depth = (D); f_->a = (A); f_->b = (B); f_->shift = (S); if (sp < STACK) sp++; } while (0)
    {
        WFrame root = { ru32(c, holder + 0x15c), { pos[0], pos[1], pos[2] }, s->depth, 0, 0, s->shift };
        WFrame* f = &root;
        PUSH(root.node, root.depth, 0, 0, root.shift);
    }
    for (;;) {
        if (s->remaining < 1) break;
        sp--;
        if (sp < 0) break;
        WFrame cur = st[sp];
        WFrame* f = &cur;
        uint8_t* n = at(c, f->node, 0x14);
        if (!n) { last_depth = f->depth; continue; }
        switch (n[0]) {
        case 1: {
            if (n[0x15] & 1) break;
            if (!(n[0x15] & 4)) {
                int32_t rad = rd32(n, 0x10);
                if (rad < 0xb505) {
                    int32_t dx = rd32(n, 4) - f->pos[0], dy = rd32(n, 8) - f->pos[1], dz = rd32(n, 0xc) - f->pos[2];
                    int32_t d = tm2_wadd(tm2_wadd(tm2_wmul(dz, dz), tm2_wmul(dx, dx)), tm2_wmul(dy, dy));
                    if (!(d >= 0 && d < tm2_wadd(tm2_wmul(rad, rad), r2))) break;
                }
            }
            int32_t A = rd16(n, 2) ? (uint16_t)rd16(n, 2) : f->a, B = rd16(n, 2) ? (uint16_t)rd16(n, 0x16) : f->b;
            int k = n[0x14];
            for (int i = 0; i < k; i++) PUSH((uint32_t)rd32(n, 0x18 + 4 * i), f->depth, A, B, f->shift);
            break;
        }
        case 2: {
            int sh = f->shift & 31;
            int32_t dx = (rd32(n, 4) - f->pos[0]) >> sh, dy = (rd32(n, 8) - f->pos[1]) >> sh, dz = (rd32(n, 0xc) - f->pos[2]) >> sh;
            uint32_t d = (uint32_t)dx * (uint32_t)dx + (uint32_t)dy * (uint32_t)dy + (uint32_t)dz * (uint32_t)dz;
            for (int32_t i = 0; i < rd32(n, 0x10); i++) {
                const uint8_t* lod = at(c, (uint32_t)rd32(n, 0x14 + 4 * i), 12);
                if (!lod) break;
                if ((uint32_t)rd32(lod, 4) <= d && d < (uint32_t)rd32(lod, 0)) {
                    PUSH((uint32_t)rd32(lod, 8), f->depth, f->a, f->b, f->shift);
                    break;
                }
            }
            break;
        }
        case 3: {
            int32_t A = rd16(n, 2) ? (uint16_t)rd16(n, 2) : f->a, B = rd16(n, 2) ? n[4] : f->b;
            PUSH((uint32_t)rd32(n, 0x20 + 4 * n[0xc]), f->depth, A, B, f->shift);
            break;
        }
        case 4: {
            if (n[0x13] & 1) break;
            for (int k = 0; k < 3; k++) f->pos[k] = tm2_wadd(f->pos[k], -rd32(n, 4 + 4 * k));
            int32_t A = rd16(n, 2) ? (uint16_t)rd16(n, 2) : f->a, B = rd16(n, 2) ? (uint16_t)rd16(n, 0x10) : f->b;
            for (int i = 0; i < n[0x12]; i++) PUSH((uint32_t)rd32(n, 0x14 + 4 * i), f->depth, A, B, f->shift);
            break;
        }
        case 5: {
            if (!at(c, f->node, 0x38) || (n[0x37] & 1)) break;
            int32_t t[3];
            for (int k = 0; k < 3; k++) t[k] = tm2_wadd(f->pos[k], -rd32(n, 0x28 + 4 * k));
            col(c, f->node + 4, t, f->pos);
            if (f->depth >= 0 && f->depth < CAR_COLLIDE_MAX_DEPTH) s->mats[f->depth] = f->node + 4;
            f->depth++;
            int32_t A = rd16(n, 2) ? (uint16_t)rd16(n, 2) : f->a, B = rd16(n, 2) ? (uint16_t)rd16(n, 0x34) : f->b;
            for (int i = 0; i < n[0x36]; i++) PUSH((uint32_t)rd32(n, 0x38 + 4 * i), f->depth, A, B, f->shift);
            break;
        }
        case 8: {
            n = at(c, f->node, 0x80);
            if (!n || n[7] == 1) break;
            uint32_t nd = f->node;
            int np = n[4];
            if (rd32(n, 2) >> 24 == -2) {
                if (s->sphere_mode == 0) {
                    for (int k = 0; k < count; k++) {
                        uint8_t* rec = at(c, records + 0x20 * k, 0x24);
                        if (!rec || rd32(rec, 4) != 0) continue;
                        int32_t p[3], t[3];
                        packed(c, e + 0xb0, e + 0xf8 + 8 * k, p);
                        for (int i = 0; i < f->depth && i < CAR_COLLIDE_MAX_DEPTH; i++) {
                            t[0] = p[0]; t[1] = p[1]; t[2] = p[2];
                            col(c, s->mats[i], t, p);
                        }
                        for (int i = 0; i < 3; i++) p[i] = tm2_wadd(p[i], f->pos[i]);
                        int32_t v[3];
                        planes(c, nd, 0, p, v);
                        if (v[0] > 0 && v[1] > 0 && v[2] > 0) {
                            int in = 1;
                            if (np >= 4) {
                                planes(c, nd, 1, p, v);
                                in = v[0] > 0 && (np < 5 || (v[1] > 0 && (np < 6 || v[2] > 0)));
                            }
                            if (in) { s->remaining--; wr32(rec, 4, 2); }
                        }
                    }
                } else {
                    int32_t v[3];
                    int32_t R = s->radius;
                    planes(c, nd, 0, f->pos, v);
                    if (tm2_wadd(v[0], -R) > 0 && tm2_wadd(v[1], -R) > 0 && v[2] > 0) {
                        if (np < 4) s->remaining = 0;
                        else {
                            planes(c, nd, 1, f->pos, v);
                            if (tm2_wadd(v[0], -R) > 0) {
                                if (np < 5) s->remaining = 0;
                                else if (tm2_wadd(v[1], -R) > 0) {
                                    if (np < 6 || v[2] > 0) s->remaining = 0;
                                }
                            }
                        }
                    }
                    if (s->remaining == 0) {
                        s->all_out = 1;
                        s->depth = f->depth;
                        return;
                    }
                }
                break;
            }
            int32_t dshift = f->shift - s->shift;
            for (int k = 0; k < count; k++) {
                uint8_t* rec = at(c, records + 0x20 * k, 0x24);
                if (!rec || rd32(rec, 4) != 0) continue;
                int32_t p[3], t[3];
                packed(c, e + 0xb0, e + 0xf8 + 8 * k, p);
                if (k < CAR_COLLIDE_MAX_POINTS) {
                    vec_at(c, e + 0xe8, t);
                    for (int i = 0; i < 3; i++) s->world[k][i] = tm2_wadd(p[i], t[i]);
                }
                for (int i = 0; i < f->depth && i < CAR_COLLIDE_MAX_DEPTH; i++) {
                    t[0] = p[0]; t[1] = p[1]; t[2] = p[2];
                    col(c, s->mats[i], t, p);
                }
                for (int i = 0; i < 3; i++) p[i] = tm2_wadd(to_frame(p[i], dshift), f->pos[i]);
                int in = 0;
                planes(c, nd, 0, p, sv);
                if (sv[0] > 0 && sv[1] > 0 && sv[2] > 0) {
                    in = 1;
                    if (np > 3) {
                        int32_t q[3];
                        col(c, nd + 0x40, p, q);
                        sv[3] = tm2_wadd(rd32(n, 0x64), q[0]);
                        if (sv[3] < 1) in = 0;
                        else if (np > 4) {
                            sv[4] = tm2_wadd(rd32(n, 0x68), q[1]);
                            if (sv[4] < 1) in = 0;
                            else if (np > 5) {
                                sv[5] = tm2_wadd(rd32(n, 0x6c), q[2]);
                                if (sv[5] < 1) in = 0;
                            }
                        }
                    }
                }
                if (!in) continue;
                if (s->first_hit_stops) {
                    s->remaining = 0;
                    wr32(rec, 4, 1);
                    if (rd16(n, 2) == 0) { wr16(rec, 0x22, f->a); wr16(rec, 0x20, f->b); }
                    else { wr16(rec, 0x22, rd16(n, 2)); wr16(rec, 0x20, n[6]); }
                    break;
                }
                int32_t nrm[3] = { 0, 0, 0 }, dep = 0;
                if (n[9] != 0) {
                    int32_t d[3];
                    for (int i = 0; i < 3; i++) d[i] = tm2_wadd(rd32(n, 0x70 + 4 * i), -p[i]);
                    int32_t r = rd32(n, 0x7c);
                    int32_t dd = tm2_wadd(tm2_wadd(tm2_wadd(tm2_wmul(d[2], d[2]), tm2_wmul(d[0], d[0])), tm2_wmul(d[1], d[1])),
                                          -tm2_wmul(r, r));
                    int sh = f->shift & 31;
                    if (dd < 1) dep = dd < 0 ? (int32_t)isqrt(-dd) >> sh : 0;
                    else dep = (int32_t)isqrt(dd) >> sh;
                    if (n[9] == 1 && dd >= 1) {
                        collide_normalize(d, nrm);
                        for (int i = 0; i < 3; i++) nrm[i] = -nrm[i];
                    } else if (n[9] == 2 && dd < 0) {
                        collide_normalize(d, nrm);
                    } else dep = 0;
                    if (dep == 0) break;    /* original: leaves the point loop */
                } else if ((int8_t)n[5] < 0) {
                    int32_t pp[3], wp[3];
                    packed(c, s->prev_mat, e + 0xf8 + 8 * k, pp);
                    vec_at(c, s->prev_pos, wp);
                    for (int i = 0; i < 3; i++) pp[i] = tm2_wadd(pp[i], wp[i]);
                    int kk = k < CAR_COLLIDE_MAX_POINTS ? k : CAR_COLLIDE_MAX_POINTS - 1;
                    for (int i = 0; i < 3; i++) pp[i] = tm2_wadd(pp[i], -s->world[kk][i]);
                    for (int i = 0; i < f->depth && i < CAR_COLLIDE_MAX_DEPTH; i++) {
                        t[0] = pp[0]; t[1] = pp[1]; t[2] = pp[2];
                        col(c, s->mats[i], t, pp);
                    }
                    for (int i = 0; i < 3; i++) pp[i] = tm2_wadd(to_frame(pp[i], dshift), p[i]);
                    planes(c, nd, 0, pp, pv);
                    if (np > 3) planes(c, nd, 1, pp, pv + 3);
                    int best = -1;
                    int32_t mn = INT_MAX;
                    for (int i = 0; i < np; i++)
                        if (pv[i] < 1 && sv[i < 6 ? i : 5] < mn) { mn = sv[i < 6 ? i : 5]; best = i; }
                    if (best == -1) {
                        mn = INT_MAX;
                        for (int i = 0; i < np; i++)
                            if (pv[i < 6 ? i : 5] < mn) { mn = pv[i < 6 ? i : 5]; best = i; }
                    }
                    if (best < 0) best = 0;   /* not reachable with real plane values */
                    int32_t off;
                    if (best < 3) {
                        for (int i = 0; i < 3; i++) nrm[i] = rd32(n, 0x10 + 12 * best + 4 * i);
                        off = rd32(n, 0x34 + 4 * best);
                    } else {
                        for (int i = 0; i < 3; i++) nrm[i] = rd32(n, 0x40 + 12 * (best - 3) + 4 * i);
                        off = rd32(n, 0x64 + 4 * (best - 3));
                    }
                    dep = sv[best < 6 ? best : 5];
                    if ((mask & (1u << (k & 31))) && nrm[2] != 0)
                        dep = tm2_wadd(-p[2], -(tm2_wadd(tm2_wadd(tm2_wmul(nrm[1], p[1]), tm2_wmul(nrm[0], p[0])), tm2_wmul(off, 0x1000)) / nrm[2]));
                    dep = depth_scale(dep, dshift);
                } else {
                    int face = (int8_t)n[5];
                    if (face < 3) for (int i = 0; i < 3; i++) nrm[i] = rd32(n, 0x10 + 12 * face + 4 * i);
                    else for (int i = 0; i < 3; i++) nrm[i] = rd32(n, 0x40 + 12 * (face - 3) + 4 * i);
                    dep = sv[face < 6 ? face : 5];
                    if ((mask & (1u << (k & 31))) && nrm[2] != 0)
                        dep = -tm2_wadd(-p[2], -(tm2_wadd(tm2_wadd(tm2_wmul(dep, 0x1000), tm2_wmul(nrm[1], p[1])), tm2_wmul(nrm[0], p[0])) / nrm[2]));
                    dep = depth_scale(dep, dshift);
                }
                if (rd32(rec, 4) == 1 && dep <= rd32(rec, 0x14)) continue;
                wr32(rec, 4, 1);
                wr32(rec, 0x18, s->current);
                wr32(rec, 0x14, dep);
                for (int i = f->depth - 1; i >= 0; i--) {
                    if (i >= CAR_COLLIDE_MAX_DEPTH) continue;
                    t[0] = nrm[0]; t[1] = nrm[1]; t[2] = nrm[2];
                    row(c, s->mats[i], t, nrm);
                }
                for (int i = 0; i < 3; i++) wr32(rec, 8 + 4 * i, nrm[i]);
                if (rd16(n, 2) == 0) { wr16(rec, 0x22, f->a); wr16(rec, 0x20, f->b); }
                else { wr16(rec, 0x22, rd16(n, 2)); wr16(rec, 0x20, n[6]); }
                wr16(rec, 0x1c, n[8]);
                if (rd32(n, 0xc) != 0 && s->trigger_count < 10) {
                    int fresh = 1;
                    for (int i = 0; i < s->trigger_count; i++) if (s->triggers[i] == nd) fresh = 0;
                    if (fresh) s->triggers[s->trigger_count++] = nd;
                }
            }
            break;
        }
        case 9: {
            int32_t A = rd16(n, 2) ? (uint16_t)rd16(n, 2) : f->a, B = rd16(n, 2) ? (uint16_t)rd16(n, 4) : f->b;
            PUSH((uint32_t)rd32(n, 0x10 + 4 * n[7]), f->depth, A, B, f->shift);
            break;
        }
        case 0xb: {
            if (n[4] & 1) break;
            int32_t A = rd16(n, 2) ? (uint16_t)rd16(n, 2) : f->a, B = rd16(n, 2) ? n[5] : f->b;
            int out = 1;
            for (int i = 0; out && i < n[6]; i++) {
                uint32_t vp = (uint32_t)rd32(n, 0x10 + 4 * i);
                const uint8_t* v = at(c, vp, 0x64);
                if (!v) continue;
                int32_t q[3];
                col(c, vp + 4, f->pos, q);
                if (!(tm2_wadd(rd32(v, 0x28), q[0]) > 0 && tm2_wadd(rd32(v, 0x2c), q[1]) > 0 && tm2_wadd(rd32(v, 0x30), q[2]) > 0)) continue;
                if (v[0] > 3) {
                    col(c, vp + 0x34, f->pos, q);
                    if (tm2_wadd(rd32(v, 0x58), q[0]) < 1) continue;
                    if (v[0] > 4 && (tm2_wadd(rd32(v, 0x5c), q[1]) < 1 || (v[0] > 5 && tm2_wadd(rd32(v, 0x60), q[2]) < 1))) continue;
                }
                out = 0;
            }
            uint32_t child = (uint32_t)rd32(n, out ? 8 : 0xc);
            if (child) PUSH(child, f->depth, A, B, f->shift);
            break;
        }
        case 0xc: {
            int32_t ns = (uint16_t)rd16(n, 4), d = f->shift - ns;
            if (d < 0) { int sh = (int)((uint32_t)(0 - d) & 31); for (int k = 0; k < 3; k++) f->pos[k] = (int32_t)((uint32_t)f->pos[k] << sh); }
            else if (d > 0) for (int k = 0; k < 3; k++) f->pos[k] >>= (d & 31);
            for (int i = 0; i < (uint16_t)rd16(n, 6); i++) PUSH((uint32_t)rd32(n, 8 + 4 * i), f->depth, f->a, f->b, ns);
            break;
        }
        case 0xf:
            /* height field volumes (0x004bcb70): none on the disc, not ported */
            break;
        default:
            break;
        }
        last_depth = f->depth;
    }
#undef PUSH
    s->depth = last_depth;
}

void car_collide(const DpcCollideCtx* c, CarCollideState* s, uint32_t e, uint32_t records,
                 int level, uint32_t prev_mat, uint32_t prev_pos) {
    const uint8_t* E = at(c, e, 0x170);
    if (!E) return;
    int32_t count = rd32(E, 0xf4);
    int32_t epos[3];
    vec_at(c, e + 0xe8, epos);
    s->remaining = count;
    s->first_hit_stops = 0;
    s->all_out = 0;
    int32_t outside = 0;
    uint8_t* r0 = at(c, records, 4);
    if (r0) wr32(r0, 0, 0);
    s->trigger_count = 0;
    for (int k = 0; k < count; k++) {
        uint8_t* rec = at(c, records + 0x20 * k, 0x24);
        if (!rec) continue;
        wr32(rec, 4, 0); wr16(rec, 0x1e, 0); wr32(rec, 0x18, 0); wr16(rec, 0x22, -1); wr16(rec, 0x20, 0); wr16(rec, 0x1c, 0);
        int32_t p[3];
        packed(c, e + 0xb0, e + 0xf8 + 8 * k, p);
        for (int i = 0; i < 3; i++) p[i] = tm2_wadd(p[i], epos[i]);
        int sh = s->shift & 31;
        if (collide_grid_reject(s->grid, p[0] >> sh, p[1] >> sh, p[2] >> sh)) {
            outside++;
            wr32(rec, 4, 2);
        }
    }
    for (int i = 0; i < 10; i++) s->triggers[i] = 0;
    s->prev_mat = prev_mat;
    s->prev_pos = prev_pos;
    uint32_t pc = (uint32_t)rd32(E, 0x15c);
    const uint8_t* P = pc ? at(c, pc, 0x14) : 0;
    s->radius = (P && P[0] == 1) ? rd32(P, 0x10) : 1000;
    int32_t r2 = tm2_wmul(s->radius, s->radius);
    s->entity = e;
    if (outside != count) {
        s->current = (int32_t)s->scene_root;
        if (ru32(c, s->scene_root + 0x15c) != 0 && level) {
            s->depth = 0;
            walk(c, s, s->scene_root, epos, records, r2);
        }
    }
    for (int k = 0; k < count; k++) {
        uint8_t* rec = at(c, records + 0x20 * k, 0x24);
        if (rec && (rd32(rec, 4) == 2 || s->all_out != 0)) { s->remaining++; wr32(rec, 4, 0); }
    }
    for (s->current = (int32_t)s->entity_head; s->remaining > 0 && s->current != 0;
         s->current = (int32_t)ru32(c, (uint32_t)s->current + 0x160)) {
        uint32_t o = (uint32_t)s->current;
        s->depth = 0;
        const uint8_t* O = at(c, o, 0x170);
        if (!O || rd32(O, 8) == 0 || o == e || rd32(O, 0xc) == 0 || rd32(O, 0x15c) == 0) continue;
        s->mats[0] = o + 0xb0;
        int32_t d[3], opos[3], lp[3];
        vec_at(c, o + 0xe8, opos);
        for (int i = 0; i < 3; i++) d[i] = tm2_wadd(epos[i], -opos[i]);
        s->depth = 1;
        col(c, o + 0xb0, d, lp);
        walk(c, s, o, lp, records, r2);
    }
    if (s->floor_kind == 1 || s->floor_kind == 2) {
        for (int k = 0; k < count; k++) {
            uint8_t* rec = at(c, records + 0x20 * k, 0x24);
            if (!rec || rd32(rec, 4) != 0) continue;
            int32_t p[3];
            packed(c, e + 0xb0, e + 0xf8 + 8 * k, p);
            for (int i = 0; i < 3; i++) p[i] = tm2_wadd(p[i], epos[i]);
            if (s->floor_kind == 1) {
                int32_t v = tm2_wadd(tm2_fx12(tm2_wadd(tm2_wadd(tm2_wmul(p[2], s->floor_n[2]), tm2_wmul(p[0], s->floor_n[0])),
                                                       tm2_wmul(p[1], s->floor_n[1]))), s->floor_d);
                if (v > 0) {
                    wr32(rec, 0x18, 0); wr16(rec, 0x20, 0); wr32(rec, 4, 1);
                    wr32(rec, 8, s->floor_n[0]); wr16(rec, 0x22, s->floor_surface); wr32(rec, 0xc, s->floor_n[1]);
                    wr32(rec, 0x14, v); wr16(rec, 0x1c, s->floor_id); wr32(rec, 0x10, s->floor_n[2]);
                }
            } else {
                int32_t d[3];
                for (int i = 0; i < 3; i++) d[i] = tm2_wadd(p[i], -s->floor_c[i]);
                int32_t dd = tm2_wadd(tm2_wadd(tm2_wmul(d[2], d[2]), tm2_wmul(d[0], d[0])), tm2_wmul(d[1], d[1]));
                if (s->floor_r2 < dd) {
                    wr32(rec, 4, 1); wr16(rec, 0x22, s->floor_surface); wr32(rec, 0x18, 0);
                    wr16(rec, 0x1c, s->floor_id); wr16(rec, 0x20, 0);
                    int32_t nn[3];
                    collide_normalize(d, nn);
                    for (int i = 0; i < 3; i++) wr32(rec, 8 + 4 * i, nn[i]);
                    wr32(rec, 0x14, (int32_t)isqrt(dd - s->floor_r2) >> 1);
                }
            }
        }
    }
    for (int i = 0; i < s->trigger_count; i++) {
        const uint8_t* t = at(c, s->triggers[i], 0x10);
        if (t && (t[0] == 8 || t[0] == 7)) dpc_trigger(c, (uint32_t)rd32(t, 0xc), t[0xb]);
    }
}
