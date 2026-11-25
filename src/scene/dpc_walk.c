/*
 * dpc_walk.c: DPC node tree walk, ported from TM2.EXE 0x0049f59c (see dpc_walk.h).
 */
#include "dpc_walk.h"

#include "../math/tm2math.h"

#include <string.h>

typedef struct {
    uint32_t node;
    int32_t slot;
    int32_t pos[3];
    int32_t group;
    int32_t shift;
} Entry;

typedef struct {
    DpcWalk* w;
    Entry stack[DPC_WALK_STACK + 1];
    int32_t depth;
    int err;
} Ctx;

static int ok(Ctx* c, uint32_t p, size_t need) {
    uint32_t o = p - c->w->addr_base;
    if ((size_t)o > c->w->size || c->w->size - o < need) { c->err = 1; return 0; }
    return 1;
}
static uint8_t u8(Ctx* c, uint32_t p) { return ok(c, p, 1) ? c->w->buf[p - c->w->addr_base] : 0; }
static uint16_t u16(Ctx* c, uint32_t p) {
    if (!ok(c, p, 2)) return 0;
    const uint8_t* b = c->w->buf + (p - c->w->addr_base);
    return (uint16_t)(b[0] | b[1] << 8);
}
static int32_t i32(Ctx* c, uint32_t p) {
    if (!ok(c, p, 4)) return 0;
    const uint8_t* b = c->w->buf + (p - c->w->addr_base);
    return (int32_t)(b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24);
}
static void rd_mat(Ctx* c, uint32_t p, int32_t m[9]) { for (int i = 0; i < 9; i++) m[i] = i32(c, p + 4u * (uint32_t)i); }

/* the original's push: write at the current depth, advance while below the limit */
static void push(Ctx* c, uint32_t node, int32_t slot, const int32_t pos[3], int32_t group, int32_t shift) {
    Entry* e = &c->stack[c->depth];
    e->node = node; e->slot = slot; e->group = group; e->shift = shift;
    e->pos[0] = pos[0]; e->pos[1] = pos[1]; e->pos[2] = pos[2];
    if (c->depth < DPC_WALK_STACK) c->depth++;
}

static int visible(Ctx* c, uint32_t bounds, int32_t radius, const Entry* e) {
    return c->w->visible ? c->w->visible(c->w->user, bounds, radius, e->pos, e->slot, (uint8_t)e->shift) : 1;
}

/* type 11: is the viewer (at -pos in the node's frame) inside volume v */
static int inside(Ctx* c, uint32_t v, const int32_t pos[3]) {
    int32_t neg[3] = { (int32_t)(0u - (uint32_t)pos[0]), (int32_t)(0u - (uint32_t)pos[1]), (int32_t)(0u - (uint32_t)pos[2]) };
    int32_t m[9], r[3];
    rd_mat(c, v + 4, m);
    tm2_mat_vec_mul_col(m, neg, r);
    if (!(tm2_wadd(i32(c, v + 0x28), r[0]) > 0 && tm2_wadd(i32(c, v + 0x2c), r[1]) > 0 && tm2_wadd(i32(c, v + 0x30), r[2]) > 0))
        return 0;
    uint8_t planes = u8(c, v);
    if (planes < 4) return 1;
    rd_mat(c, v + 0x34, m);
    tm2_mat_vec_mul_col(m, neg, r);
    if (!(tm2_wadd(i32(c, v + 0x58), r[0]) > 0)) return 0;
    if (planes < 5) return 1;
    if (!(tm2_wadd(i32(c, v + 0x5c), r[1]) > 0)) return 0;
    if (planes < 6) return 1;
    return tm2_wadd(i32(c, v + 0x60), r[2]) > 0;
}

int dpc_walk(DpcWalk* w, uint32_t root, uint32_t obj, const int32_t obj_pos[3],
             const int32_t obj_mat[12], const int32_t mat_c[9], const int32_t mat_d[9], int32_t root_shift) {
    Ctx ctx;   /* ~28 KB, the stack of 1000 entries */
    Ctx* c = &ctx;
    c->w = w;
    c->err = 0;
    int32_t before = w->item_count;

    int32_t s0 = w->slot_index;
    memcpy(w->mat[0][s0], obj_mat, 12 * sizeof(int32_t));
    w->parent[s0][0] = obj_mat;
    w->parent[s0][1] = w->mat[1][s0];
    w->parent[s0][2] = mat_c;
    w->parent[s0][3] = mat_d;
    c->depth = 0;
    push(c, root, s0, obj_pos, w->group_count, root_shift);

    while (!c->err) {
        c->depth--;
        if (c->depth < 0) break;
        Entry e = c->stack[c->depth];
        uint32_t n = e.node;
        switch (u8(c, n)) {
        case 0: {
            int v = (u8(c, n + 0x19) & 0x20) ? 1 : visible(c, n + 0x1c, i32(c, n + 0x28), &e);
            if (v == 1 && w->item_count < DPC_WALK_MAX_ITEMS) {
                DpcDrawItem* it = &w->items[w->item_count++];
                it->node = n; it->slot = e.slot; it->group = e.group; it->root = obj; it->shift = e.shift;
                it->pos[0] = e.pos[0]; it->pos[1] = e.pos[1]; it->pos[2] = e.pos[2];
            }
            break;
        }
        case 1: {
            uint8_t f = u8(c, n + 0x15);
            if (f & 2) break;
            int v = (f & 4) ? 1 : visible(c, n + 4, i32(c, n + 0x10), &e);
            if (v != 1) break;
            for (int i = 0; i < u8(c, n + 0x14); i++)
                push(c, (uint32_t)i32(c, n + 0x18 + 4u * (uint32_t)i), e.slot, e.pos, e.group, e.shift);
            break;
        }
        case 2: {
            uint32_t tab = n + 0x14;
            if (w->lod_disabled) {
                push(c, (uint32_t)i32(c, (uint32_t)i32(c, tab) + 8), e.slot, e.pos, e.group, e.shift);
                break;
            }
            int sh = e.shift & 31;
            int32_t d[3];
            for (int k = 0; k < 3; k++) d[k] = tm2_wadd(e.pos[k], i32(c, n + 4 + 4u * (uint32_t)k)) >> sh;
            uint32_t sq = (uint32_t)d[1] * (uint32_t)d[1] + (uint32_t)d[0] * (uint32_t)d[0] + (uint32_t)d[2] * (uint32_t)d[2];
            int32_t cnt = i32(c, n + 0x10);
            for (int32_t i = 0; i < cnt && !c->err; i++) {
                uint32_t r = (uint32_t)i32(c, tab + 4u * (uint32_t)i);
                if ((uint32_t)i32(c, r + 4) <= sq && sq < (uint32_t)i32(c, r)) {
                    push(c, (uint32_t)i32(c, r + 8), e.slot, e.pos, e.group, e.shift);
                    break;
                }
            }
            break;
        }
        case 3: {
            if (u8(c, n + 0x12) == 0) {
                uint32_t fc = n + 0x10 + (uint32_t)w->frame_clear, fs = n + 0x10 + (uint32_t)w->frame_set;
                if (ok(c, fc, 1)) w->buf[fc - w->addr_base] = 0;
                if (u8(c, fs) == 0) {
                    uint8_t kind = u8(c, n + 0xa);
                    if (kind <= 3 && w->anim) w->anim(w->user, n, kind);
                    if (ok(c, fs, 1)) w->buf[fs - w->addr_base] = 1;
                }
            }
            push(c, (uint32_t)i32(c, n + 0x20 + 4u * u8(c, n + 0xc)), e.slot, e.pos, e.group, e.shift);
            break;
        }
        case 4: {
            if (u8(c, n + 0x13) & 2) break;
            int32_t p[3];
            for (int k = 0; k < 3; k++) p[k] = tm2_wadd(e.pos[k], i32(c, n + 4 + 4u * (uint32_t)k));
            w->group_count++;
            for (int i = 0; i < u8(c, n + 0x12); i++)
                push(c, (uint32_t)i32(c, n + 0x14 + 4u * (uint32_t)i), e.slot, p, w->group_count, e.shift);
            break;
        }
        case 5: {
            if (u8(c, n + 0x37) & 2) break;
            int32_t t[3], p[3], rot[9], rt[9];
            for (int k = 0; k < 3; k++) t[k] = tm2_wadd(e.pos[k], i32(c, n + 0x28 + 4u * (uint32_t)k));
            rd_mat(c, n + 4, rot);
            tm2_mat_vec_mul_col(rot, t, p);
            if (w->slot_index < DPC_WALK_MAX_SLOTS) {
                int32_t s = ++w->slot_index;
                const int32_t* const* par = w->parent[e.slot];
                tm2_matrix_transpose(rot, rt);
                tm2_matrix_load(par[0]);
                tm2_matrix_multiply(rt, w->mat[0][s]);
                tm2_matrix_transpose(rot, rt);
                tm2_matrix_load(par[1]);
                tm2_matrix_multiply(rt, w->mat[1][s]);
                const int32_t* pc = par[2];
                const int32_t* pd = par[3];
                tm2_matrix_load(rot);
                tm2_matrix_multiply(pc, w->mat[2][s]);
                tm2_matrix_load(rot);
                tm2_matrix_multiply(pd, w->mat[3][s]);
                for (int k = 0; k < 4; k++) w->parent[s][k] = w->mat[k][s];
            }
            w->group_count++;
            for (int i = 0; i < u8(c, n + 0x36); i++)
                push(c, (uint32_t)i32(c, n + 0x38 + 4u * (uint32_t)i), w->slot_index, p, w->group_count, e.shift);
            break;
        }
        case 9:
            push(c, (uint32_t)i32(c, n + 0x10 + 4u * u8(c, n + 7)), e.slot, e.pos, e.group, e.shift);
            break;
        case 0xb: {
            if (u8(c, n + 4) & 2) break;
            int out = 1;
            for (int i = 0; out && i < u8(c, n + 6); i++)
                if (inside(c, (uint32_t)i32(c, n + 0x10 + 4u * (uint32_t)i), e.pos)) out = 0;
            uint32_t child = (uint32_t)i32(c, n + (out ? 8u : 0xcu));
            if (child) push(c, child, e.slot, e.pos, e.group, e.shift);
            break;
        }
        case 0xc: {
            w->group_count++;
            int32_t ns = u16(c, n + 4);
            int32_t d = e.shift - ns;
            int32_t p[3] = { e.pos[0], e.pos[1], e.pos[2] };
            if (d < 0) {
                int s = (int)((uint32_t)(0 - d) & 31);
                for (int k = 0; k < 3; k++) p[k] = (int32_t)((uint32_t)p[k] << s);
            } else if (d > 0) {
                for (int k = 0; k < 3; k++) p[k] >>= (d & 31);
            }
            for (int i = 0; i < u16(c, n + 6); i++)
                push(c, (uint32_t)i32(c, n + 8 + 4u * (uint32_t)i), e.slot, p, w->group_count, ns);
            break;
        }
        case 0xf:
            for (int i = 0; i < u8(c, n + 0xa); i++)
                push(c, (uint32_t)i32(c, n + 0xc + 4u * (uint32_t)i), e.slot, e.pos, e.group, e.shift);
            break;
        default:
            break;
        }
    }
    return c->err ? -1 : w->item_count - before;
}
