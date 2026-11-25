/*
 * en_scene.c: scene drivers, ported from TM2.EXE 0x00492a40, 0x004a0e20, 0x004a10fc, 0x004a1194
 * (see en_scene.h).
 */
#include "en_scene.h"

#include "../math/tm2math.h"

#include <string.h>

static int32_t scratch[16];

/* entity memory lives in the walk's buffer */
static int32_t* P(EnScene* s, uint32_t a) {
    uint32_t o = a - s->w->addr_base;
    if ((size_t)o > s->w->size || s->w->size - o < 0x30) { memset(scratch, 0, sizeof scratch); return scratch; }
    return (int32_t*)(void*)(s->w->buf + o);
}
static int32_t R32(EnScene* s, uint32_t a) { int32_t v; memcpy(&v, P(s, a), 4); return v; }
static uint8_t R8(EnScene* s, uint32_t a) { return *(uint8_t*)P(s, a); }

void en_entity_setup(EnScene* s, uint32_t e, int32_t out[9], int force) {
    uint32_t v = (uint32_t)R32(s, e + 0x10);
    int32_t pos[3], t[3];
    for (int k = 0; k < 3; k++) pos[k] = tm2_wadd(R32(s, v + 0x14 + 4u * (uint32_t)k), R32(s, e + 0xe8 + 4u * (uint32_t)k));
    uint32_t node = (uint32_t)R32(s, e + 0x15c);
    int16_t vis = 1;
    if (R8(s, node) == 1 && !(R8(s, node + 0x15) & 4)) {
        int32_t nr = (int32_t)(0u - (uint32_t)R32(s, node + 0x10));
        tm2_mat_vec_mul_row(P(s, v + 0x50), pos, t);
        if (nr > t[0] || nr > t[1] || nr > t[2]) vis = 0;
        else {
            tm2_mat_vec_mul_row(P(s, v + 0x80), pos, t);
            if (nr > t[0] || nr > t[1]) vis = 0;
        }
    }
    if (force) vis = 1;
    memcpy((uint8_t*)P(s, e + 0x160) + 6, &vis, 2);   /* +0x166 */
    if (!vis) return;
    int32_t tr[9];
    int32_t* rot = P(s, e + 0xb0);
    tm2_mat_vec_mul_col(rot, pos, P(s, e + 0x14));
    tm2_matrix_transpose(rot, tr);
    tm2_matrix_load(P(s, v + 0x20));
    tm2_matrix_multiply(tr, P(s, e + 0x20));
    if (force) {
        tm2_matrix_load(rot);
        tm2_matrix_multiply(P(s, v + 0x50), P(s, e + 0x50));
        tm2_matrix_load(rot);
        tm2_matrix_multiply(P(s, v + 0x80), P(s, e + 0x80));
    }
    tm2_matrix_transpose(rot, tr);
    tm2_matrix_load(P(s, (uint32_t)R32(s, v + 4) + 0x3c));
    tm2_matrix_multiply(tr, out);
}

static void walk(EnScene* s, uint32_t e, uint32_t model) {
    dpc_walk(s->w, model, e, P(s, e + 0x14), P(s, e + 0x20), P(s, e + 0x50), P(s, e + 0x80), s->root_shift);
}

static void next_slot(EnScene* s) {
    if (s->w->slot_index < 400) s->w->slot_index++;
}

/* the scene root's view matrix (owner +0x3c, 12 ints) into the current slot's chain B */
static void root_view(EnScene* s, uint32_t root) {
    memcpy(s->w->mat[1][s->w->slot_index], P(s, (uint32_t)R32(s, root + 4) + 0x3c), 12 * sizeof(int32_t));
}

void en_scene_draw(EnScene* s, uint32_t param, int32_t view) {
    DpcWalk* w = s->w;
    w->item_count = 0;
    w->slot_index = 0;
    memset(s->cleared, 0, sizeof s->cleared);
    w->group_count = 0;
    s->cull_depth = s->cull_depth_init;

    uint32_t root = s->scene_root;
    uint32_t model = (uint32_t)R32(s, root + 0x15c);
    if (model) {
        root_view(s, root);
        walk(s, root, model);
    }
    for (int i = 0; i < EN_SCENE_EXTRA; i++) {
        if (!s->extra[i]) continue;
        root = s->scene_root;
        next_slot(s);
        root_view(s, root);
        walk(s, root, s->extra[i]);
    }
    uint32_t ve = view < 2 ? s->viewer[view] : s->viewer[0];
    if (ve) {
        next_slot(s);
        en_entity_setup(s, ve, w->mat[1][w->slot_index], 1);
        w->group_count++;
        walk(s, ve, (uint32_t)R32(s, ve + 0x15c));
    }
    s->cull_depth = s->cull_depth_init;
    if (!s->force_visible) s->cull_force = 1;
    for (uint32_t e = s->list_head; e; e = (uint32_t)R32(s, e + 0x160)) {
        if (!R32(s, e + 8) || !R32(s, e + 0x15c)) continue;
        next_slot(s);
        en_entity_setup(s, e, w->mat[1][w->slot_index], s->force_visible);
        int16_t vis;
        memcpy(&vis, (uint8_t*)P(s, e + 0x160) + 6, 2);
        if (vis) {
            w->group_count++;
            walk(s, e, (uint32_t)R32(s, e + 0x15c));
        }
    }
    s->cull_force = 0;
    s->draw(s->user, param, w->item_count);
    if (!s->anim_on) return;
    for (int32_t i = 0; i < s->anim_count && i < EN_SCENE_ANIM; i++) {
        uint32_t n = s->anim_node[i];
        uint8_t* b = (uint8_t*)P(s, n);
        b[0x10 + w->frame_clear] = 0;
        if (b[0x10 + w->frame_set]) continue;
        uint8_t kind = b[0xa];
        if (kind <= 3) s->anim(s->user, n, kind);
        b[0x10 + w->frame_set] = 1;
    }
}

void en_scene_draw_one(EnScene* s, uint32_t param, uint32_t ent) {
    DpcWalk* w = s->w;
    w->item_count = 0;
    w->slot_index = 0;
    w->group_count = 0;
    s->cull_depth = s->cull_depth_init;
    s->anim_ticks = (int32_t)s->ticks60 * 0x3c / 0x3c;
    en_entity_setup(s, ent, w->mat[1][0], s->force_visible);
    walk(s, ent, (uint32_t)R32(s, ent + 0x15c));
    w->slot_index++;
    s->draw(s->user, param, w->item_count);
}

void en_scene_light_one(EnScene* s, uint32_t ent) {
    DpcWalk* w = s->w;
    int32_t force = s->force_visible;
    w->item_count = 0;
    w->slot_index = 0;
    w->group_count = 0;
    s->cull_force = 1;
    s->cull_depth = s->cull_depth_init;
    w->lod_disabled = 1;
    en_entity_setup(s, ent, w->mat[1][0], force);
    walk(s, ent, (uint32_t)R32(s, ent + 0x15c));
    w->lod_disabled = 0;
    s->cull_force = 0;
    w->slot_index++;
    s->lights(s->user, w->item_count);
}
