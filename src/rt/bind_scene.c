/*
 * bind_scene.c: the scene tree ports of src/scene (the DPC render walk, the point and car
 * collision walks), called the way tools/emu/diff_dpc_walk.py / diff_dpc_collide.py /
 * diff_car_collide.py called the originals. The trees are relocated DPC data in game memory:
 * every context gets all of it (buf = the address space, addr_base 0). The walks' globals are
 * copied from game memory into the port's state before the call and its results back after.
 * The animation steps of type 3 nodes call 0x004a09f8 / 0x004a0a78 / 0x004a0b08 / 0x004a0b7c by
 * kind with the node in eax.
 */
#include "rt_env.h"

#include <stdio.h>
#include <stdlib.h>

#include "host/win32_host.h"
#include "math/tm2math.h"
#include "scene/car_collide.h"
#include "scene/dpc_collide.h"
#include "scene/dpc_walk.h"

#define A(i) rt_arg(a, n, (i))

static void math_tables(void)
{
    static int loaded;
    if (!loaded) {
        char s[1024], x[1024];   /* in the data folder, any letter case (host/data/data_dir.h) */
        w32_host_path("SINTAB.DAT", s, sizeof s);
        w32_host_path("TM2.EXE", x, sizeof x);
        if (tm2_math_load_tables(s, x) != 0) rt_stop("math tables: SINTAB.DAT / TM2.EXE in the game's data folder", 0);
        loaded = 1;
    }
}

static const uint32_t ANIM_FN[4] = {0x004a09f8u, 0x004a0a78u, 0x004a0b08u, 0x004a0b7cu};
static void cb_anim(void* u, uint32_t node, int kind)
{
    (void)u;
    uint32_t v[1] = {node};
    if (kind >= 0 && kind < 4) rt_call(ANIM_FN[kind], v, 1);
}

_Static_assert(sizeof(DpcCollision) == 0x14, "DpcCollision is the original's 0x14 byte result");

static DpcCollideCtx collide_ctx(void)
{
    DpcCollideCtx c = {rt_base, RT_SIZE, 0, (int32_t)rt_r32(0x009f7888u), cb_anim, NULL};
    return c;
}

static void rd_i32s(uint32_t a, int32_t* v, int k) { for (int i = 0; i < k; i++) v[i] = (int32_t)rt_r32(a + 4u * (uint32_t)i); }
static void wr_i32s(uint32_t a, const int32_t* v, int k) { for (int i = 0; i < k; i++) rt_w32(a + 4u * (uint32_t)i, (uint32_t)v[i]); }

/* ---- dpc_collide.c ---- */

/* 0x0048d0b8(scene entity, point, result 0x14 bytes): the tree at entity +0x15C, the starting
 * shift 0x004e8288 */
static uint32_t b_dpc_collide(const uint32_t* a, int n)
{
    math_tables();
    DpcCollideCtx c = collide_ctx();
    int32_t p[3];
    rd_i32s(A(1), p, 3);
    DpcCollision r;
    memcpy(&r, rt_ptr(A(2)), sizeof r);
    dpc_collide(&c, rt_r32(A(0) + 0x15c), p, (int32_t)rt_r32(0x004e8288u), &r);
    memcpy(rt_ptr(A(2)), &r, sizeof r);
    return 0;
}

/* 0x004a0ccc(node, mode) */
static uint32_t b_dpc_trigger(const uint32_t* a, int n)
{
    DpcCollideCtx c = collide_ctx();
    dpc_trigger(&c, A(0), A(1));
    return 0;
}

/* ---- car_collide.c (globals: car_collide.h) ---- */

#define G_GRID 0x00c85bd0u

static int grid_load(CollideGrid* g)
{
    g->x = (int32_t)rt_r32(G_GRID);
    g->y = (int32_t)rt_r32(G_GRID + 4);
    g->z = (int32_t)rt_r32(G_GRID + 8);
    g->sx = (int32_t)rt_r32(G_GRID + 0xc);
    g->sy = (int32_t)rt_r32(G_GRID + 0x10);
    g->sz = (int32_t)rt_r32(G_GRID + 0x14);
    g->cell = (int32_t)rt_r32(G_GRID + 0x18);
    g->row = (int32_t)rt_r32(G_GRID + 0x1c);
    g->layer = (int32_t)rt_r32(G_GRID + 0x28);
    uint32_t bits = rt_r32(G_GRID + 0x2c);
    g->bits = bits ? (const uint32_t*)(const void*)rt_ptr(bits) : NULL;
    return bits != 0;
}

/* 0x004bca88(x, y, z) */
static uint32_t b_collide_grid_reject(const uint32_t* a, int n)
{
    CollideGrid g;
    grid_load(&g);
    return (uint32_t)collide_grid_reject(&g, (int32_t)A(0), (int32_t)A(1), (int32_t)A(2));
}

/* 0x0048b5a0(v, out) */
static uint32_t b_vec_normalize_4096(const uint32_t* a, int n)
{
    int32_t v[3], o[3];
    rd_i32s(A(0), v, 3);
    collide_normalize(v, o);
    wr_i32s(A(1), o, 3);
    return 0;
}

/* 0x0049cec0(entity, records, level, prev matrix, then on the stack prev position) */
static uint32_t b_car_collide(const uint32_t* a, int n)
{
    static CarCollideState s;
    static CollideGrid g;
    math_tables();
    memset(&s, 0, sizeof s);
    s.grid = grid_load(&g) ? &g : NULL;
    s.shift = (int32_t)rt_r32(0x004e8430u);
    s.scene_root = rt_r32(0x009f7e38u);
    s.entity_head = rt_r32(0x009f7e34u);
    s.sphere_mode = (int32_t)rt_r32(0x00a3cf80u);
    s.floor_kind = rt_r8(0x00a3cf9cu);
    s.floor_id = rt_r8(0x00a3cf9du);
    for (int i = 0; i < 3; i++) {
        s.floor_n[i] = (int16_t)rt_r16(0x00a3cf90u + 2u * (uint32_t)i);
        s.floor_c[i] = (int32_t)rt_r32(0x00a3cfa4u + 4u * (uint32_t)i);
    }
    s.floor_d = (int32_t)rt_r32(0x00a3cf98u);
    s.floor_surface = rt_r16(0x00a3cfa0u);
    s.floor_r2 = (int32_t)rt_r32(0x00a3cfb0u);
    DpcCollideCtx c = collide_ctx();
    car_collide(&c, &s, A(0), A(1), (int)A(2), A(3), A(4));
    rt_w32(0x00a3cf78u, (uint32_t)s.remaining);
    rt_w32(0x00a3cf7cu, (uint32_t)s.all_out);
    rt_w32(0x00a3cf84u, (uint32_t)s.current);
    rt_w32(0x00a3cf88u, (uint32_t)s.radius);
    rt_w32(0x00a3cf8cu, (uint32_t)s.first_hit_stops);
    rt_w32(0x00a3c898u, (uint32_t)s.depth);
    for (int i = 0; i < CAR_COLLIDE_MAX_DEPTH; i++) rt_w32(0x00a3c89cu + 4u * (uint32_t)i, s.mats[i]);
    rt_w32(0x00a3cee4u, s.entity);
    rt_w32(0x00a3cee0u, s.prev_mat);
    rt_w32(0x00a3cedcu, s.prev_pos);
    for (int k = 0; k < CAR_COLLIDE_MAX_POINTS; k++) wr_i32s(0x00a3cee8u + 12u * (uint32_t)k, s.world[k], 3);
    rt_w32(0x00a3cfb4u, (uint32_t)s.trigger_count);
    for (int i = 0; i < 10; i++) rt_w32(0x00a3cfb8u + 4u * (uint32_t)i, s.triggers[i]);
    return 0;
}

/* ---- dpc_walk.c ---- */

#define G_ITEMS 0x00a81c0cu
#define G_SLOT 0x00a81c10u
#define G_GROUP 0x00a81c14u
#define G_LOD 0x00a81c40u
#define G_SHIFT 0x004e8448u
#define G_FCLEAR 0x004e844cu
#define G_FSET 0x00a81c38u
#define RLIST 0x00a658c8u
#define PARENTS 0x00a801c8u   /* 4 matrix addresses per slot */
static const uint32_t MATS[4] = {0x00a6d5c8u, 0x00a720c8u, 0x00a76bc8u, 0x00a7b6c8u};

static DpcWalk W;

/* the game address of a matrix the walk points at: one of its own slots (copied to the
 * original's slot arrays first) or the caller's (already game memory) */
static uint32_t mat_addr(const int32_t* p)
{
    const int32_t* m0 = &W.mat[0][0][0];
    const size_t per = (size_t)(DPC_WALK_MAX_SLOTS + 1) * 12;
    if (p >= m0 && p < m0 + 4 * per) {
        size_t i = (size_t)(p - m0);
        int k = (int)(i / per), s = (int)(i % per / 12), o = (int)(i % 12);
        uint32_t g = MATS[k] + 0x30u * (uint32_t)s;
        wr_i32s(g, W.mat[k][s], 12);
        return g + 4u * (uint32_t)o;
    }
    return p ? rt_addr(p) : 0;
}

/* the culling test 0x004a043c(bounds, radius, &entry pos, slot's matrix 2, then on the stack
 * slot's matrix 3, shift): the entry pos is copied into a frame */
static int cb_visible(void* u, uint32_t bounds, int32_t radius, const int32_t pos[3], int32_t slot, uint8_t shift)
{
    (void)u;
    uint32_t f = rt_frame_push(16);
    wr_i32s(f, pos, 3);
    uint32_t v[6] = {bounds, (uint32_t)radius, f, mat_addr(W.parent[slot][2]), mat_addr(W.parent[slot][3]), shift};
    uint32_t r = rt_call(0x004a043cu, v, 6);
    rt_frame_pop(16);
    return (int)r;
}

/* 0x0049f59c(object, root): object +0x14 position, +0x20 matrix (12), +0x50 / +0x80 matrices; the
 * render list, the new slots' matrices and the slot parent table are written to the original's
 * globals after the walk */
static uint32_t b_dpc_walk(const uint32_t* a, int n)
{
    math_tables();
    uint32_t obj = A(0), root = A(1);
    W.buf = rt_base;
    W.size = RT_SIZE;
    W.addr_base = 0;
    W.lod_disabled = (int)rt_r32(G_LOD);
    W.frame_clear = (int)rt_r32(G_FCLEAR);
    W.frame_set = (int)rt_r32(G_FSET);
    W.visible = cb_visible;
    W.anim = cb_anim;
    W.user = NULL;
    W.slot_index = (int32_t)rt_r32(G_SLOT);
    W.group_count = (int32_t)rt_r32(G_GROUP);
    W.item_count = (int32_t)rt_r32(G_ITEMS);
    int32_t s0 = W.slot_index, i0 = W.item_count;
    if (s0 < 0 || s0 > DPC_WALK_MAX_SLOTS || i0 < 0 || i0 > DPC_WALK_MAX_ITEMS) rt_stop("dpc_walk: slot / item count out of range", (uint32_t)s0);
    rd_i32s(MATS[1] + 0x30u * (uint32_t)s0, W.mat[1][s0], 9);
    int r = dpc_walk(&W, root, obj, (const int32_t*)(void*)rt_ptr(obj + 0x14), (const int32_t*)(void*)rt_ptr(obj + 0x20),
                     (const int32_t*)(void*)rt_ptr(obj + 0x50), (const int32_t*)(void*)rt_ptr(obj + 0x80), (int32_t)rt_r32(G_SHIFT));
    for (int32_t i = i0; i < W.item_count; i++) {
        const DpcDrawItem* it = &W.items[i];
        uint32_t g = RLIST + 0x20u * (uint32_t)i;
        rt_w32(g, it->node);
        wr_i32s(g + 4, it->pos, 3);
        rt_w32(g + 0x10, (uint32_t)it->shift);
        rt_w32(g + 0x14, it->root);
        rt_w32(g + 0x18, (uint32_t)it->slot);
        rt_w32(g + 0x1c, (uint32_t)it->group);
    }
    wr_i32s(MATS[0] + 0x30u * (uint32_t)s0, W.mat[0][s0], 12);
    for (int32_t s = s0 + 1; s <= W.slot_index; s++)
        for (int k = 0; k < 4; k++) wr_i32s(MATS[k] + 0x30u * (uint32_t)s, W.mat[k][s], 9);
    for (int32_t s = s0; s <= W.slot_index; s++)
        for (int k = 0; k < 4; k++) rt_w32(PARENTS + 16u * (uint32_t)s + 4u * (uint32_t)k, mat_addr(W.parent[s][k]));
    rt_w32(G_SLOT, (uint32_t)W.slot_index);
    rt_w32(G_GROUP, (uint32_t)W.group_count);
    rt_w32(G_ITEMS, (uint32_t)W.item_count);
    if (r < 0) rt_stop("dpc_walk: pointer outside the address space", root);
    return 0;
}

void rt_bind_scene(void)
{
    rt_bind(0x0048d0b8u, b_dpc_collide, "dpc_collide");
    rt_bind(0x004a0cccu, b_dpc_trigger, "dpc_trigger");
    rt_bind(0x004bca88u, b_collide_grid_reject, "collide_grid_reject");
    rt_bind(0x0048b5a0u, b_vec_normalize_4096, "vec_normalize_4096");
    rt_bind(0x0049cec0u, b_car_collide, "car_collide");
    rt_bind(0x0049f59cu, b_dpc_walk, "dpc_walk");
}

RT_BIND_AREA(scene)
