/*
 * bind_engine_render.c: runtime bindings of the scene / renderer engine ports that take their
 * own context structs (not the T(...) harness): en_anim, en_clut, en_debris, en_ot, en_poly,
 * en_quad, en_split, en_sprite, en_submit. Each adapter does what the port's emulator diff
 * (tools/emu/diff_engine_<x>.py) did: the same registers / stack words, the context filled from
 * the original's globals (read before the call, written back after where the port keeps them in
 * the context), callbacks to the original's addresses through rt_call with the original's
 * registers (EAX, EDX, EBX, ECX, then the stack words).
 *
 * The ports' float math uses src/math tables (tm2_math_load_tables): the runtime has to load
 * them once at start up (sin / atan) before any of these run.
 *
 * Port code (not a decompiled function).
 */
#include "rt_env.h"

#include "engine/en_anim.h"
#include "engine/en_clut.h"
#include "engine/en_debris.h"
#include "engine/en_ot.h"
#include "engine/en_poly.h"
#include "engine/en_quad.h"
#include "engine/en_split.h"
#include "engine/en_sprite.h"
#include "engine/en_submit.h"

static uint32_t call1(uint32_t fn, uint32_t a0) { uint32_t v[1] = {a0}; return rt_call(fn, v, 1); }
static uint32_t call2(uint32_t fn, uint32_t a0, uint32_t a1) { uint32_t v[2] = {a0, a1}; return rt_call(fn, v, 2); }
static uint32_t call3(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2)
{ uint32_t v[3] = {a0, a1, a2}; return rt_call(fn, v, 3); }
static float f_of(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
/* host pointer of a game address, NULL for 0 (the ports' optional arguments) */
static void* opt(uint32_t a) { return a ? (void*)rt_ptr(a) : NULL; }

/* ---- en_anim ----------------------------------------------------------------------------- */

static int32_t anim_rand(void* u) { (void)u; return (int32_t)rt_call(0x004899c2u, NULL, 0); }
static void anim_trigger(void* u, uint32_t node, uint32_t mode) { (void)u; call2(0x004a0ccc, node, mode); }
static EnAnimEnv anim_env(void)
{
    EnAnimEnv e = {anim_rand, anim_trigger, NULL, rt_r16(0x00a81c48u)};
    return e;
}

/* 0x004a043c (EAX bounds, EDX radius, EBX pos, ECX mat_c, stack mat_d, shift) */
static uint32_t b_sphere_visible(const uint32_t* a, int n)
{
    EnCullEnv e = {(int32_t)rt_r32(0x00a81c3cu), (int32_t)rt_r32(0x004e8440u)};
    return (uint32_t)en_sphere_visible(&e, (const int32_t*)rt_ptr(rt_arg(a, n, 0)), (int32_t)rt_arg(a, n, 1),
                                       (const int32_t*)rt_ptr(rt_arg(a, n, 2)), (const int32_t*)rt_ptr(rt_arg(a, n, 3)),
                                       (const int32_t*)rt_ptr(rt_arg(a, n, 4)), (uint8_t)rt_arg(a, n, 5));
}
#define ANIM1(name, fn) \
    static uint32_t name(const uint32_t* a, int n) { EnAnimEnv e = anim_env(); fn(&e, rt_ptr(rt_arg(a, n, 0))); return 0; }
ANIM1(b_anim_loop, en_anim_loop)            /* 0x004a09f8 (EAX node) */
ANIM1(b_anim_pingpong, en_anim_pingpong)    /* 0x004a0a78 */
ANIM1(b_anim_random, en_anim_random)        /* 0x004a0b08 */
ANIM1(b_anim_weighted, en_anim_weighted)    /* 0x004a0b7c */
/* 0x004a0c1c (EAX node, EDX on) */
static uint32_t b_anim_delay(const uint32_t* a, int n)
{
    EnAnimEnv e = anim_env();
    en_anim_delay(&e, rt_ptr(rt_arg(a, n, 0)), rt_arg(a, n, 1) != 0);
    return 0;
}
/* 0x004a0c94 (EAX node or 0, EDX frame) */
static uint32_t b_anim_set_step(const uint32_t* a, int n)
{
    EnAnimEnv e = anim_env();
    en_anim_set_step(&e, opt(rt_arg(a, n, 0)), rt_arg(a, n, 1));
    return 0;
}

/* ---- en_clut ----------------------------------------------------------------------------- */

#define CLUT_LAST 0x004e842cu
static void clut_bad_node(void* u, int type)
{
    (void)u;   /* printf 0x004a1f1e("Expecting an object node for clut anim %d", type): cdecl stack words */
    uint32_t v[2] = {0x004e3088u, (uint32_t)type};
    rt_call(0x004a1f1eu, v, 2);
}
static EnClutEnv clut_env(void)
{
    EnClutEnv e = {rt_mem_cb, NULL, 0x00a3a5e0u, 0x00a3a838u, 0x00a30c48u, (int32_t)rt_r32(CLUT_LAST), clut_bad_node};
    return e;
}
static void clut_done(const EnClutEnv* e) { rt_w32(CLUT_LAST, (uint32_t)e->last); }

#define CLUT_R2(name, fn) static uint32_t name(const uint32_t* a, int n) \
    { EnClutEnv e = clut_env(); uint32_t r = fn(&e, rt_arg(a, n, 0), rt_arg(a, n, 1)); clut_done(&e); return r; }
#define CLUT_V3(name, fn) static uint32_t name(const uint32_t* a, int n) \
    { EnClutEnv e = clut_env(); fn(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), (int32_t)rt_arg(a, n, 2)); clut_done(&e); return 0; }
#define CLUT_V1(name, fn) static uint32_t name(const uint32_t* a, int n) \
    { EnClutEnv e = clut_env(); fn(&e, rt_arg(a, n, 0)); clut_done(&e); return 0; }
CLUT_R2(b_clut_new_def, en_clut_new_def)        /* 0x00499ee8 (EAX a, EDX b) */
CLUT_R2(b_clut_find_def, en_clut_find_def)      /* 0x00499f50 */
CLUT_R2(b_clut_find_slot, en_clut_find_slot)    /* 0x00499fa4 */
CLUT_V3(b_clut_start_sweep, en_clut_start_sweep)    /* 0x00499984 (EAX a, EDX b, EBX speed) */
CLUT_V3(b_clut_start_fade, en_clut_start_fade)      /* 0x00499a34 (EBX divisor) */
CLUT_V3(b_clut_start_cycle, en_clut_start_cycle)    /* 0x00499b00 */
CLUT_V1(b_clut_last_differing, en_clut_last_differing)  /* 0x0049a020 (EAX slot) */
CLUT_V1(b_clut_step_sweep, en_clut_step_sweep)  /* 0x00499cd4 */
CLUT_V1(b_clut_step_fade, en_clut_step_fade)    /* 0x00499d80 */
CLUT_V1(b_clut_step_cycle, en_clut_step_cycle)  /* 0x00499e08 */
static uint32_t b_clut_stop_fade(const uint32_t* a, int n)   /* 0x00499ae8 (EAX a, EDX b) */
{
    EnClutEnv e = clut_env();
    en_clut_stop_fade(&e, rt_arg(a, n, 0), rt_arg(a, n, 1));
    clut_done(&e);
    return 0;
}
static uint32_t b_clut_free_slot(const uint32_t* a, int n)   /* 0x00499ff0 */
{
    (void)a; (void)n;
    EnClutEnv e = clut_env();
    uint32_t r = en_clut_free_slot(&e);
    clut_done(&e);
    return r;
}
static uint32_t b_clut_register(const uint32_t* a, int n)    /* 0x00499b9c (EAX node, EDX kind) */
{
    EnClutEnv e = clut_env();
    en_clut_register(&e, rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1));
    clut_done(&e);
    return 0;
}
static uint32_t b_clut_update(const uint32_t* a, int n)      /* 0x00499c74 */
{
    (void)a; (void)n;
    EnClutEnv e = clut_env();
    en_clut_update(&e);
    clut_done(&e);
    return 0;
}

/* ---- en_debris --------------------------------------------------------------------------- */

#define DEBRIS_DATA 0x009f6d8cu
_Static_assert(sizeof(EnDebrisData) == 0xa0c, "EnDebrisData is the original's block byte for byte");

static uint8_t* dbr_entity(void* u, uint32_t e) { (void)u; return rt_ptr(e); }
static uint32_t dbr_alloc(void* u) { (void)u; return rt_call(0x00492d90u, NULL, 0); }
static void dbr_list_add(void* u, uint32_t e) { (void)u; call1(0x00492c4cu, e); }
static void dbr_set_draw(void* u, uint32_t e, int on) { (void)u; call2(0x00492e50u, e, (uint32_t)on); }
static void dbr_draw_unculled(void* u, uint32_t e) { (void)u; call1(0x004a1194u, e); }
static void dbr_set_model(void* u, uint32_t e, uint32_t m) { (void)u; call2(0x00492de4u, e, m); }
static const uint8_t* dbr_node(void* u, uint32_t p) { (void)u; return rt_ptr(p); }
static EnDebrisEnv dbr_env(void)
{
    EnDebrisEnv e = {(EnDebrisData*)(void*)rt_ptr(DEBRIS_DATA), 0x009f6eb4u, NULL, dbr_entity, dbr_alloc, dbr_list_add,
                     dbr_set_draw, dbr_draw_unculled, anim_rand, dbr_set_model, dbr_node};
    return e;
}
static uint32_t b_debris_init(const uint32_t* a, int n) { (void)a; (void)n; EnDebrisEnv e = dbr_env(); en_debris_init(&e); return 0; }
static uint32_t b_debris_clear(const uint32_t* a, int n) { (void)a; (void)n; EnDebrisEnv e = dbr_env(); en_debris_clear(&e); return 0; }
static uint32_t b_debris_update(const uint32_t* a, int n) { (void)a; (void)n; EnDebrisEnv e = dbr_env(); en_debris_update(&e); return 0; }
/* 0x0048a8b8 (EAX ent, EDX dir, EBX spin or 0, ECX hold, stack scale, end, random spin, drift or 0) */
static uint32_t b_debris_start_burst(const uint32_t* a, int n)
{
    EnDebrisEnv e = dbr_env();
    en_debris_start_burst(&e, rt_arg(a, n, 0), (const int16_t*)rt_ptr(rt_arg(a, n, 1)), opt(rt_arg(a, n, 2)),
                          (int32_t)rt_arg(a, n, 3), (int32_t)rt_arg(a, n, 4), (int32_t)rt_arg(a, n, 5),
                          (int32_t)rt_arg(a, n, 6), opt(rt_arg(a, n, 7)));
    return 0;
}
/* 0x0048a738 (EAX node or 0, EDX pos, EBX dir, ECX spin or 0, stack drift or 0, hold, scale, end, random spin) */
static uint32_t b_debris_burst_at(const uint32_t* a, int n)
{
    EnDebrisEnv e = dbr_env();
    uint32_t node = rt_arg(a, n, 0);
    en_debris_burst_at(&e, node, opt(node), (const int32_t*)rt_ptr(rt_arg(a, n, 1)),
                       (const int16_t*)rt_ptr(rt_arg(a, n, 2)), opt(rt_arg(a, n, 3)), opt(rt_arg(a, n, 4)),
                       (int32_t)rt_arg(a, n, 5), (int32_t)rt_arg(a, n, 6), (int32_t)rt_arg(a, n, 7), (int32_t)rt_arg(a, n, 8));
    return 0;
}
/* 0x0048a488 (EAX ent, EDX node or 0, EBX target, ECX dir, stack scale, life) */
static uint32_t b_debris_break_model(const uint32_t* a, int n)
{
    EnDebrisEnv e = dbr_env();
    en_debris_break_model(&e, rt_arg(a, n, 0), opt(rt_arg(a, n, 1)), (const int32_t*)rt_ptr(rt_arg(a, n, 2)),
                          (const int16_t*)rt_ptr(rt_arg(a, n, 3)), (int32_t)rt_arg(a, n, 4), (int32_t)rt_arg(a, n, 5));
    return 0;
}
/* 0x0048b3f8 (EAX items, EDX count, EBX matrices) */
static uint32_t b_debris_capture(const uint32_t* a, int n)
{
    EnDebrisEnv e = dbr_env();
    en_debris_capture(&e, (const DpcDrawItem*)(const void*)rt_ptr(rt_arg(a, n, 0)), (int32_t)rt_arg(a, n, 1),
                      (const int32_t(*)[12])(const void*)rt_ptr(rt_arg(a, n, 2)));
    return 0;
}

/* ---- en_ot ------------------------------------------------------------------------------- */

#define DISPLAY 0x00c84840u
static void ot_lock(void* u) { (void)u; rt_call(0x004961b8u, NULL, 0); }     /* 0x004b4418(display, 0, 0, 0, 0) */
static void ot_unlock(void* u) { (void)u; rt_call(0x004961d4u, NULL, 0); }   /* 0x004b44e4(display, 0, 0) */
static void ot_dispatch(void* u, uint32_t p) { (void)u; call1(0x004ad874u, p); }
static void ot_hw_prim(void* u, uint32_t p) { (void)u; call2(0x004b67d0u, DISPLAY, p); }
static void ot_hw_flush(void* u) { (void)u; call1(0x004b6780u, DISPLAY); }
static EnOtEnv ot_env(void)
{
    EnOtEnv e = {rt_base, 0, RT_SIZE, rt_r32(0x00a2dbb0u), (int32_t)rt_r32(0x00c85008u),
                 ot_lock, ot_unlock, ot_dispatch, ot_hw_prim, ot_hw_flush, NULL};
    return e;
}
static uint32_t b_ot_insert(const uint32_t* a, int n)   /* 0x00496680 (EAX slot, EDX record) */
{
    EnOtEnv e = ot_env();
    en_ot_insert(&e, rt_arg(a, n, 0), rt_arg(a, n, 1));
    return 0;
}
#define OT1(name, fn) static uint32_t name(const uint32_t* a, int n) { EnOtEnv e = ot_env(); fn(&e, rt_arg(a, n, 0)); return 0; }
OT1(b_add_prim, en_add_prim)        /* 0x004967f4 (EAX prim) */
OT1(b_prim_queue, en_prim_queue)    /* 0x004963b4 */
OT1(b_ot_flush, en_ot_flush)        /* 0x0049681c (EAX ot) */
OT1(b_ot_flush_hw, en_ot_flush_hw)  /* 0x00496380 */

/* ---- shared: clip buffers in game memory -------------------------------------------------- */

/* a clip buffer (count, 16 vertex pointers) at game address `at`, as host pointers */
static void clip_in(EnClipBuf* c, uint32_t at)
{
    c->count = (int32_t)rt_r32(at);
    for (int i = 0; i < EN_CLIP_MAX; i++) {
        uint32_t v = rt_r32(at + 4 + 4u * (uint32_t)i);
        c->v[i] = v ? (EnVertex*)(void*)rt_ptr(v) : NULL;
    }
}

/* ---- en_quad ----------------------------------------------------------------------------- */

#define Q_VTX 0x009f78b0u
#define Q_CLIPA 0x009f7ddcu
static void rf(uint32_t a, float* f) { uint32_t u = rt_r32(a); memcpy(f, &u, 4); }
static void wf(uint32_t a, float f) { uint32_t u; memcpy(&u, &f, 4); rt_w32(a, u); }
static void quad_in(EnQuadEnv* q)
{
    EnClipBuf c;
    clip_in(&c, Q_CLIPA);
    q->vtx = (EnVertex*)(void*)rt_ptr(Q_VTX);
    q->vert_count = c.count;
    memcpy(q->ptr, c.v, sizeof q->ptr);
    rf(0x009f7d14u, &q->tex_inv); rf(0x009f7ce0u, &q->fov_x);
    rf(0x009f7d00u, &q->size_x); rf(0x009f7d30u, &q->size_y);
    rf(0x009f7d1cu, &q->zmax); rf(0x009f7d2cu, &q->zmin);
    q->render_flags = rt_r8(0x00c846d8u);
    q->poly = rt_ptr(rt_r32(0x009f7cf4u));
    q->verts = rt_ptr(rt_r32(0x009f7cc4u));
    q->wave_a = (int32_t)rt_r32(0x009f7d04u);
    q->wave_b = (int32_t)rt_r32(0x009f7d08u);
}
static void quad_out(const EnQuadEnv* q)
{
    rt_w32(Q_CLIPA, (uint32_t)q->vert_count);
    for (int i = 0; i < EN_CLIP_MAX; i++) rt_w32(Q_CLIPA + 4 + 4u * (uint32_t)i, q->ptr[i] ? rt_addr(q->ptr[i]) : 0);
    wf(0x009f7d00u, q->size_x); wf(0x009f7d30u, q->size_y);
    wf(0x009f7d1cu, q->zmax); wf(0x009f7d2cu, q->zmin);
    rt_w8(0x00c846d8u, q->render_flags);
}
static uint32_t b_billboard_quad(const uint32_t* a, int n)   /* 0x00490780 (AL result) */
{
    (void)a; (void)n;
    EnQuadEnv q; quad_in(&q);
    uint32_t r = (uint32_t)en_billboard_quad(&q);
    quad_out(&q);
    return r;
}
static uint32_t b_line_segment_quad(const uint32_t* a, int n)  /* 0x004908b0 */
{
    (void)a; (void)n;
    EnQuadEnv q; quad_in(&q);
    uint32_t r = (uint32_t)en_line_segment_quad(&q);
    quad_out(&q);
    return r;
}
static uint32_t b_water_uv_animate(const uint32_t* a, int n)   /* 0x00490e74 */
{
    (void)a; (void)n;
    EnQuadEnv q; quad_in(&q);
    en_water_uv_animate(&q);
    quad_out(&q);
    return 0;
}
/* 0x00492914 (stack z, z0, z1 as float bits, c0, c1; ret 0x14), no register arguments: a[0 .. 4] */
static uint32_t b_color_interpolate(const uint32_t* a, int n)
{
    return en_color_interpolate(f_of(rt_arg(a, n, 0)), f_of(rt_arg(a, n, 1)), f_of(rt_arg(a, n, 2)),
                                rt_arg(a, n, 3), rt_arg(a, n, 4));
}

/* ---- en_submit --------------------------------------------------------------------------- */

#define G_CLIPPTR 0x004e8290u
#define G_CURSOR 0x009f7d78u
static void sub_ot(void* u, int32_t bucket, uint32_t rec)
{
    uint32_t ot = *(const uint32_t*)u;   /* *param_1 */
    call2(0x00496680u, ot + 4u * (uint32_t)bucket, rec);
}
static void sub_normalize(void* u, EnVertex* v) { (void)u; call1(0x0048c890u, rt_addr(v)); }

static void submit_in(EnSubmitEnv* s, EnClipBuf* clip, uint32_t* ot)
{
    memset(s, 0, sizeof *s);
    clip_in(clip, rt_r32(G_CLIPPTR));
    s->clip = clip;
    s->tri = (EnVertex*)(void*)rt_ptr(Q_VTX);
    s->color = rt_r32(0x009f7d54u);
    s->depth = (int32_t)rt_r32(0x009f7d58u);
    s->depth_fixed = (int32_t)rt_r32(0x009f7d48u);
    s->depth_bias = (int32_t)rt_r32(0x009f7d50u);
    s->render_flags = rt_r16(0x00c846d8u);
    s->page_ref = rt_r32(0x009f7e30u);
    s->fog = (int32_t)rt_r32(0x009f7d28u);
    rf(0x009f7d20u, &s->fog_dist);
    rf(0x009f7d2cu, &s->zmin); rf(0x009f7d1cu, &s->zmax);
    rf(0x009f7d6cu, &s->fog_sub);
    s->normalize = (int32_t)rt_r32(0x009f7d80u);
    s->format = rt_r8(0x00a2dba5u);
    rf(0x009f7d6cu, &s->q[0]); rf(0x009f7d74u, &s->q[1]); rf(0x009f7d68u, &s->q[2]);
    s->light_index = (int32_t)rt_r32(0x009f7cccu);
    s->lights = rt_ptr(0x00c85c00u);
    s->prim = rt_base; s->prim_addr = 0;   /* the primitives are in game memory */
    s->cursor = rt_r32(G_CURSOR);
    s->end = rt_r32(0x009f7cc8u);
    s->ot_insert = sub_ot;
    s->normalize_fn = sub_normalize;
    s->user = ot;
}

/* 0x0048f798 (EAX depth) */
static uint32_t b_depth_bucket(const uint32_t* a, int n)
{
    EnSubmitEnv s; EnClipBuf c; uint32_t ot = 0;
    submit_in(&s, &c, &ot);
    return (uint32_t)en_depth_bucket(&s, (int32_t)rt_arg(a, n, 0));
}
/* 0x0048f838 render_submit_effect: writes at the cursor, record 0x009f7ce4 */
static uint32_t b_submit_gradients(const uint32_t* a, int n)
{
    (void)a; (void)n;
    EnSubmitEnv s; EnClipBuf c; uint32_t ot = 0;
    submit_in(&s, &c, &ot);
    s.rec = rt_r32(0x009f7ce4u);
    en_submit_gradients(&s);
    rt_w32(0x009f7ce4u, s.rec);
    return 0;
}
/* 0x0048fd54 render_submit_nonplit (EAX the display context: +0 ordering table) */
static uint32_t b_submit_prim(const uint32_t* a, int n)
{
    EnSubmitEnv s; EnClipBuf c;
    uint32_t ot = rt_r32(rt_arg(a, n, 0));
    submit_in(&s, &c, &ot);
    uint32_t cur = s.cursor;
    en_submit_prim(&s);
    rt_w32(G_CURSOR, s.cursor);
    rt_w32(0x009f7cc8u, s.end);
    switch (rt_r8(cur + 7)) {   /* the record pointer the original keeps per primitive code */
    case 0x40: rt_w32(0x009f7cd0u, s.rec); break;
    case 0x90: rt_w32(0x009f7d0cu, s.rec); break;
    case 0xa0: rt_w32(0x009f7ce4u, s.rec); break;
    case 0xb0: rt_w32(0x009f7d24u, s.rec); break;
    default: break;
    }
    return 0;
}

/* ---- en_split ---------------------------------------------------------------------------- */

static void split_ot(void* u, uint32_t slot, uint32_t rec) { (void)u; call2(0x004966ccu, slot, rec); }
/* 0x00491018 display_cmd_build (EAX the display context: +0 ordering table) */
static uint32_t b_display_cmd_build(const uint32_t* a, int n)
{
    EnClipBuf c;
    clip_in(&c, rt_r32(G_CLIPPTR));
    EnSplitEnv e;
    memset(&e, 0, sizeof e);
    e.clip = &c;
    e.shade = rt_r32(0x009f7d34u);
    e.color = rt_r32(0x009f7d54u);
    e.semi = (int32_t)rt_r32(0x009f7d38u);
    e.blend = rt_r32(0x009f7d7cu);
    e.render_flags = rt_r16(0x00c846d8u);
    e.page_ref = rt_r32(0x009f7e30u);
    e.depth = (int32_t)rt_r32(0x009f7d58u);
    e.depth_fixed = (int32_t)rt_r32(0x009f7d48u);
    e.depth_bias = (int32_t)rt_r32(0x009f7d50u);
    e.tex_table = rt_ptr(0x00a30c48u);
    e.tex_count = 512;
    e.prim = rt_base; e.prim_addr = 0;
    e.cursor = rt_r32(G_CURSOR);
    e.end = rt_r32(0x009f7cd4u);
    e.ot_insert = split_ot;
    e.ot_base = rt_r32(rt_arg(a, n, 0));
    uint32_t cur = e.cursor;
    en_split_prim(&e);
    rt_w32(G_CURSOR, e.cursor);
    rt_w32(0x009f7cd4u, e.end);
    rt_w32(0x009f7d54u, e.color);
    rt_w16(0x00c846d8u, e.render_flags);
    switch (rt_r8(cur + 7) & 0xfd) {
    case 0x40: rt_w32(0x009f7cd0u, e.rec); break;
    case 0x50: rt_w32(0x009f7cf0u, e.rec); break;
    case 0x80: rt_w32(0x009f7cd8u, e.rec); break;
    default: break;
    }
    return 0;
}
/* 0x004966cc depth_sort_insert (EAX slot, EDX record) */
static uint32_t b_ot_link(const uint32_t* a, int n)
{
    uint32_t slot = rt_arg(a, n, 0), rec = rt_arg(a, n, 1);
    uint32_t s = rt_r32(slot), r = rt_r32(rec);
    en_ot_link(&s, &r, rec);
    rt_w32(slot, s); rt_w32(rec, r);
    return 0;
}

/* ---- en_poly ----------------------------------------------------------------------------- */

#define P_G0 0x009f7700u
#define P_POOL 0x009f78b0u
#define P_CLIPB 0x009f7d84u
#define P_CLIPA 0x009f7ddcu

/* the 32 bit globals of the block that are EnPoly state fields (tools/emu/diff_engine_poly_shim.c) */
#define POLY_FIELDS(X) \
    X(0x9f7cc0, tex_shift) X(0x9f7cc4, verts) X(0x9f7ccc, light_index) X(0x9f7ce0, fov_x) X(0x9f7ce8, fov_y) \
    X(0x9f7cec, inv_fov_y) X(0x9f7cf8, inv_fov_x) X(0x9f7cf4, poly) X(0x9f7cfc, far_clip) X(0x9f7d00, size_x) \
    X(0x9f7d04, wave_a) X(0x9f7d08, wave_b) X(0x9f7d10, fog_rgb_copy) X(0x9f7d14, tex_inv) X(0x9f7d18, kind) \
    X(0x9f7d1c, zmax) X(0x9f7d20, fog_dist) X(0x9f7d28, fog) X(0x9f7d2c, zmin) X(0x9f7d30, size_y) \
    X(0x9f7d34, shade) X(0x9f7d38, semi) X(0x9f7d3c, node8) X(0x9f7d44, interp_mask) X(0x9f7d48, depth_fixed) \
    X(0x9f7d4c, item) X(0x9f7d50, depth_bias) X(0x9f7d54, color) X(0x9f7d58, depth) X(0x9f7d64, normals) \
    X(0x9f7d68, q[2]) X(0x9f7d6c, q[0]) X(0x9f7d70, bits19) X(0x9f7d74, q[1]) X(0x9f7d78, cursor) \
    X(0x9f7d7c, blend) X(0x9f7d80, clipped) X(0x9f7e30, page_ref)

static EnPoly P;   /* one renderer pass at a time (its state is the original's globals) */

static EnVertex* poly_vin(uint32_t a)
{
    if (a >= P_POOL && a < P_POOL + 0x34u * EN_POLY_POOL && (a - P_POOL) % 0x34 == 0) return &P.vtx[(a - P_POOL) / 0x34];
    return a ? (EnVertex*)(void*)rt_ptr(a) : NULL;
}
static uint32_t poly_vout(const EnVertex* v)
{
    if (v >= P.vtx && v < P.vtx + EN_POLY_POOL) return P_POOL + 0x34u * (uint32_t)(v - P.vtx);
    return v ? rt_addr(v) : 0;
}
static void poly_bufs(int out)
{
    EnClipBuf* l[2] = {&P.b, &P.a};
    uint32_t at[2] = {P_CLIPB, P_CLIPA};
    for (int k = 0; k < 2; k++)
        for (int i = -1; i < EN_CLIP_MAX; i++) {
            uint32_t w = at[k] + 4 + 4u * (uint32_t)i;
            if (i < 0) { if (out) rt_w32(at[k], (uint32_t)l[k]->count); else l[k]->count = (int32_t)rt_r32(at[k]); }
            else if (out) rt_w32(w, poly_vout(l[k]->v[i]));
            else l[k]->v[i] = poly_vin(rt_r32(w));
        }
}
static void poly_ot_insert(void* u, int32_t bucket, uint32_t rec)
{
    (void)u;   /* 0x00496680 takes the slot address */
    call2(0x00496680u, P.ot_base + 4u * (uint32_t)bucket, rec);
}
static void poly_ot_link(void* u, uint32_t slot, uint32_t rec) { (void)u; call2(0x004966ccu, slot, rec); }

static void poly_in(uint32_t ctx)
{
    memset(&P, 0, sizeof P);
    P.mem = rt_mem_cb; P.user = NULL;
    /* inputs the original reads through small getters */
    uint32_t vp = 0x00a03ba8u + 0x94u * rt_r32(0x00a04138u);   /* level_get_viewport_data 0x00493880 */
    P.view_w = (int32_t)rt_r32(vp);
    P.view_h = (int32_t)rt_r32(vp + 8);
    P.elapsed = rt_call(0x00493038u, NULL, 0);                     /* timer_get_elapsed */
    P.fog_rgb = (uint32_t)rt_r8(0x00a03b64u) << 16 | (uint32_t)rt_r8(0x00a03b68u) << 8 | rt_r8(0x00a03b6cu);   /* 0x00494dc0 */
    P.camera_obj = rt_r32(0x00a03accu);                            /* 0x00494c48(0) */
    P.split = rt_r32(0x00c85008u) != 0;                            /* 0x004958ac */
    P.format = rt_r8(0x00a2dba5u);
    memcpy(&P.fov_base, rt_ptr(0x004e2d6cu), 8);
    memcpy(&P.fog_k2, rt_ptr(0x004e2d74u), 8);
    P.special_mode = (int32_t)rt_r32(0x004e82ccu);
    P.fog_mode = (int32_t)rt_r32(0x004e82b0u);
    P.fog_on = (int32_t)rt_r32(0x004e85f8u);
    P.depth_scale = rt_r32(0x004e82b4u);
    P.alpha_test = (int32_t)rt_r32(0x004e82b8u);
    rf(0x004e82bcu, &P.alpha_z);
    rf(0x004e85fcu, &P.fog_k);
    P.fog_color = rt_r32(0x004e8608u);
    P.q_enable = (int32_t)rt_r32(0x004e8e10u);
    P.tex_table = rt_ptr(0x00a30c48u); P.tex_count = 512;
    P.lights = rt_ptr(0x00c85c00u);
    memcpy(&P.proj, rt_ptr(0x009f7810u), sizeof P.proj);
    /* state */
    memcpy(&P.xf, rt_ptr(0x009f77d0u), sizeof P.xf);
    memcpy(P.vtx, rt_ptr(P_POOL), sizeof P.vtx);
#define P_IN(a, f) memcpy(&P.f, rt_ptr(a), 4);
    POLY_FIELDS(P_IN)
#undef P_IN
    poly_bufs(0);
    P.b_is_a = rt_r32(G_CLIPPTR) == P_CLIPA;
    P.render_flags = rt_r32(0x00c846d8u);
    /* the display context: +0 ordering table, +0x8008 primitive cursor (copied to 0x009f7d78) */
    P.prim = rt_base; P.prim_addr = 0;
    P.ot_base = rt_r32(ctx);
    P.cursor = rt_r32(ctx + 0x8008);
    P.ot_insert = poly_ot_insert; P.ot_link = poly_ot_link;
    P.fog_q = NULL;
}
static void poly_out(uint32_t ctx)
{
#define P_OUT(a, f) memcpy(rt_ptr(a), &P.f, 4);
    POLY_FIELDS(P_OUT)
#undef P_OUT
    memcpy(rt_ptr(0x009f77d0u), &P.xf, sizeof P.xf);
    memcpy(rt_ptr(P_POOL), P.vtx, sizeof P.vtx);
    poly_bufs(1);
    rt_w32(G_CLIPPTR, P.b_is_a ? P_CLIPA : P_CLIPB);
    rt_w32(0x00c846d8u, P.render_flags);
    rt_w32(ctx + 0x8008, P.cursor);
}
/* 0x0048e9b8 polygon_renderer (EAX ctx, EDX items, EBX count, ECX matrices, one stack word unused) */
static uint32_t b_polygon_renderer(const uint32_t* a, int n)
{
    uint32_t ctx = rt_arg(a, n, 0);
    poly_in(ctx);
    en_poly_render(&P, rt_arg(a, n, 1), (int32_t)rt_arg(a, n, 2), rt_arg(a, n, 3));
    poly_out(ctx);
    return 0;
}
/* 0x00489d20 fxDebrisDraw (EAX ctx): the bursts 0x009f6eb4, the scene root [0x009f7e38] */
static uint32_t b_debris_draw(const uint32_t* a, int n)
{
    uint32_t ctx = rt_arg(a, n, 0);
    poly_in(ctx);
    en_poly_debris(&P, 0x009f6eb4u, rt_r32(0x009f7e38u));
    poly_out(ctx);
    return 0;
}

/* ---- en_sprite --------------------------------------------------------------------------- */

static void spr_add_prim(void* u, uint32_t p) { (void)u; call1(0x004967f4u, p); }
static EnSpriteEnv spr_env(void) { EnSpriteEnv e = {rt_mem_cb, NULL, spr_add_prim}; return e; }
#define SPR_R3(name, fn) static uint32_t name(const uint32_t* a, int n) \
    { EnSpriteEnv e = spr_env(); return (uint32_t)fn(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2)); }
SPR_R3(b_sprite_parse_rects, en_sprite_parse_rects)   /* 0x00496d04 (EAX node, EDX out, EBX max) */
SPR_R3(b_sprite_parse_2d, en_sprite_parse_2d)         /* 0x00496da0 */
#define SPR_V3(name, fn) static uint32_t name(const uint32_t* a, int n) \
    { EnSpriteEnv e = spr_env(); fn(&e, (int32_t)rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2)); return 0; }
SPR_V3(b_sprite_print_number, en_sprite_print_number)     /* 0x00496dcc (EAX value, EDX prims, EBX rects) */
SPR_V3(b_sprite_print_number2, en_sprite_print_number2)   /* 0x00496ee8 */
static uint32_t b_sprite_get_a36c54(const uint32_t* a, int n)   /* 0x00496fac */
{
    (void)a; (void)n;
    EnSpriteEnv e = spr_env();
    return en_sprite_get_a36c54(&e);
}

/* 0x00498b38 (EAX path, EDX &size): the file size goes to EDX, which Load4BitClut points at a
 * stack local it never reads (the port's open takes only the path): a frame word here */
static uint32_t io_open(void* u, uint32_t path)
{
    (void)u;
    uint32_t f = rt_frame_push(16), r = call2(0x00498b38u, path, f);
    rt_frame_pop(16);
    return r;
}
/* 0x00498bb8 (EAX h, EDX buf, EBX n): the port reads its two header words into host locals (the
 * original's stack locals): those go through a frame */
static uint32_t io_read(void* u, uint32_t h, uint8_t* dst, uint32_t k)
{
    (void)u;
    if (dst >= rt_base && dst < rt_base + RT_SIZE) return call3(0x00498bb8u, h, rt_addr(dst), k);
    uint32_t f = rt_frame_push((k + 15) & ~15u);
    uint32_t r = call3(0x00498bb8u, h, f, k);
    memcpy(dst, rt_ptr(f), k);
    rt_frame_pop((k + 15) & ~15u);
    return r;
}
static void io_close(void* u, uint32_t h) { (void)u; call1(0x00498cb0u, h); }
/* 0x004960bc Load4BitClut (EAX path, EDX &w, EBX &h) */
static uint32_t b_load_4bit_clut(const uint32_t* a, int n)
{
    EnSpriteEnv e = spr_env();
    EnFileIo io = {io_open, io_read, io_close};
    return en_load_4bit_clut(&e, &io, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2));
}

/* 0x00495b8c viewport_outline (AL r, DL g, BL b) */
static int ol_split(void* u) { (void)u; return rt_r32(0x00c85008u) != 0; }   /* 0x004958ac */
static void ol_clear(void* u, int r, int g, int b) { (void)u; call3(0x004ad624u, (uint32_t)r, (uint32_t)g, (uint32_t)b); }
static void ol_viewport(void* u, int32_t out[4])
{
    (void)u;
    uint32_t f = rt_frame_push(0x10);
    call2(0x0049392cu, 0, f);
    memcpy(out, rt_ptr(f), 0x10);
    rt_frame_pop(0x10);
}
/* the original's primitive is a stack record: here a frame, passed to AddPrim by address */
static void ol_add_prim(void* u, const uint8_t* prim)
{
    (void)u;
    uint32_t f = rt_frame_push(0x10);
    memcpy(rt_ptr(f), prim, 0x10);
    call1(0x004967f4u, f);
    rt_frame_pop(0x10);
}
static uint32_t b_viewport_outline(const uint32_t* a, int n)
{
    EnOutlineEnv e = {ol_split, ol_clear, ol_viewport, ol_add_prim};
    en_viewport_outline(&e, NULL, (uint8_t)rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1), (uint8_t)rt_arg(a, n, 2));
    return 0;
}

void rt_bind_engine_render(void)
{
    rt_bind(0x004a043cu, b_sphere_visible, "csSphereVisible");
    rt_bind(0x004a09f8u, b_anim_loop, "anim_step_loop");
    rt_bind(0x004a0a78u, b_anim_pingpong, "anim_step_pingpong");
    rt_bind(0x004a0b08u, b_anim_random, "anim_step_random");
    rt_bind(0x004a0b7cu, b_anim_weighted, "anim_step_weighted");
    rt_bind(0x004a0c1cu, b_anim_delay, "anim_pick_delay");
    rt_bind(0x004a0c94u, b_anim_set_step, "animSetStep");

    rt_bind(0x00499984u, b_clut_start_sweep, "clutAnimStartSweep");
    rt_bind(0x00499a34u, b_clut_start_fade, "clutAnimStartFade");
    rt_bind(0x00499ae8u, b_clut_stop_fade, "clutAnimStopFade");
    rt_bind(0x00499b00u, b_clut_start_cycle, "clutAnimStartCycle");
    rt_bind(0x00499b9cu, b_clut_register, "clutAnimRegister");
    rt_bind(0x00499c74u, b_clut_update, "clutAnimUpdate");
    rt_bind(0x00499cd4u, b_clut_step_sweep, "clutAnimStepSweep");
    rt_bind(0x00499d80u, b_clut_step_fade, "clutAnimStepFade");
    rt_bind(0x00499e08u, b_clut_step_cycle, "clutAnimStepCycle");
    rt_bind(0x00499ee8u, b_clut_new_def, "clutAnimNewDef");
    rt_bind(0x00499f50u, b_clut_find_def, "clutAnimFindDef");
    rt_bind(0x00499fa4u, b_clut_find_slot, "clutAnimFindSlot");
    rt_bind(0x00499ff0u, b_clut_free_slot, "clutAnimFreeSlot");
    rt_bind(0x0049a020u, b_clut_last_differing, "clutAnimLastDiffering");

    rt_bind(0x00489a00u, b_debris_init, "debrisInit");
    rt_bind(0x00489a70u, b_debris_clear, "debrisClear");
    rt_bind(0x00489ab0u, b_debris_update, "debrisUpdateChunks");
    rt_bind(0x0048a488u, b_debris_break_model, "debrisBreakModel");
    rt_bind(0x0048a738u, b_debris_burst_at, "debrisBurstAt");
    rt_bind(0x0048a8b8u, b_debris_start_burst, "debrisStartBurst");
    rt_bind(0x0048b3f8u, b_debris_capture, "debrisCaptureModel");

    rt_bind(0x00496380u, b_ot_flush_hw, "display_list_flush_hw");
    rt_bind(0x004963b4u, b_prim_queue, "prim_queue");
    rt_bind(0x00496680u, b_ot_insert, "ot_insert");
    rt_bind(0x004967f4u, b_add_prim, "AddPrim");
    rt_bind(0x0049681cu, b_ot_flush, "display_list_flush");

    rt_bind(0x00489d20u, b_debris_draw, "fxDebrisDraw");
    rt_bind(0x0048e9b8u, b_polygon_renderer, "polygon_renderer");

    rt_bind(0x00490780u, b_billboard_quad, "billboard_quad");
    rt_bind(0x004908b0u, b_line_segment_quad, "line_segment_quad");
    rt_bind(0x00490e74u, b_water_uv_animate, "water_uv_animate");
    rt_bind(0x00492914u, b_color_interpolate, "color_interpolate");

    rt_bind(0x00491018u, b_display_cmd_build, "display_cmd_build");
    rt_bind(0x004966ccu, b_ot_link, "depth_sort_insert");

    rt_bind(0x00495b8cu, b_viewport_outline, "viewport_outline");
    rt_bind(0x004960bcu, b_load_4bit_clut, "Load4BitClut");
    rt_bind(0x00496d04u, b_sprite_parse_rects, "grutilsParseSpriteRects");
    rt_bind(0x00496da0u, b_sprite_parse_2d, "grutilsParse2DSprite");
    rt_bind(0x00496dccu, b_sprite_print_number, "fontSpritePrintCenteredXY");
    rt_bind(0x00496ee8u, b_sprite_print_number2, "fontSpritePrintNumber");
    rt_bind(0x00496facu, b_sprite_get_a36c54, "get_a36c54");

    rt_bind(0x0048f798u, b_depth_bucket, "depth_bucket");
    rt_bind(0x0048f838u, b_submit_gradients, "render_submit_effect");
    rt_bind(0x0048fd54u, b_submit_prim, "render_submit_nonplit");
}

RT_BIND_AREA(engine_render)
