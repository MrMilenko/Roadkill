/*
 * bind_engine_view.c: runtime bindings for the engine view / camera / math / clip / fog / registry
 * / scene ports (src/engine/en_view.c, en_view2.c, en_camera.c, en_math.c, en_clip.c, en_fog.c,
 * en_reg.c, en_scene.c). Each adapter calls the port the way its tools/emu/diff_engine_<x>.py did.
 *
 * These are the older ports with host pointer contexts: a context whose layout matches the
 * original's globals is given rt_ptr(<its address>) directly; one that does not (EnViewSize,
 * EnReg, EnFogEnv, EnClipEnv, EnScene + DpcWalk) is copied in from game memory before the call
 * and its written fields copied back after (and around every callback that leaves the port).
 *
 * Math tables: en_view (light directions), en_fog (shimmer), en_scene (matrices) use src/math
 * (tm2_sin, tm2_matrix_*), whose tables come from tm2_math_load_tables(); the runtime core must
 * load them (SINTAB.DAT + TM2.EXE) before these run.
 */
#include "rt_env.h"
#include "engine/en_anim.h"
#include "engine/en_camera.h"
#include "engine/en_clip.h"
#include "engine/en_fog.h"
#include "engine/en_math.h"
#include "engine/en_reg.h"
#include "engine/en_scene.h"
#include "engine/en_view.h"
#include "engine/en_view2.h"

#define A(i) rt_arg(a, n, (i))
#define HW() ((int)rt_r32(0x00c85008u))   /* 3D device mode */

static EnViewport* vp_g(void) { return (EnViewport*)(void*)rt_ptr(0x009f7800u); }
static EnLights* lights_g(void) { return (EnLights*)(void*)rt_ptr(0x00a04178u); }
static EnViewBlock* vblk(void) { return (EnViewBlock*)(void*)rt_ptr(0x00a03a98u); }
static int32_t* I32(uint32_t addr) { return (int32_t*)(void*)rt_ptr(addr); }
static EnMem whole_mem(void) { EnMem m = {rt_base, 0, RT_SIZE}; return m; }

/* ---- en_view.c ---------------------------------------------------------------------------- */

/* 0x0048bb10 viewport_set_origin (EAX x, EDX y) */
static uint32_t b_vp_origin(const uint32_t* a, int n) { en_viewport_set_origin(vp_g(), (int32_t)A(0), (int32_t)A(1), HW()); return 0; }
/* 0x0048bb94 (EAX w) */
static uint32_t b_vp_width(const uint32_t* a, int n) { en_viewport_set_width(vp_g(), (int32_t)A(0), HW()); return 0; }
/* 0x00493880 level_get_viewport_data: cur = [0x00a04138] */
static uint32_t b_vp_data(const uint32_t* a, int n) { (void)a; (void)n; return en_viewport_data((int32_t)rt_r32(0x00a04138u)); }
/* 0x00494c48 view_get_entity (EAX i) */
static uint32_t b_view_entity(const uint32_t* a, int n) { return en_view_entity((const uint32_t*)(void*)rt_ptr(0x00a03accu), (int32_t)A(0)); }
/* 0x00494dc0 fog_get_color */
static uint32_t b_fog_color(const uint32_t* a, int n)
{
    (void)a; (void)n;
    return en_fog_color(rt_r8(0x00a03b64u), rt_r8(0x00a03b68u), rt_r8(0x00a03b6cu));
}
/* 0x00495864 lightGetEnv (EAX i) */
static uint32_t b_light_env(const uint32_t* a, int n) { return en_light_env_addr((int32_t)A(0)); }
/* 0x0049527c light_set_pos (EAX e, EDX j, EBX x, ECX y, stack z), i16 x y z */
static uint32_t b_light_pos(const uint32_t* a, int n)
{
    en_light_set_pos(lights_g(), (int32_t)A(0), (int32_t)A(1), (int16_t)A(2), (int16_t)A(3), (int16_t)A(4));
    return 0;
}
/* 0x0049517c lightSetEnvDirection (EAX e, EDX j, EBX az, ECX el), i16 angles */
static uint32_t b_light_dir(const uint32_t* a, int n)
{
    en_light_set_dir(lights_g(), (int32_t)A(0), (int32_t)A(1), (int16_t)A(2), (int16_t)A(3));
    return 0;
}
/* 0x00495050 light_set (EAX e, EDX j, EBX az, ECX el, stack x, y, z) */
static uint32_t b_light_set(const uint32_t* a, int n)
{
    en_light_set(lights_g(), (int32_t)A(0), (int32_t)A(1), (int32_t)A(2), (int32_t)A(3),
                 (int16_t)A(4), (int16_t)A(5), (int16_t)A(6));
    return 0;
}
/* 0x00493aa4 viewInit */
static uint32_t b_views_init(const uint32_t* a, int n)
{
    (void)a; (void)n;
    en_views_init((EnViews*)(void*)rt_ptr(0x00a03ac8u), I32(0x00a0413cu));
    return 0;
}
/* 0x00494ec4 viewSetFixedCamera (EAX i, EDX dist, EBX mode); root = [0x009f7e38], cam = view i's camera */
static uint32_t b_view_fixed(const uint32_t* a, int n)
{
    int32_t i = (int32_t)A(0);
    const EnViews* v = (const EnViews*)(void*)rt_ptr(0x00a03ac8u);
    uint32_t cam = rt_r32(0x00a03ad4u + 4u * (uint32_t)i);
    en_view_fixed_camera(vp_g(), v, rt_ptr(0x00a03b70u), I32(rt_r32(0x009f7e38u)), I32(cam), i, (int32_t)A(1),
                         (int32_t)A(2), HW());
    return 0;
}
/* 0x004948b4 viewProc (EAX i); cam = views.camera[i], tgt = cam +0x10, root = [0x009f7e38],
 * trans 0x009f77c4. i >= 2: the port does nothing (pointers then unused). */
static uint32_t b_view_proc(const uint32_t* a, int n)
{
    int32_t i = (int32_t)A(0);
    uint32_t cam = (i >= 0 && i < 2) ? rt_r32(0x00a03ad4u + 4u * (uint32_t)i) : 0;
    uint32_t tgt = cam ? rt_r32(cam + 0x10u) : 0;
    en_view_proc(vp_g(), vblk(), I32(rt_r32(0x009f7e38u)), I32(cam), I32(tgt), I32(0x009f77c4u), i, HW());
    return 0;
}

/* ---- en_view2.c --------------------------------------------------------------------------- */

/* 0x004938a4 (EAX &w, EDX &h) */
static uint32_t b_screen_size(const uint32_t* a, int n) { en_view_screen_size(vblk(), I32(A(0)), I32(A(1))); return 0; }
/* 0x004938d4: into 0x00a03a90, returns it */
static uint32_t b_rect16(const uint32_t* a, int n)
{
    (void)a; (void)n;
    return en_view_rect16(vblk(), (int16_t*)(void*)rt_ptr(0x00a03a90u));
}
/* 0x0049392c (EAX i, EDX rect or 0) */
static uint32_t b_rect(const uint32_t* a, int n) { en_view_rect(vblk(), (int32_t)A(0), A(1) ? I32(A(1)) : NULL); return 0; }

/* 0x004941f0 view_resize_all (EAX W, EDX H): 0x00493ed0(EAX i, 6 stack words),
 * 0x004ad3d4(EAX i, EDX w, EBX h, ECX x0, stack x1, y0, y1) */
static void rs_setup(void* u, int32_t i, const int32_t s[6])
{
    (void)u;
    uint32_t v[7] = {(uint32_t)i};
    for (int k = 0; k < 6; k++) v[1 + k] = (uint32_t)s[k];
    rt_call(0x00493ed0u, v, 7);
}
static void rs_gpu(void* u, int32_t i, int32_t w, int32_t h, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    (void)u;
    uint32_t v[7] = {(uint32_t)i, (uint32_t)w, (uint32_t)h, (uint32_t)x0, (uint32_t)x1, (uint32_t)y0, (uint32_t)y1};
    rt_call(0x004ad3d4u, v, 7);
}
static uint32_t b_resize_all(const uint32_t* a, int n)
{
    EnResizeEnv e = {I32(0x00a0413cu), I32(0x004e83ecu), I32(0x004e83f0u), rs_setup, rs_gpu, NULL};
    en_view_resize_all(vblk(), &e, (int32_t)A(0), (int32_t)A(1));
    return 0;
}
/* 0x00494360 (EAX i, EDX out) */
static uint32_t b_border(const uint32_t* a, int n) { en_view_border(vblk(), (int32_t)A(0), I32(A(1))); return 0; }
/* 0x0049433c (EAX i, EDX x): 0x0049437c(EAX i, EDX x + border) */
static void bc_fn(void* u, int32_t i, int32_t x)
{
    (void)u;
    uint32_t v[2] = {(uint32_t)i, (uint32_t)x};
    rt_call(0x0049437cu, v, 2);
}
static uint32_t b_border_call(const uint32_t* a, int n) { en_view_border_call(vblk(), (int32_t)A(0), (int32_t)A(1), bc_fn, NULL); return 0; }
/* 0x00494bfc (EAX target, EDX i) */
static uint32_t b_set_target(const uint32_t* a, int n) { EnMem m = whole_mem(); en_view_set_target(vblk(), &m, A(0), (int32_t)A(1)); return 0; }
/* 0x00494c3c (EAX p) */
static uint32_t b_set_viewer0(const uint32_t* a, int n) { EnMem m = whole_mem(); en_view_set_viewer0(vblk(), &m, A(0)); return 0; }
/* 0x00494c5c (EAX v, EDX i) */
static uint32_t b_viewer_pos(const uint32_t* a, int n) { EnMem m = whole_mem(); en_view_set_viewer_pos(vblk(), &m, I32(A(0)), (int32_t)A(1)); return 0; }
/* 0x00494ca4 / 0x00494cc0 (EAX out, EDX i); 0x00494cdc / 0x00494d34 (EAX in, EDX i) */
static uint32_t b_get_angles(const uint32_t* a, int n) { EnMem m = whole_mem(); en_view_get_cam_angles(vblk(), &m, rt_ptr(A(0)), (int32_t)A(1)); return 0; }
static uint32_t b_get_pos(const uint32_t* a, int n) { EnMem m = whole_mem(); en_view_get_cam_pos(vblk(), &m, rt_ptr(A(0)), (int32_t)A(1)); return 0; }
static uint32_t b_set_angles(const uint32_t* a, int n) { EnMem m = whole_mem(); en_view_set_cam_angles(vblk(), &m, rt_ptr(A(0)), (int32_t)A(1)); return 0; }
static uint32_t b_set_pos(const uint32_t* a, int n) { EnMem m = whole_mem(); en_view_set_cam_pos(vblk(), &m, rt_ptr(A(0)), (int32_t)A(1)); return 0; }
/* 0x00494d04 / 0x00494d14 / 0x00494d24 (EAX i): camera i +0xe8 / +0xe0 / +0xb0 */
static uint32_t b_cam_e8(const uint32_t* a, int n) { return en_view_cam_field(vblk(), (int32_t)A(0), 0xe8u); }
static uint32_t b_cam_e0(const uint32_t* a, int n) { return en_view_cam_field(vblk(), (int32_t)A(0), 0xe0u); }
static uint32_t b_cam_b0(const uint32_t* a, int n) { return en_view_cam_field(vblk(), (int32_t)A(0), 0xb0u); }
/* 0x00494d5c (EAX i) */
static uint32_t b_cam_target(const uint32_t* a, int n) { EnMem m = whole_mem(); return en_view_cam_target(vblk(), &m, (int32_t)A(0)); }
/* 0x00494d80 fog_set_color (EAX r, EDX g, EBX b): device 0x004b6428(EAX 0x00c84840, EDX r, EBX g, ECX b) */
static void fog_dev(void* u, uint32_t r, uint32_t g, uint32_t b)
{
    (void)u;
    uint32_t v[4] = {EN2_DISPLAY, r, g, b};
    rt_call(0x004b6428u, v, 4);
}
static uint32_t b_fog_set(const uint32_t* a, int n)
{
    en_fog_set(vblk(), (int32_t)A(0), (int32_t)A(1), (int32_t)A(2), HW(), fog_dev, NULL);
    return 0;
}
/* 0x00494e64 / 0x00494e84 (EAX i) */
static uint32_t b_render_pos(const uint32_t* a, int n) { return en_view_render_pos_addr((int32_t)A(0)); }
static uint32_t b_cam_mat(const uint32_t* a, int n) { return en_view_cam_mat_addr((int32_t)A(0)); }

/* ---- en_camera.c -------------------------------------------------------------------------- */

/* 0x0048b9b4 / 0x0048ba84 (EAX src): EnXform at 0x009f77d0; 0x0048ba48 (EAX mat): out 0x009f77c4 */
static uint32_t b_load_rot(const uint32_t* a, int n) { en_view_load_rot((EnXform*)(void*)rt_ptr(0x009f77d0u), I32(A(0))); return 0; }
static uint32_t b_load_pos(const uint32_t* a, int n) { en_view_load_pos((EnXform*)(void*)rt_ptr(0x009f77d0u), I32(A(0))); return 0; }
static uint32_t b_set_trans(const uint32_t* a, int n) { en_set_trans(I32(0x009f77c4u), I32(A(0))); return 0; }

/* ---- en_math.c ---------------------------------------------------------------------------- */

/* 0x0048cca8 / 0x0048cd3c (EAX m, EDX v, EBX out) */
static uint32_t b_mat_vec(const uint32_t* a, int n) { en_mat_vec_mul(I32(A(0)), I32(A(1)), I32(A(2))); return 0; }
static uint32_t b_mat_t_vec_s16(const uint32_t* a, int n)
{
    en_mat_t_vec_mul_s16(I32(A(0)), (const int16_t*)(void*)rt_ptr(A(1)), I32(A(2)));
    return 0;
}
/* 0x0048ce88 (EAX dst, EDX src, EBX count) */
static uint32_t b_copy_words(const uint32_t* a, int n) { en_copy_words(I32(A(0)), I32(A(1)), (int32_t)A(2)); return 0; }
/* 0x0048bbcc view_set_size (EAX w, EDX h): w, h at 0x009f7808 / 0c, scale at 0x009f7818 / 1c
 * (not contiguous: copied in and out as the harness did) */
static uint32_t b_view_set_size(const uint32_t* a, int n)
{
    EnViewSize v;
    memcpy(&v.w, rt_ptr(0x009f7808u), 8);
    memcpy(&v.scale_x, rt_ptr(0x009f7818u), 8);
    en_view_set_size(&v, (int32_t)A(0), (int32_t)A(1), HW());
    memcpy(rt_ptr(0x009f7808u), &v.w, 8);
    memcpy(rt_ptr(0x009f7818u), &v.scale_x, 8);
    return 0;
}
/* 0x004aab90 (EAX dst), returns dst (EAX untouched) */
static uint32_t b_name_init(const uint32_t* a, int n) { en_name_init(rt_ptr(A(0))); return A(0); }
/* 0x0048b460 (EAX node) */
static uint32_t b_mesh_quadrants(const uint32_t* a, int n) { en_mesh_set_quadrants(rt_base, 0, RT_SIZE, A(0)); return 0; }

/* ---- en_clip.c ---------------------------------------------------------------------------- */

/* 0x0048c62c / 0x0048c6b8 (EAX in, EDX out) */
static uint32_t b_xform_short(const uint32_t* a, int n)
{
    en_xform_short((const EnXform*)(void*)rt_ptr(0x009f77d0u), (const int16_t*)(void*)rt_ptr(A(0)), (float*)(void*)rt_ptr(A(1)));
    return 0;
}
static uint32_t b_xform_int(const uint32_t* a, int n)
{
    en_xform_int((const EnXform*)(void*)rt_ptr(0x009f77d0u), I32(A(0)), (float*)(void*)rt_ptr(A(1)));
    return 0;
}
/* 0x0048c890 (EAX vertex): EnProj at 0x009f7810 */
static uint32_t b_project(const uint32_t* a, int n)
{
    en_project((const EnProj*)(void*)rt_ptr(0x009f7810u), (EnVertex*)(void*)rt_ptr(A(0)));
    return 0;
}

/* game clip list (i32 count, game vertex addresses) <-> EnClipBuf (host pointers) */
static void clip_in(uint32_t list, EnClipBuf* b)
{
    memset(b, 0, sizeof *b);
    b->count = (int32_t)rt_r32(list);
    for (int i = 0; i < b->count && i < EN_CLIP_MAX; i++) b->v[i] = (EnVertex*)(void*)rt_ptr(rt_r32(list + 4u + 4u * (uint32_t)i));
}
static void clip_out(uint32_t list, const EnClipBuf* b)
{
    rt_w32(list, (uint32_t)b->count);
    for (int i = 0; i < b->count && i < EN_CLIP_MAX; i++) rt_w32(list + 4u + 4u * (uint32_t)i, rt_addr(b->v[i]));
}
/* 0x0048f59c frustum_clip_plane (EAX in, EDX out, EBX a, ECX b, stack sign); env 0x009f7dd8 next,
 * 0x009f7d44 mask, 0x009f7d80 clipped, 0x004e82b4 depth scale */
static uint32_t b_clip_plane(const uint32_t* a, int n)
{
    EnClipBuf in, out;
    clip_in(A(0), &in);
    clip_in(A(1), &out);
    EnClipEnv e = {(EnVertex*)(void*)rt_ptr(rt_r32(0x009f7dd8u)), rt_r32(0x009f7d44u), (int32_t)rt_r32(0x009f7d80u), rt_r32(0x004e82b4u)};
    en_clip_plane(&e, &in, &out, (int)A(2), (int)A(3), (int)A(4));
    clip_out(A(1), &out);
    rt_w32(0x009f7dd8u, rt_addr(e.next));
    rt_w32(0x009f7d80u, (uint32_t)e.clipped);
    return 0;
}
/* 0x00491ee4 winding_check: list [0x004e8290], split = 0x004958ac() */
static uint32_t b_winding(const uint32_t* a, int n)
{
    (void)a; (void)n;
    EnClipBuf l;
    clip_in(rt_r32(0x004e8290u), &l);
    int split = (int)rt_call(0x004958acu, NULL, 0);
    return (uint32_t)en_winding_check(&l, split);
}

/* ---- en_fog.c ----------------------------------------------------------------------------- */

/* 0x0049024c polygon_light_terms */
static uint32_t b_light_terms(const uint32_t* a, int n)
{
    (void)a; (void)n;
    static const uint32_t QA[3] = {0x009f7d6cu, 0x009f7d74u, 0x009f7d68u};
    EnClipBuf clip;
    clip_in(rt_r32(0x004e8290u), &clip);
    EnFogEnv e;
    e.lights = rt_ptr(0x00c85c00u);
    e.clip = &clip;
    e.poly = rt_ptr(rt_r32(0x009f7cf4u));
    e.verts = rt_ptr(rt_r32(0x009f7cc4u));
    e.wave_a = (int32_t)rt_r32(0x009f7d04u);
    e.wave_b = (int32_t)rt_r32(0x009f7d08u);
    e.render_flags = rt_r32(0x00c846d8u);
    e.light_index = (int32_t)rt_r32(0x009f7cccu);
    for (int k = 0; k < 3; k++) memcpy(&e.q[k], rt_ptr(QA[k]), 4);
    en_light_terms(&e);
    rt_w32(0x00c846d8u, e.render_flags);
    rt_w32(0x009f7cccu, (uint32_t)e.light_index);
    for (int k = 0; k < 3; k++) memcpy(rt_ptr(QA[k]), &e.q[k], 4);
    return 0;
}

/* ---- en_reg.c ----------------------------------------------------------------------------- */

static const uint32_t REG_SHIFT[4] = {0x004e8448u, 0x004e8430u, 0x004e8288u, 0x004e8f88u};

/* "Can't add: ..." : printf 0x004a1f1e (cdecl, the format on the stack); the game's own string
 * at 0x004e30b4 is passed (the port's text is the same message) */
static void reg_print(void* u, const char* msg)
{
    (void)u; (void)msg;
    uint32_t v[1] = {0x004e30b4u};
    rt_call(0x004a1f1eu, v, 1);
}
static void reg_in(EnReg* r)
{
    memcpy(r->anims, rt_ptr(0x00a81c9cu), sizeof r->anims);
    r->anim_count = (int32_t)rt_r32(0x00a81e2cu);
    r->anim_queue = (int32_t)rt_r32(0x00a81e30u);
    memcpy(r->models, rt_ptr(0x00a81c4cu), sizeof r->models);
    r->list_a = rt_r32(0x00a81e3cu);
    r->list_b = rt_r32(0x00a81e34u);
    for (int k = 0; k < 4; k++) r->shift[k] = (int32_t)rt_r32(REG_SHIFT[k]);
    r->frame_clear = (int32_t)rt_r32(0x004e844cu);
    r->frame_set = (int32_t)rt_r32(0x00a81c38u);
    r->ticks = (int32_t)rt_r32(0x00a81c48u);
    r->print = reg_print;
    r->user = NULL;
}
static void reg_out(const EnReg* r)
{
    memcpy(rt_ptr(0x00a81c9cu), r->anims, sizeof r->anims);
    rt_w32(0x00a81e2cu, (uint32_t)r->anim_count);
    rt_w32(0x00a81e30u, (uint32_t)r->anim_queue);
    memcpy(rt_ptr(0x00a81c4cu), r->models, sizeof r->models);
    rt_w32(0x00a81e3cu, r->list_a);
    rt_w32(0x00a81e34u, r->list_b);
    for (int k = 0; k < 4; k++) rt_w32(REG_SHIFT[k], (uint32_t)r->shift[k]);
    rt_w32(0x004e844cu, (uint32_t)r->frame_clear);
    rt_w32(0x00a81c38u, (uint32_t)r->frame_set);
    rt_w32(0x00a81c48u, (uint32_t)r->ticks);
}
static uint32_t b_reg_reset(const uint32_t* a, int n) { (void)a; (void)n; EnReg r; reg_in(&r); en_reg_reset(&r); reg_out(&r); return 0; }
/* 0x004a1430 animRegister (EAX node) */
static uint32_t b_reg_add_anim(const uint32_t* a, int n) { EnReg r; reg_in(&r); en_reg_add_anim(&r, A(0)); reg_out(&r); return 0; }
static uint32_t b_reg_off(const uint32_t* a, int n) { (void)a; (void)n; EnReg r; reg_in(&r); en_reg_queue(&r, 0); reg_out(&r); return 0; }
static uint32_t b_reg_on(const uint32_t* a, int n) { (void)a; (void)n; EnReg r; reg_in(&r); en_reg_queue(&r, 1); reg_out(&r); return 0; }
/* 0x004a14f0 (EAX a, EDX b) */
static uint32_t b_reg_lists(const uint32_t* a, int n) { EnReg r; reg_in(&r); en_reg_set_lists(&r, A(0), A(1)); reg_out(&r); return 0; }
/* 0x004a1314 (EAX shift) */
static uint32_t b_reg_shift(const uint32_t* a, int n) { EnReg r; reg_in(&r); en_reg_set_shift(&r, (int32_t)A(0)); reg_out(&r); return 0; }
/* 0x004a0dd4 renderFlipFrame: parity [0x00a0364c], ticks60 i16 [0x004e8300] */
static uint32_t b_reg_frame(const uint32_t* a, int n)
{
    (void)a; (void)n;
    EnReg r;
    reg_in(&r);
    en_reg_frame(&r, (int32_t)rt_r32(0x00a0364cu), (int16_t)rt_r16(0x004e8300u));
    reg_out(&r);
    return 0;
}

/* ---- en_scene.c (+ en_view2.c en_scene_init) ---------------------------------------------- */

#define SC_RLIST 0x00a658c8u
#define SC_PARENT 0x00a801c8u
static const uint32_t SC_MATS[4] = {0x00a6d5c8u, 0x00a720c8u, 0x00a76bc8u, 0x00a7b6c8u};
static const uint32_t SC_ANIM[4] = {0x004a09f8u, 0x004a0a78u, 0x004a0b08u, 0x004a0b7cu};

/* one walk state for the whole run (the parent pointers of its slots point into it) */
static DpcWalk sc_w;
static EnScene sc_s;

/* game address of a matrix the walk points at: entity memory as is; a slot of the walk's own
 * chains is copied out to its place in the original's chain first (slot 400 lands on the next
 * chain's slot 0, as in the original) */
static uint32_t sc_mat_addr(const int32_t* p)
{
    const uint8_t* q = (const uint8_t*)p;
    if (q >= rt_base && q < rt_base + RT_SIZE) return rt_addr(q);
    const uint8_t* m0 = (const uint8_t*)sc_w.mat;
    if (q >= m0 && q < m0 + sizeof sc_w.mat) {
        size_t o = (size_t)(q - m0) / (12 * sizeof(int32_t));
        uint32_t k = (uint32_t)(o / (DPC_WALK_MAX_SLOTS + 1)), s = (uint32_t)(o % (DPC_WALK_MAX_SLOTS + 1));
        uint32_t ga = SC_MATS[k] + 0x30u * s;
        memcpy(rt_ptr(ga), sc_w.mat[k][s], 0x30);
        return ga;
    }
    return 0;
}

/* scalar driver globals out to memory (before any call that leaves the port) */
static void sc_globals_out(void)
{
    EnScene* s = &sc_s;
    rt_w32(0x00a81c3cu, (uint32_t)s->cull_force);
    rt_w32(0x004e8440u, (uint32_t)s->cull_depth);
    memcpy(rt_ptr(0x00a81c18u), s->cleared, sizeof s->cleared);
    rt_w32(0x00a81c48u, (uint32_t)s->anim_ticks);
    rt_w32(0x00a81c40u, (uint32_t)sc_w.lod_disabled);
    rt_w32(0x00a81c10u, (uint32_t)sc_w.slot_index);
    rt_w32(0x00a81c14u, (uint32_t)sc_w.group_count);
    rt_w32(0x00a81c0cu, (uint32_t)sc_w.item_count);
}

/* the render list, slot chains and parent table out (before the renderer / light pass read them) */
static void sc_tables_out(void)
{
    int32_t top = sc_w.slot_index;
    if (top < 0) top = 0;
    if (top > DPC_WALK_MAX_SLOTS) top = DPC_WALK_MAX_SLOTS;
    for (int k = 0; k < 4; k++)
        for (int32_t s = 0; s <= top && s < DPC_WALK_MAX_SLOTS; s++)
            memcpy(rt_ptr(SC_MATS[k] + 0x30u * (uint32_t)s), sc_w.mat[k][s], 0x30);
    for (int32_t s = 0; s <= top && s < DPC_WALK_MAX_SLOTS; s++)
        for (int k = 0; k < 4; k++)
            if (sc_w.parent[s][k]) rt_w32(SC_PARENT + 16u * (uint32_t)s + 4u * (uint32_t)k, sc_mat_addr(sc_w.parent[s][k]));
    /* ORIGINAL BUG (as verified): slot 400 of chain k is slot 0 of chain k + 1; chain D's slot
     * 400 (the parent table's slot 0) is left alone */
    if (top == DPC_WALK_MAX_SLOTS)
        for (int k = 0; k < 3; k++) memcpy(rt_ptr(SC_MATS[k] + 0x30u * DPC_WALK_MAX_SLOTS), sc_w.mat[k][DPC_WALK_MAX_SLOTS], 0x30);
    int32_t ni = sc_w.item_count;
    if (ni > DPC_WALK_MAX_ITEMS) ni = DPC_WALK_MAX_ITEMS;
    for (int32_t i = 0; i < ni; i++) {
        const DpcDrawItem* it = &sc_w.items[i];
        uint32_t ga = SC_RLIST + 0x20u * (uint32_t)i;
        rt_w32(ga, it->node);
        for (int k = 0; k < 3; k++) rt_w32(ga + 4u + 4u * (uint32_t)k, (uint32_t)it->pos[k]);
        rt_w32(ga + 0x10u, (uint32_t)it->shift);
        rt_w32(ga + 0x14u, it->root);
        rt_w32(ga + 0x18u, (uint32_t)it->slot);
        rt_w32(ga + 0x1cu, (uint32_t)it->group);
    }
}

/* 0x004a043c csSphereVisible(EAX bounds, EDX radius, EBX pos, ECX mat_c, stack mat_d, shift):
 * mat_c / mat_d are the slot's parent chains C and D (0x00a801c8 + 16 slot + 8 / + 0xc, read
 * from the original walk 0x0049f6e8); pos (a local of the walk) goes on a frame */
static int sc_visible(void* u, uint32_t bounds, int32_t radius, const int32_t pos[3], int32_t slot, uint8_t shift)
{
    (void)u;
    sc_globals_out();
    uint32_t fp = rt_frame_push(16);
    memcpy(rt_ptr(fp), pos, 12);
    uint32_t mc = 0, md = 0;
    if (slot >= 0 && slot <= DPC_WALK_MAX_SLOTS) {
        mc = sc_w.parent[slot][2] ? sc_mat_addr(sc_w.parent[slot][2]) : rt_r32(SC_PARENT + 16u * (uint32_t)slot + 8u);
        md = sc_w.parent[slot][3] ? sc_mat_addr(sc_w.parent[slot][3]) : rt_r32(SC_PARENT + 16u * (uint32_t)slot + 12u);
    }
    uint32_t v[6] = {bounds, (uint32_t)radius, fp, mc, md, shift};
    uint32_t r = rt_call(0x004a043cu, v, 6);
    rt_frame_pop(16);
    return (int)r;
}
/* type 3 animation steps (EAX node) */
static void sc_anim(void* u, uint32_t node, int kind)
{
    (void)u;
    if (kind < 0 || kind > 3) return;
    sc_globals_out();
    rt_call(SC_ANIM[kind], &node, 1);
}
/* 0x0048e9a8 (EAX param, EDX 0x00a658c8, EBX count, ECX 0x00a6d5c8, stack 0x00a720c8) */
static void sc_draw(void* u, uint32_t param, int32_t count)
{
    (void)u;
    sc_globals_out();
    sc_tables_out();
    uint32_t v[5] = {param, SC_RLIST, (uint32_t)count, SC_MATS[0], SC_MATS[1]};
    rt_call(0x0048e9a8u, v, 5);
}
/* 0x0048b3f8 (EAX 0x00a658c8, EDX count, EBX 0x00a6d5c8) */
static void sc_lights(void* u, int32_t count)
{
    (void)u;
    sc_globals_out();
    sc_tables_out();
    uint32_t v[3] = {SC_RLIST, (uint32_t)count, SC_MATS[0]};
    rt_call(0x0048b3f8u, v, 3);
}

/* EnScene and the walk inputs from the original's globals */
static EnScene* sc_in(void)
{
    EnScene* s = &sc_s;
    DpcWalk* w = &sc_w;
    w->buf = rt_base; w->size = RT_SIZE; w->addr_base = 0;
    w->lod_disabled = (int)rt_r32(0x00a81c40u);
    w->frame_clear = (int)rt_r32(0x004e844cu);
    w->frame_set = (int)rt_r32(0x00a81c38u);
    w->visible = sc_visible; w->anim = sc_anim; w->user = s;
    w->slot_index = (int32_t)rt_r32(0x00a81c10u);
    w->group_count = (int32_t)rt_r32(0x00a81c14u);
    w->item_count = (int32_t)rt_r32(0x00a81c0cu);
    s->w = w;
    s->scene_root = rt_r32(0x009f7e38u);
    s->list_head = rt_r32(0x009f7e34u);
    s->viewer[0] = rt_r32(0x00a03accu);
    s->viewer[1] = rt_r32(0x00a03ad0u);
    memcpy(s->extra, rt_ptr(0x00a81c4cu), sizeof s->extra);
    memcpy(s->anim_node, rt_ptr(0x00a81c9cu), sizeof s->anim_node);
    s->anim_count = (int32_t)rt_r32(0x00a81e2cu);
    s->anim_on = (int32_t)rt_r32(0x00a81e30u);
    s->force_visible = (int32_t)rt_r32(0x00a81c44u);
    s->cull_force = (int32_t)rt_r32(0x00a81c3cu);
    s->cull_depth = (int32_t)rt_r32(0x004e8440u);
    s->cull_depth_init = (int32_t)rt_r32(0x004e8444u);
    s->root_shift = (int32_t)rt_r32(0x004e8448u);
    memcpy(s->cleared, rt_ptr(0x00a81c18u), sizeof s->cleared);
    s->anim_ticks = (int32_t)rt_r32(0x00a81c48u);
    s->ticks60 = (int16_t)rt_r16(0x004e8300u);
    s->draw = sc_draw; s->lights = sc_lights; s->anim = sc_anim; s->user = s;
    return s;
}
static void sc_done(void) { sc_globals_out(); sc_tables_out(); }

/* 0x00492a40 entity_setup_render (EAX entity, EDX out matrix, EBX force) */
static uint32_t b_entity_setup(const uint32_t* a, int n)
{
    EnScene* s = sc_in();
    en_entity_setup(s, A(0), I32(A(1)), (int)A(2));
    return 0;
}
/* 0x004a0e20 render_overlays (EAX param, EDX view) */
static uint32_t b_scene_draw(const uint32_t* a, int n)
{
    EnScene* s = sc_in();
    en_scene_draw(s, A(0), (int32_t)A(1));
    sc_done();
    return 0;
}
/* 0x004a10fc renderDrawModel (EAX param, EDX entity) */
static uint32_t b_draw_one(const uint32_t* a, int n)
{
    EnScene* s = sc_in();
    en_scene_draw_one(s, A(0), A(1));
    sc_done();
    return 0;
}
/* 0x004a1194 renderDrawObjectUnculled (EAX entity) */
static uint32_t b_light_one(const uint32_t* a, int n)
{
    EnScene* s = sc_in();
    en_scene_light_one(s, A(0));
    sc_done();
    return 0;
}
/* 0x00492c3c entity_list_head */
static uint32_t b_list_head(const uint32_t* a, int n) { (void)a; (void)n; return rt_r32(0x009f7e34u); }

/* 0x004a1224 subsys_entities_init: the four module inits, then the globals it clears.
 * k 3: 0x004a14f0 also takes EDX, which the original leaves at 0x00a5eb68 (0x004c38f4 does not
 * touch it) */
static void si_init(void* u, int k, uint32_t a)
{
    (void)u;
    static const uint32_t FN[4] = {0x0049d494u, 0x0048e240u, 0x004c38f4u, 0x004a14f0u};
    uint32_t v[2] = {a, 0x00a5eb68u};
    rt_call(FN[k], v, k >= 2 ? 2 : 1);
}
static uint32_t b_scene_init(const uint32_t* a, int n)
{
    (void)a; (void)n;
    EnScene s;
    DpcWalk* w = &sc_w;
    memset(&s, 0, sizeof s);
    s.w = w;
    en_scene_init(&s, si_init, NULL);
    /* only what it writes */
    rt_w32(0x00a81c3cu, (uint32_t)s.cull_force);
    rt_w32(0x00a81c40u, (uint32_t)w->lod_disabled);
    rt_w32(0x00a81c44u, (uint32_t)s.force_visible);
    rt_w32(0x00a81e2cu, (uint32_t)s.anim_count);
    rt_w32(0x00a81e30u, (uint32_t)s.anim_on);
    memcpy(rt_ptr(0x00a81c9cu), s.anim_node, sizeof s.anim_node);
    memcpy(rt_ptr(0x00a81c4cu), s.extra, sizeof s.extra);
    return 0;
}

void rt_bind_engine_view(void)
{
    /* en_view.c */
    rt_bind(0x0048bb10u, b_vp_origin, "viewport_set_origin");
    rt_bind(0x0048bb94u, b_vp_width, "viewport_set_width");
    rt_bind(0x00493880u, b_vp_data, "level_get_viewport_data");
    rt_bind(0x00493aa4u, b_views_init, "viewInit");
    rt_bind(0x004948b4u, b_view_proc, "viewProc");
    rt_bind(0x00494c48u, b_view_entity, "view_get_entity");
    rt_bind(0x00494dc0u, b_fog_color, "fog_get_color");
    rt_bind(0x00494ec4u, b_view_fixed, "viewSetFixedCamera");
    rt_bind(0x00495050u, b_light_set, "light_set");
    rt_bind(0x0049517cu, b_light_dir, "lightSetEnvDirection");
    rt_bind(0x0049527cu, b_light_pos, "light_set_pos");
    rt_bind(0x00495864u, b_light_env, "lightGetEnv");
    /* en_view2.c */
    rt_bind(0x004938a4u, b_screen_size, "view_get_screen_size");
    rt_bind(0x004938d4u, b_rect16, "view_get_rect16");
    rt_bind(0x0049392cu, b_rect, "view_get_rect");
    rt_bind(0x004941f0u, b_resize_all, "view_resize_all");
    rt_bind(0x0049433cu, b_border_call, "view_border_call");
    rt_bind(0x00494360u, b_border, "view_get_border");
    rt_bind(0x00494bfcu, b_set_target, "view_set_target");
    rt_bind(0x00494c3cu, b_set_viewer0, "view_set_viewer0");
    rt_bind(0x00494c5cu, b_viewer_pos, "view_set_viewer_pos");
    rt_bind(0x00494ca4u, b_get_angles, "view_get_cam_angles");
    rt_bind(0x00494cc0u, b_get_pos, "view_get_cam_pos");
    rt_bind(0x00494cdcu, b_set_angles, "view_set_cam_angles");
    rt_bind(0x00494d04u, b_cam_e8, "view_cam_pos_addr");
    rt_bind(0x00494d14u, b_cam_e0, "view_cam_angles_addr");
    rt_bind(0x00494d24u, b_cam_b0, "view_cam_rot_addr");
    rt_bind(0x00494d34u, b_set_pos, "view_set_cam_pos");
    rt_bind(0x00494d5cu, b_cam_target, "view_get_cam_target");
    rt_bind(0x00494d80u, b_fog_set, "fog_set_color");
    rt_bind(0x00494e64u, b_render_pos, "view_render_pos_addr");
    rt_bind(0x00494e84u, b_cam_mat, "view_cam_mat_addr");
    rt_bind(0x004a1224u, b_scene_init, "subsys_entities_init");
    /* en_camera.c */
    rt_bind(0x0048b9b4u, b_load_rot, "camera_load_orient");
    rt_bind(0x0048ba48u, b_set_trans, "SetTransMatrix");
    rt_bind(0x0048ba84u, b_load_pos, "camera_load_position");
    /* en_math.c */
    rt_bind(0x0048b460u, b_mesh_quadrants, "meshSetPrimQuadrants");
    rt_bind(0x0048bbccu, b_view_set_size, "view_set_size");
    rt_bind(0x0048cca8u, b_mat_vec, "mathMulMatVec2");
    rt_bind(0x0048cd3cu, b_mat_t_vec_s16, "math_mat_vec_mul_packed");
    rt_bind(0x0048ce88u, b_copy_words, "copy_words");
    rt_bind(0x004aab90u, b_name_init, "name_init");
    /* en_clip.c */
    rt_bind(0x0048c62cu, b_xform_short, "math_short_to_float_vec");
    rt_bind(0x0048c6b8u, b_xform_int, "math_int_to_float_vec");
    rt_bind(0x0048c890u, b_project, "vertex_project");
    rt_bind(0x0048f59cu, b_clip_plane, "frustum_clip_plane");
    rt_bind(0x00491ee4u, b_winding, "winding_check");
    /* en_fog.c */
    rt_bind(0x0049024cu, b_light_terms, "polygon_light_terms");
    /* en_reg.c */
    rt_bind(0x004a0dd4u, b_reg_frame, "renderFlipFrame");
    rt_bind(0x004a129cu, b_reg_reset, "scene_reg_reset");
    rt_bind(0x004a1314u, b_reg_shift, "scene_set_shift");
    rt_bind(0x004a1430u, b_reg_add_anim, "animRegister");
    rt_bind(0x004a1488u, b_reg_off, "scene_anim_queue_off");
    rt_bind(0x004a1494u, b_reg_on, "scene_anim_queue_on");
    rt_bind(0x004a14f0u, b_reg_lists, "scene_set_lists");
    /* en_scene.c */
    rt_bind(0x00492a40u, b_entity_setup, "entity_setup_render");
    rt_bind(0x00492c3cu, b_list_head, "entity_list_head");
    rt_bind(0x004a0e20u, b_scene_draw, "render_overlays");
    rt_bind(0x004a10fcu, b_draw_one, "renderDrawModel");
    rt_bind(0x004a1194u, b_light_one, "renderDrawObjectUnculled");
}

RT_BIND_AREA(engine_view)
