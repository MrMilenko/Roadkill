/*
 * bind_match_scene.c: the front end scene engine's setup, frame loop and redraw 0x00469c78..
 * 0x0046a5fc (src/match/mt_scene.c), called the way tools/emu/diff_match_scene.py called them:
 * the scene block / records are game addresses in EAX, the callbacks the original addresses with
 * the registers the harness recorded. Host pointers the port hands to a callback (scene + 0x10,
 * the redraw rect, the image record) go back to game addresses with rt_addr.
 */
#include "rt_env.h"
#include "match/mt_scene.h"

static uint32_t c4(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx)
{
    uint32_t a[4] = {eax, edx, ebx, ecx};
    return rt_call(fn, a, 4);
}

static uint8_t* s_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static uint32_t s_screen_buffer(void* u) { (void)u; return c4(0x0046cb9cu, 0, 0, 0, 0); }
static void s_input_start(void* u) { (void)u; c4(0x00493aa4u, 0, 0, 0, 0); }
static void s_input_finalize(void* u) { (void)u; c4(0x00492df4u, 0, 0, 0, 0); }
static void s_entity_create(void* u, int32_t a0, const uint32_t stack[6])
{
    (void)u;
    uint32_t a[7] = {(uint32_t)a0};
    for (int i = 0; i < 6; i++) a[1 + i] = stack[i];
    rt_call(0x00494610u, a, 7);
}
static void s_textures_setup(void* u, int32_t a) { (void)u; c4(0x0049456cu, (uint32_t)a, 0, 0, 0); }
static void s_light_a(void* u, int32_t a0, int32_t b, int32_t c, int32_t d, uint32_t s)
{
    (void)u;
    uint32_t a[5] = {(uint32_t)a0, (uint32_t)b, (uint32_t)c, (uint32_t)d, s};
    rt_call(0x0049527cu, a, 5);
}
static void s_light_b(void* u, int32_t a, int32_t b, int32_t c, int32_t d) { (void)u; c4(0x0049517cu, (uint32_t)a, (uint32_t)b, (uint32_t)c, (uint32_t)d); }
static void s_light_c(void* u, int32_t a, int32_t b, int32_t c, int32_t d) { (void)u; c4(0x004952f4u, (uint32_t)a, (uint32_t)b, (uint32_t)c, (uint32_t)d); }
static void s_pad_a(void* u, uint8_t* p, int32_t b) { (void)u; c4(0x00494d34u, rt_addr(p), (uint32_t)b, 0, 0); }
static uint32_t s_pad_b(void* u, uint8_t* p, int32_t b) { (void)u; return c4(0x00494cdcu, rt_addr(p), (uint32_t)b, 0, 0); }
static void s_input_poll(void* u, uint32_t v) { (void)u; c4(0x00493748u, v, 0, 0, 0); }
static void s_input_update(void* u) { (void)u; c4(0x0049364cu, 0, 0, 0, 0); }
static uint32_t s_timer_now(void* u) { (void)u; return c4(0x00493038u, 0, 0, 0, 0); }
static void s_render_effects(void* u) { (void)u; c4(0x004a0dd4u, 0, 0, 0, 0); }
/* a command's run: its vtable (+8) entry 0, eax = command */
static void s_run_command(void* u, uint32_t cmd) { (void)u; c4(rt_r32(rt_r32(cmd + 8u)), cmd, 0, 0, 0); }
static int32_t s_tics_to_rate(void* u, uint32_t t) { (void)u; return (int32_t)c4(0x00493274u, t, 0, 0, 0); }
static void s_idle(void* u, int32_t a) { (void)u; c4(0x00495890u, (uint32_t)a, 0, 0, 0); }
static void s_frame_timer_update(void* u) { (void)u; c4(0x004930acu, 0, 0, 0, 0); }
static void s_bg_draw(void* u, int32_t a, int32_t b, int32_t c, int32_t d) { (void)u; c4(0x0046f7c4u, (uint32_t)a, (uint32_t)b, (uint32_t)c, (uint32_t)d); }
static void s_draw_begin(void* u) { (void)u; c4(0x0049632cu, 0, 0, 0, 0); }
static void s_buffer_reset(void* u, uint32_t b, int32_t v) { (void)u; c4(0x004966d8u, b, (uint32_t)v, 0, 0); }
/* an object's draw: its vtable (+8) entry 1, eax = object, edx = draw buffer */
static void s_draw_object(void* u, uint32_t o, uint32_t b) { (void)u; c4(rt_r32(rt_r32(o + 8u) + 4u), o, b, 0, 0); }
static void s_list_flush(void* u, uint32_t l) { (void)u; c4(0x0049681cu, l, 0, 0, 0); }
static void s_draw_end(void* u) { (void)u; c4(0x0049635cu, 0, 0, 0, 0); }
static void s_present_rect(void* u, uint8_t* r) { (void)u; c4(0x00495b20u, rt_addr(r), 0, 0, 0); }
static void s_bg_load(void* u, uint8_t* rec, int32_t b) { (void)u; c4(0x0046f784u, rt_addr(rec), (uint32_t)b, 0, 0); }
static void s_file_load(void* u, uint32_t v) { (void)u; c4(0x004740a8u, v, 0, 0, 0); }

static const MtSceneEnv env = {
    s_resolve, s_screen_buffer, s_input_start, s_input_finalize, s_entity_create, s_textures_setup,
    s_light_a, s_light_b, s_light_c, s_pad_a, s_pad_b, s_input_poll, s_input_update, s_timer_now,
    s_render_effects, s_run_command, s_tics_to_rate, s_idle, s_frame_timer_update, s_bg_draw,
    s_draw_begin, s_buffer_reset, s_draw_object, s_list_flush, s_draw_end, s_present_rect, s_bg_load,
    s_file_load, NULL,
};

#define R(i) rt_arg(a, n, i)

/* 0x00469f08 sceneInit(eax scene, edx, ebx, ecx, stack word) */
static uint32_t b_scene_init(const uint32_t* a, int n) { mt_scene_init(rt_ptr(R(0)), R(1), R(2), R(3), R(4), &env); return 0; }
/* 0x0046a5fc sceneRun(eax scene) */
static uint32_t b_scene_run(const uint32_t* a, int n) { return (uint32_t)mt_scene_run(rt_ptr(R(0)), &env); }
/* 0x0046a4f8 redraw(eax scene, edx buffer) */
static uint32_t b_scene_draw(const uint32_t* a, int n) { mt_scene_draw(rt_ptr(R(0)), R(1), &env); return 0; }
/* 0x00469e24 hit test(eax object, edx x, ebx y) */
static uint32_t b_scene_hit(const uint32_t* a, int n) { return (uint32_t)mt_scene_hit(rt_ptr(R(0)), (int32_t)R(1), (int32_t)R(2), &env); }
/* 0x00469c78 image load(eax record) */
static uint32_t b_scene_image_load(const uint32_t* a, int n) { mt_scene_image_load(rt_ptr(R(0)), &env); return 0; }
/* 0x0046a324 resources(eax, edx, ebx, ecx): idle(0), file_load(edx) */
static uint32_t b_scene_load_resources(const uint32_t* a, int n) { return (uint32_t)mt_scene_load_resources(R(0), R(1), &env); }

void rt_bind_match_scene(void)
{
    rt_bind(0x00469f08u, b_scene_init, "sceneInit");
    rt_bind(0x0046a5fcu, b_scene_run, "sceneRun");
    rt_bind(0x0046a4f8u, b_scene_draw, "scene_redraw");
    rt_bind(0x00469e24u, b_scene_hit, "scene_obj_hit");
    rt_bind(0x00469c78u, b_scene_image_load, "scene_image_load");
    rt_bind(0x0046a324u, b_scene_load_resources, "scene_load_resources");
}

RT_BIND_AREA(match_scene)
