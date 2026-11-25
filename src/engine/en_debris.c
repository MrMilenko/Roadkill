/*
 * en_debris.c: debris chunks and bursts, ported from TM2.EXE 0x00489a00, 0x00489a70,
 * 0x00489ab0, 0x0048a738, 0x0048a8b8, 0x0048a488, 0x0048b3f8 (see en_debris.h).
 */
#include "en_debris.h"

#include "../math/tm2math.h"
#include "../scene/car_collide.h"   /* collide_normalize 0x0048b5a0 */

#include <stddef.h>
#include <string.h>

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static void add16(uint8_t* p, int16_t d) {
    int16_t v; memcpy(&v, p, 2); v = (int16_t)(v + d); memcpy(p, &v, 2);
}
static void add32(uint8_t* p, int32_t d) { wr32(p, (int32_t)((uint32_t)rd32(p) + (uint32_t)d)); }
static int32_t wadd(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }

/* math_build_matrix on angles and an output that may sit in memory as bytes */
static void build(const uint8_t* ang, uint8_t* out) {
    int16_t a[3]; int32_t m[9];
    memcpy(a, ang, 6);
    tm2_build_matrix(a, m);
    memcpy(out, m, sizeof m);
}

void en_debris_init(EnDebrisEnv* e) {
    EnDebrisData* d = e->d;
    for (int i = 0; i < EN_DEBRIS_CHUNKS; i++) {
        uint32_t ent = e->alloc(e->user);
        d->chunk[i].entity = ent;
        e->list_add(e->user, ent);
        e->set_draw(e->user, d->chunk[i].entity, 0);
        d->chunk[i].active = 0;
    }
    for (int i = 0; i < EN_DEBRIS_BURSTS; i++) d->burst[i].active = 0;
    d->holder = e->alloc(e->user);
    d->draw = e->alloc(e->user);
    wr32(e->entity(e->user, d->draw) + 0x10, (int32_t)d->holder);
}

void en_debris_clear(EnDebrisEnv* e) {
    EnDebrisData* d = e->d;
    for (int i = 0; i < EN_DEBRIS_CHUNKS; i++) {
        d->chunk[i].active = 0;
        e->set_draw(e->user, d->chunk[i].entity, 0);
    }
    for (int i = 0; i < EN_DEBRIS_BURSTS; i++) d->burst[i].active = 0;
}

void en_debris_update(EnDebrisEnv* e) {
    EnDebrisData* d = e->d;
    for (int i = 0; i < EN_DEBRIS_CHUNKS; i++) {
        EnDebrisChunk* c = &d->chunk[i];
        if (c->active == 0) continue;
        uint8_t* ent = e->entity(e->user, c->entity);
        add16(ent + 0xe0, c->spin[0]);
        add16(ent + 0xe2, c->spin[1]);
        add16(ent + 0xe4, c->spin[2]);
        build(ent + 0xe0, ent + 0xb0);
        add32(ent + 0xe8, c->vel[0]);
        add32(ent + 0xec, c->vel[1]);
        add32(ent + 0xf0, c->vel[2]);
        if (c->age > 10) c->vel[2]--;
        if (c->life < c->age) {
            e->set_draw(e->user, c->entity, 0);
            c->active = 0;
        }
        c->age++;
    }
    for (int i = 0; i < EN_DEBRIS_BURSTS; i++) {
        EnDebrisBurst* b = &d->burst[i];
        if (b->active == 0) continue;
        if (b->frame < b->hold) {
            for (int k = 0; k < 4; k++)
                for (int j = 0; j < 3; j++) b->pos[k][j] = wadd(b->pos[k][j], b->drift[j]);
            for (int j = 0; j < 3; j++) b->angle[j] = (int16_t)(b->angle[j] + b->spin_hold[j]);
            build((const uint8_t*)b->angle, (uint8_t*)b->mat);
        } else if (b->frame != b->hold) {
            for (int k = 0; k < 4; k++) {
                for (int j = 0; j < 3; j++) b->pos[k][j] = wadd(b->pos[k][j], b->vel[k][j]);
                b->vel[k][2]--;
                for (int j = 0; j < 3; j++) b->world[k][j] = wadd(b->pos[k][j], b->piece[0].pos[j]);
                /* ORIGINAL QUIRK: the spin is added (and the matrix rebuilt) once per piece, so
                 * after the hold the burst turns 4 x spin_after per frame */
                for (int j = 0; j < 3; j++) b->angle[j] = (int16_t)(b->angle[j] + b->spin_after[j]);
                build((const uint8_t*)b->angle, (uint8_t*)b->mat);
            }
            if (b->frame > b->end) { b->f04 = 0; b->active = 0; }
        }
        b->frame++;
    }
}

void en_debris_start_burst(EnDebrisEnv* e, uint32_t ent, const int16_t dir[2], const int16_t* spin,
                           int32_t hold, int32_t scale, int32_t end, int32_t random_spin, const int32_t* drift) {
    EnDebrisData* d = e->d;
    EnDebrisBurst* b = NULL;
    int bi = 0;
    for (int i = 0; i < EN_DEBRIS_BURSTS; i++)
        if (d->burst[i].active == 0) { b = &d->burst[i]; bi = i; break; }
    if (!b) return;
    static const int32_t zmask[4] = { 0x1f, 0x1f, 0xf, 0x1f };
    for (int k = 0; k < 4; k++) {
        b->vel[k][0] = (int32_t)((uint32_t)dir[0] * (uint32_t)scale) >> 12;
        b->vel[k][1] = (int32_t)((uint32_t)dir[1] * (uint32_t)scale) >> 12;
        int32_t r = e->rand(e->user) & zmask[k];
        b->vel[k][2] = wadd(r, k == 1 ? 5 : scale);
    }
    for (int k = 0; k < 4; k++) b->pos[k][0] = b->pos[k][1] = b->pos[k][2] = 0;
    if (ent != 0) {
        b->frame = 0;
        d->capture = e->burst_base + (uint32_t)bi * (uint32_t)sizeof(EnDebrisBurst);
        b->end = end;
        b->f04 = 0;
        e->draw_unculled(e->user, ent);
        b->active = 1;
    }
    for (int j = 0; j < 3; j++) { b->angle[j] = 0; b->spin_after[j] = 0; }
    if (drift) { for (int j = 0; j < 3; j++) b->drift[j] = drift[j]; }
    else { b->drift[0] = b->drift[1] = b->drift[2] = 0; }
    if (random_spin) {
        switch (e->rand(e->user) & 3) {
        case 0: b->spin_after[1] = (int16_t)((e->rand(e->user) & 0x1f) + 0x1e); break;
        case 1: b->spin_after[0] = (int16_t)((e->rand(e->user) & 0x1f) + 0x1e); break;
        case 2: b->spin_after[2] = (int16_t)((e->rand(e->user) & 0x1f) + 0x1e); break;
        default: b->spin_after[1] = (int16_t)((e->rand(e->user) & 0x3f) + 2); break;
        }
    }
    if (spin) { for (int j = 0; j < 3; j++) b->spin_hold[j] = spin[j]; }
    else { b->spin_hold[0] = b->spin_hold[1] = b->spin_hold[2] = 0; }
    b->hold = hold;
}

void en_debris_burst_at(EnDebrisEnv* e, uint32_t node_addr, const uint8_t* node, const int32_t pos[3],
                        const int16_t dir[2], const int16_t* spin, const int32_t* drift, int32_t hold,
                        int32_t scale, int32_t end, int32_t random_spin) {
    if (node_addr == 0) return;
    EnDebrisData* d = e->d;
    uint8_t* ent = e->entity(e->user, d->draw);
    if (node[0] == 4) {
        wr32(ent + 0xe8, (int32_t)((uint32_t)pos[0] - (uint32_t)rd32(node + 4)));
        wr32(ent + 0xec, (int32_t)((uint32_t)pos[1] - (uint32_t)rd32(node + 8)));
        wr32(ent + 0xf0, (int32_t)((uint32_t)pos[2] - (uint32_t)rd32(node + 0xc)));
    } else {
        wr32(ent + 0xe8, pos[0]);
        wr32(ent + 0xec, pos[1]);
        wr32(ent + 0xf0, pos[2]);
    }
    wr32(ent + 0x15c, (int32_t)node_addr);
    en_debris_start_burst(e, d->draw, dir, spin, hold, scale, end, random_spin, drift);
}

void en_debris_break_model(EnDebrisEnv* e, uint32_t ent, uint8_t* node, const int32_t target[3],
                           const int16_t dir[2], int32_t scale, int32_t life) {
    EnDebrisData* d = e->d;
    uint8_t* E = e->entity(e->user, ent);
    int made = 0;
    int32_t bias = 0;
    int32_t mat[9];
    memcpy(mat, E + 0xb0, sizeof mat);
    if (node) {
        for (int i = 0; i < node[0x14]; i++) {
            EnDebrisChunk* c = NULL;
            for (int k = 0; k < EN_DEBRIS_CHUNKS; k++)
                if (d->chunk[k].active == 0) { c = &d->chunk[k]; break; }
            if (!c) continue;
            const uint8_t* ch = e->node(e->user, (uint32_t)rd32(node + 0x18 + 4 * i));
            if (ch[0] != 4) continue;
            c->active = 1;
            c->life = life;
            made = 1;
            e->set_model(e->user, c->entity, (uint32_t)rd32(ch + 0x14));
            e->set_draw(e->user, c->entity, 1);
            int32_t off[3] = { rd32(ch + 4), rd32(ch + 8), rd32(ch + 0xc) }, o[3];
            memcpy(mat, E + 0xb0, sizeof mat);   /* the chunk entity may be ent itself */
            tm2_mat_vec_mul_row(mat, off, o);
            uint8_t* C = e->entity(e->user, c->entity);
            wr32(C + 0xe8, wadd(rd32(E + 0xe8), o[0]));
            wr32(C + 0xec, wadd(rd32(E + 0xec), o[1]));
            wr32(C + 0xf0, wadd(rd32(E + 0xf0), o[2]));
            memset(C + 0xe0, 0, 6);
            int32_t r = e->rand(e->user) & 0x3f;
            if (r <= 0) {
                c->vel[0] = (int32_t)((uint32_t)(dir[0] + r) * (uint32_t)scale) >> 12;
                c->vel[1] = (int32_t)((uint32_t)(dir[1] + r) * (uint32_t)scale) >> 12;
                c->vel[2] = wadd(scale, 5);
            } else {
                int32_t v[3] = { (int32_t)((uint32_t)target[0] - (uint32_t)rd32(C + 0xe8)),
                                 (int32_t)((uint32_t)target[1] - (uint32_t)rd32(C + 0xec)),
                                 (int32_t)((uint32_t)target[2] - (uint32_t)rd32(C + 0xf0)) }, nv[3];
                collide_normalize(v, nv);
                uint32_t s3 = (uint32_t)scale * 3u;
                c->vel[0] = (int32_t)(s3 * (uint32_t)nv[0]) >> 12;
                c->vel[1] = (int32_t)(s3 * (uint32_t)nv[1]) >> 12;
                c->vel[2] = wadd(wadd(scale, 5), bias);
                c->vel[1] = wadd(c->vel[1], bias);
                c->vel[0] = wadd(c->vel[0], bias);
                bias += 3;
            }
            c->spin[0] = (int16_t)(r + 0xf);
            c->spin[1] = (int16_t)((e->rand(e->user) & 0x3f) + 10);
            c->age = 0;
            c->spin[2] = (int16_t)((e->rand(e->user) & 0x3f) + 10);
        }
    }
    if (made) node[0x14] = 0;
    int32_t saved = rd32(E + 0x10);
    wr32(E + 0x10, (int32_t)d->holder);
    static const int16_t zero[3] = { 0, 0, 0 };
    en_debris_start_burst(e, ent, dir, zero, 0, scale, life, 1, NULL);
    E = e->entity(e->user, ent);
    wr32(E + 8, 0);
    wr32(E + 0x10, saved);
}

void en_debris_capture(EnDebrisEnv* e, const DpcDrawItem* items, int32_t count, const int32_t (*mats)[12]) {
    uint32_t off = e->d->capture - e->burst_base;
    if (off % sizeof(EnDebrisBurst) || off / sizeof(EnDebrisBurst) >= EN_DEBRIS_BURSTS) return;
    EnDebrisBurst* b = &e->d->burst[off / sizeof(EnDebrisBurst)];
    for (int32_t i = 0; i < count && i < 16; i++) {
        b->f04++;
        b->piece[i].node = items[i].node;
        b->piece[i].shift = items[i].shift;
        tm2_mat_vec_mul_col(mats[items[i].slot], items[i].pos, b->piece[i].pos);
    }
}
