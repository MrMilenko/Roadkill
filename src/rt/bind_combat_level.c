/*
 * bind_combat_level.c: the small level entity, pickup and weapon object helpers (src/combat/cb_level.c),
 * called the way tools/emu/diff_combat_level.py called them: the port's arguments are EAX, EDX, EBX
 * in order; a "host pointer" argument (stats block, weapon block, combo block) is rt_ptr of the
 * register. Callbacks go to the original addresses with the registers the harness stubs read.
 * Functions the harness compared no return value for return 0.
 */
#include "rt_env.h"
#include "combat/cb_level.h"
#include "qol_arenas.h"

static WMem g_mem;

static void cbl_vis(void* u, int32_t id, int32_t slot, int32_t visible)
{
    (void)u;
    uint32_t a[3] = {(uint32_t)id, (uint32_t)slot, (uint32_t)visible};
    rt_call(0x004707b0u, a, 3);
}
static void cbl_rem(void* u, uint32_t obj) { (void)u; rt_call(0x004a14c0u, &obj, 1); }
static void cbl_add(void* u, uint32_t obj) { (void)u; rt_call(0x004a14a0u, &obj, 1); }
static void cbl_rotx(void* u, int32_t angle, uint32_t out)
{
    (void)u;
    uint32_t a[2] = {(uint32_t)angle, out};
    rt_call(0x0048c50cu, a, 2);
}
/* math_build_matrix 0x0048c360(angles, out): the port hands host i16[3]; the original passed a
 * game address (a local of its caller), so the angles go to a frame for the call. */
static void cbl_bm(void* u, const int16_t ang[3], uint32_t out)
{
    (void)u;
    uint32_t f = rt_frame_push(8);
    for (int i = 0; i < 3; i++)
        rt_w16(f + 2u * (uint32_t)i, (uint16_t)ang[i]);
    uint32_t a[2] = {f, out};
    rt_call(0x0048c360u, a, 2);
    rt_frame_pop(8);
}

static const CbLevelEnv* env(void)
{
    static CbLevelEnv e;
    if (!e.mem) {
        g_mem.n = 1;
        g_mem.r[0].addr = 0;
        g_mem.r[0].size = RT_SIZE;
        g_mem.r[0].host = rt_base;
        e.mem = &g_mem;
        e.pickup_set_visible = cbl_vis;
        e.model_remove = cbl_rem;
        e.model_add = cbl_add;
        e.rot_x = cbl_rotx;
        e.user = NULL;
        e.build_matrix = cbl_bm;
    }
    return &e;
}

#define A0 rt_arg(a, n, 0)
#define A1 rt_arg(a, n, 1)
#define A2 rt_arg(a, n, 2)

/* env only, no result */
#define V_E(name, fn) \
    static uint32_t name(const uint32_t* a, int n) { (void)a; (void)n; fn(env()); return 0; }
/* env + eax, no result */
#define V_EI(name, fn, T0) \
    static uint32_t name(const uint32_t* a, int n) { fn(env(), (T0)A0); return 0; }
#define V_EII(name, fn, T0, T1) \
    static uint32_t name(const uint32_t* a, int n) { fn(env(), (T0)A0, (T1)A1); return 0; }
/* env (+ eax) with a result */
#define R_E(name, fn) \
    static uint32_t name(const uint32_t* a, int n) { (void)a; (void)n; return (uint32_t)fn(env()); }
#define R_EI(name, fn, T0) \
    static uint32_t name(const uint32_t* a, int n) { return (uint32_t)fn(env(), (T0)A0); }
#define OBJ5(name, w) \
    static uint32_t name(const uint32_t* a, int n) { cb_obj5_register(env(), w, A0); return 0; }
#define OBJ4(name, w) \
    static uint32_t name(const uint32_t* a, int n) { cb_obj4_register(env(), w, A0); return 0; }

/* 0x0043a6d8(stats) */
static uint32_t b_stats_preset(const uint32_t* a, int n) { cb_stats_preset(rt_ptr(A0)); return 0; }
/* 0x004469bc(block, dir, n) */
static uint32_t b_weapon_select(const uint32_t* a, int n)
{
    cb_weapon_select(rt_ptr(A0), (int32_t)A1, (int32_t)A2);
    return 0;
}
V_E(b_wobj_reset, cb_weapon_objects_reset)
V_E(b_wobj_detach, cb_weapon_objects_detach)
V_E(b_ent_init, cb_level_entities_init)
/* 0x00449888(pos, kind, i) */
static uint32_t b_ent_add(const uint32_t* a, int n)
{
    cb_level_entity_add(env(), A0, (int16_t)A1, (int32_t)A2);
    return 0;
}
static uint32_t b_ent_kinds(const uint32_t* a, int n) { (void)a; (void)n; return (uint32_t)cb_level_entity_kinds(); }
V_EII(b_ent_show, cb_level_entity_show, int32_t, int32_t)
V_EII(b_ent_hide, cb_level_entity_hide, int32_t, int32_t)
R_E(b_tgt_field, cb_target_object_field)
R_E(b_tgt, cb_target_object)
/* + [gameplay] hidden_arenas (qol_arenas.h) */
static uint32_t b_counter_take(const uint32_t* a, int n)
{
    uint32_t i = A0;
    return qola_counter(i, (uint32_t)cb_pickup_counter_take(env(), i));
}
/* 0x0044ef60(block, id) */
static uint32_t b_combo_init(const uint32_t* a, int n) { cb_combo_block_init(rt_ptr(A0), A1); return 0; }
V_E(b_fx_clear, cb_fx_counter_clear)
static uint32_t b_nothing(const uint32_t* a, int n) { (void)a; (void)n; cb_nothing(); return 0; }
R_EI(b_render_mode, cb_render_mode_set, uint32_t)
V_E(b_objs_clear, cb_world_objects_clear)
OBJ5(b_obj5_a, CB_OBJ5_A)
OBJ5(b_obj5_b, CB_OBJ5_B)
OBJ5(b_obj5_c, CB_OBJ5_C)
OBJ5(b_obj5_d, CB_OBJ5_D)
OBJ5(b_obj5_e, CB_OBJ5_E)
V_EII(b_flicker, cb_light_flicker_set, int32_t, uint32_t)
V_EI(b_body_show, cb_car_body_show, int32_t)
V_EI(b_body_hide, cb_car_body_hide, int32_t)
V_EI(b_slam_arm, cb_slam_arm_register, uint32_t)
V_EII(b_carobj_reg, cb_car_object_register, int32_t, uint32_t)
R_EI(b_carobj_get, cb_car_object_get, int32_t)
R_EI(b_plat_h, cb_platform_height, int32_t)
V_EI(b_set_5c, cb_world_set_502f5c, uint32_t)
OBJ4(b_obj4_a, CB_OBJ4_A)
OBJ4(b_obj4_b, CB_OBJ4_B)
OBJ4(b_obj4_c, CB_OBJ4_C)
V_EI(b_tgt_reg, cb_target_register, uint32_t)
V_EII(b_pool_reg, cb_pool_object_register, uint32_t, int32_t)
R_E(b_pool_free, cb_pool_object_free)
V_EII(b_set_80, cb_world_set_502f80, uint8_t, int32_t)
R_EI(b_plat_pos, cb_platform_target_pos, int32_t)
R_EI(b_plat_vec, cb_platform_step_vec, int32_t)
R_EI(b_plat_ang, cb_platform_step_angle, int32_t)
V_EI(b_arm_rot, cb_slam_arm_rotate, int32_t)
V_EI(b_claw_rot, cb_slam_claw_rotate, int32_t)
static uint32_t b_combo_taps(const uint32_t* a, int n) { return cb_combo_taps(A0); }
R_EI(b_combo_count, cb_combo_count, uint32_t)

void rt_bind_combat_level(void)
{
    rt_bind(0x0043a6d8u, b_stats_preset, "carStatsBossPreset");
    rt_bind(0x004469bcu, b_weapon_select, "weaponSelect");
    rt_bind(0x00447d30u, b_wobj_reset, "weaponObjectsReset");
    rt_bind(0x00448098u, b_wobj_detach, "weaponObjectsDetach");
    rt_bind(0x00449844u, b_ent_init, "level_entities_init");
    rt_bind(0x00449888u, b_ent_add, "levelEntityAdd");
    rt_bind(0x00449eb4u, b_ent_kinds, "levelEntityKinds");
    rt_bind(0x00449ebcu, b_ent_show, "levelEntityShow");
    rt_bind(0x00449ef4u, b_ent_hide, "levelEntityHide");
    rt_bind(0x0044df04u, b_tgt_field, "targetObjectField");
    rt_bind(0x0044df10u, b_tgt, "targetObject");
    rt_bind(0x0044eee4u, b_counter_take, "pickupCounterTake");
    rt_bind(0x0044ef60u, b_combo_init, "comboBlockInit");
    rt_bind(0x0044f11cu, b_combo_taps, "car_combo_taps");
    rt_bind(0x0044f120u, b_combo_count, "car_combo_count");
    rt_bind(0x0045029cu, b_fx_clear, "fxCounterClear");
    rt_bind(0x00452d28u, b_nothing, "game_set_palette_mode");
    rt_bind(0x00452d48u, b_render_mode, "fontColorByteSet");
    rt_bind(0x00452d90u, b_objs_clear, "worldObjectsClear");
    rt_bind(0x00453400u, b_obj5_a, "carPartObjectRegisterA");
    rt_bind(0x00453428u, b_obj5_b, "carPartObjectRegisterB");
    rt_bind(0x00453450u, b_obj5_c, "carPartObjectRegisterC");
    rt_bind(0x00453478u, b_obj5_d, "carPartObjectRegisterD");
    rt_bind(0x004534a0u, b_obj5_e, "carPartObjectRegisterE");
    rt_bind(0x00453b10u, b_flicker, "lightFlickerStart");
    rt_bind(0x00453b68u, b_nothing, "level_init_vehicles");
    rt_bind(0x00453c00u, b_body_show, "carBodyObjectShow");
    rt_bind(0x00453c38u, b_body_hide, "carBodyObjectHide");
    rt_bind(0x00453e60u, b_slam_arm, "slamArmRegister");
    rt_bind(0x00453ed0u, b_arm_rot, "scene_rotate_b");
    rt_bind(0x00453f10u, b_claw_rot, "scene_rotate_a");
    rt_bind(0x00453f50u, b_carobj_reg, "carObjectRegister");
    rt_bind(0x00453f70u, b_carobj_get, "carObjectGet");
    rt_bind(0x00455fe8u, b_plat_h, "platformHeight");
    rt_bind(0x0045600cu, b_plat_pos, "platform_target_pos");
    rt_bind(0x00456f84u, b_plat_vec, "platform_step_vec");
    rt_bind(0x00456f9cu, b_plat_ang, "platform_step_angle");
    rt_bind(0x004570a0u, b_nothing, "nothing_004570a0");
    rt_bind(0x004570a4u, b_set_5c, "worldSet502f5c");
    rt_bind(0x00457118u, b_obj4_a, "fxRegisterSplashObject");
    rt_bind(0x0045721cu, b_obj4_b, "fxRegisterObject4B");
    rt_bind(0x004572f4u, b_obj4_c, "fxRegisterBlastObject");
    rt_bind(0x004573d4u, b_tgt_reg, "fxSetTargetHitObject");
    rt_bind(0x004573e4u, b_pool_reg, "fxPoolObjectRegister");
    rt_bind(0x004573f8u, b_pool_free, "fxPoolObjectFree");
    rt_bind(0x004575b8u, b_set_80, "worldSet502f80");
}

RT_BIND_AREA(combat_level)
