/*
 * bind_shell.c: the front end / registry ports of src/shell (sh_*.c, sh2_*.c), called the way
 * tools/emu/diff_shell_*.py and diff_sh2_*.py called them: the registers in order (EAX, EDX, EBX,
 * ECX), then the stack words; the ShEnv (src/shell/sh_mem.h) maps all of game memory and its
 * callbacks call the original's address with the original's argument order.
 *
 * Locals: the ports that hand a local buffer of theirs to another function (sh_screen's paths,
 * sh2_flow's level file paths and name buffer, sh2_events' debris direction, sh2_audio's mixer
 * structures) get a frame on the runtime's frame stack in ShEnv.local (the emulator harnesses
 * pass 0 there and match the placeholder SH_LOCAL(k) instead).
 *
 * Host state: sh_model builds a matrix with src/math/tm2math (tables loaded once here, from
 * TM2_DATA, default disc/files); sh_screen / sh_target / sh2_* draw random numbers from
 * src/core/tm2rand, which crt_rand 0x004899c2 / crt_srand 0x004899e6 are bound to (bind_core.c),
 * so there is one state for every caller.
 */
#include "rt_env.h"

#include <stdio.h>
#include <stdlib.h>

#include "math/tm2math.h"
#include "shell/sh_mem.h"
#include "shell/sh_misc.h"
#include "shell/sh_model.h"
#include "shell/sh_names.h"
#include "shell/sh_registry.h"
#include "shell/sh_screen.h"
#include "shell/sh_sound.h"
#include "shell/sh_target.h"
#include "shell/sh2_audio.h"
#include "shell/sh2_events.h"
#include "shell/sh2_flow.h"
#include "shell/sh3_ai_select.h"
#include "shell/sh3_car_select.h"
#include "shell/sh3_level_select.h"
#include "shell/sh3_net_wait.h"
#include "shell/sh3_single_menu.h"
#include "shell/sh3_title.h"
#include "shell/sh4_net_car.h"
#include "shell/sh4_session.h"
#include "shell/sh4_target_actors.h"
#include "shell/sh5_game_main.h"
#include "shell/sh5_level_load.h"
#include "shell/sh5_target_damage.h"
#include "qol_cars.h"

#define A(i) rt_arg(a, n, (i))

/* ---- the ShEnv ---- */

static uint32_t c(uint32_t fn, int k, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    uint32_t v[4] = {a0, a1, a2, a3};
    return rt_call(fn, v, k);
}

static void cb_anim_step(void* u, uint32_t node, uint32_t step) { (void)u; c(0x004a0c94u, 2, node, step, 0, 0); }
/* 0x004a1f1e is printf-like (cdecl, all on the stack): the format the original pushes (0x004e21c8,
 * sh_registry's target_register), type, instance */
static void cb_target_error(void* u, int32_t type, int32_t inst)
{
    (void)u;
    uint32_t v[3] = {0x004e21c8u, (uint32_t)type, (uint32_t)inst};
    rt_call(0x004a1f1eu, v, 3);
}
static int32_t cb_car_taken(void* u, int32_t car) { (void)u; return (int32_t)c(0x00463c9cu, 1, (uint32_t)car, 0, 0, 0); }
static uint32_t cb_frame_parity(void* u) { (void)u; return c(0x004931f4u, 0, 0, 0, 0, 0); }
static void cb_net_send(void* u, int32_t kind, uint32_t buf, int32_t len) { (void)u; c(0x00463e38u, 3, (uint32_t)kind, buf, (uint32_t)len, 0); }
static void cb_net_overflow(void* u) { (void)u; c(0x004632b0u, 0, 0, 0, 0, 0); }
static uint32_t cb_entity_alloc(void* u, int32_t i) { (void)u; return c(0x00492d90u, 1, (uint32_t)i, 0, 0, 0); }
static void cb_entity_init(void* u, uint32_t ent, uint32_t node) { (void)u; c(0x00492de4u, 2, ent, node, 0, 0); }
static void cb_entity_insert(void* u, uint32_t ent) { (void)u; c(0x00492c4cu, 1, ent, 0, 0, 0); }
static void cb_actor_place(void* u, uint32_t actor) { (void)u; c(0x004713f0u, 1, actor, 0, 0, 0); }
/* 0x004b9168: eax, edx, ebx, ecx, then the 4 stack words (a[4] first) */
static void cb_snd_play(void* u, const int32_t s[8])
{
    (void)u;
    uint32_t v[8];
    for (int i = 0; i < 8; i++) v[i] = (uint32_t)s[i];
    rt_call(0x004b9168u, v, 8);
}
static void cb_snd_stop(void* u, int32_t x, int32_t y, int32_t z) { (void)u; c(0x004b9240u, 3, (uint32_t)x, (uint32_t)y, (uint32_t)z, 0); }

static void math_tables(void)
{
    static int loaded;
    if (!loaded) {
        const char* dir = getenv("TM2_DATA");
        char s[1024], x[1024];
        if (!dir || !*dir) dir = "disc/files";
        snprintf(s, sizeof s, "%s/SINTAB.DAT", dir);
        snprintf(x, sizeof x, "%s/TM2.EXE", dir);
        if (tm2_math_load_tables(s, x) != 0) rt_stop("math tables (set TM2_DATA to the game's files)", 0);
        loaded = 1;
    }
}

/* the env without a local frame */
static ShEnv sh(void)
{
    static ShEnv base;
    static int init;
    if (!init) {
        memset(&base, 0, sizeof base);
        base.mem.n = 1;
        base.mem.r[0] = (WMemRegion){0, RT_SIZE, rt_base};
        base.anim_step = cb_anim_step;
        base.target_error = cb_target_error;
        base.car_taken = cb_car_taken;
        base.frame_parity = cb_frame_parity;
        base.net_send = cb_net_send;
        base.net_overflow = cb_net_overflow;
        base.entity_alloc = cb_entity_alloc;
        base.entity_init = cb_entity_init;
        base.entity_insert = cb_entity_insert;
        base.actor_place = cb_actor_place;
        base.snd_play = cb_snd_play;
        base.snd_stop = cb_snd_stop;
        base.call = rt_call_n_cb;
        init = 1;
    }
    return base;
}

/* a frame of the locals the ports name with sh_local / sh_local_at: 4 numbered buffers
 * (SH_LOCAL_STRIDE each) or sh2_audio's 0x2fc byte frame */
#define SH_FRAME (4u * SH_LOCAL_STRIDE)
static ShEnv sh_framed(void)
{
    ShEnv e = sh();
    e.local = rt_frame_push(SH_FRAME);
    return e;
}
static void sh_framed_done(void) { rt_frame_pop(SH_FRAME); }

static Sh2Env sh2(void) { Sh2Env e; memset(&e, 0, sizeof e); e.sh = sh(); return e; }
static Sh2Env sh2_framed(void) { Sh2Env e = sh2(); e.sh.local = rt_frame_push(SH_FRAME); return e; }

/* the env for the ports using tm2math (sh_model, sh_target) */
static ShEnv sh_m(void) { math_tables(); return sh(); }

/* a game address i32[3] as the host array the registry ports take */
static void pos3(uint32_t p, int32_t out[3])
{
    for (int i = 0; i < 3; i++) out[i] = (int32_t)rt_r32(p + 4u * (uint32_t)i);
}

/* ---- sh_misc.c ---- */

static uint32_t b_players_reset(const uint32_t* a, int n) { (void)a; (void)n; ShEnv e = sh(); sh_players_reset(&e); return 0; }
static uint32_t b_players_reset_ex(const uint32_t* a, int n) { ShEnv e = sh(); sh_players_reset_ex(&e, (uint8_t)A(0)); return 0; }
static uint32_t b_net_flush(const uint32_t* a, int n) { ShEnv e = sh(); sh_net_flush(&e, (uint8_t)A(0)); return 0; }
static uint32_t b_net_msg_ab(const uint32_t* a, int n) { ShEnv e = sh(); sh_net_msg_ab(&e, (uint8_t)A(0), (uint8_t)A(1)); return 0; }
static uint32_t b_net_msg_a32(const uint32_t* a, int n) { ShEnv e = sh(); sh_net_msg_a32(&e, (uint8_t)A(0), A(1)); return 0; }
static uint32_t b_net_msg_abc(const uint32_t* a, int n) { ShEnv e = sh(); sh_net_msg_abc(&e, (uint8_t)A(0), (uint8_t)A(1), (uint8_t)A(2)); return 0; }
static uint32_t b_net_msg_a16_32(const uint32_t* a, int n) { ShEnv e = sh(); sh_net_msg_a16_32(&e, (uint8_t)A(0), (uint16_t)A(1), A(2)); return 0; }
static uint32_t b_cmd_set_value(const uint32_t* a, int n) { ShEnv e = sh(); sh_cmd_set_value(&e, A(0)); return 0; }
static uint32_t b_cmd_step_back(const uint32_t* a, int n) { ShEnv e = sh(); return (uint32_t)sh_cmd_step_back(&e, A(0)); }
static uint32_t b_audio_defaults(const uint32_t* a, int n) { ShEnv e = sh(); sh_audio_defaults(&e, A(0)); return 0; }

/* ---- sh_names.c (the name lookups take the level / car in dl; eax is ignored) ---- */

static uint32_t b_game_type(const uint32_t* a, int n) { (void)a; (void)n; ShEnv e = sh(); return sh_game_type(&e); }
static uint32_t b_game_event_type(const uint32_t* a, int n) { (void)a; (void)n; ShEnv e = sh(); return sh_game_event_type(&e); }
static uint32_t b_game_mode(const uint32_t* a, int n) { ShEnv e = sh(); return sh_game_mode(&e, (int32_t)A(0)); }
static uint32_t b_level_display_name(const uint32_t* a, int n) { return sh_level_display_name((uint8_t)A(1)); }
static uint32_t b_level_short_name(const uint32_t* a, int n) { return sh_level_short_name((uint8_t)A(1)); }
static uint32_t b_car_display_name(const uint32_t* a, int n) { return sh_car_display_name((uint8_t)A(1)); }
static uint32_t b_tournament_opponents(const uint32_t* a, int n) { return sh_tournament_opponents((uint8_t)A(1)); }
static uint32_t b_car_unlocked(const uint32_t* a, int n) { ShEnv e = sh(); return (uint32_t)sh_car_unlocked(&e, (int32_t)A(0)); }
static uint32_t b_menu_car_name_text(const uint32_t* a, int n) { ShEnv e = sh(); return sh_menu_car_name_text(&e, (int32_t)A(0)); }
static uint32_t b_menu_random_car_text(const uint32_t* a, int n) { (void)a; (void)n; return sh_menu_random_car_text(); }
static uint32_t b_menu_level_name_text(const uint32_t* a, int n) { return sh_menu_level_name_text((uint8_t)A(0)); }

/* ---- sh_registry.c ---- */

static uint32_t b_reg_set_step(const uint32_t* a, int n) { ShEnv e = sh(); sh_registry_set_step(&e, (int32_t)A(0), (int32_t)A(1), A(2)); return 0; }
static uint32_t b_reg_choice_register(const uint32_t* a, int n) { ShEnv e = sh(); sh_registry_choice_register(&e, A(0), (int32_t)A(1)); return 0; }
static uint32_t b_reg_choice_state(const uint32_t* a, int n) { ShEnv e = sh(); sh_registry_choice_state(&e, (int32_t)A(0), A(1)); return 0; }
static uint32_t b_reg_scene_choice_add(const uint32_t* a, int n) { ShEnv e = sh(); sh_registry_scene_choice_add(&e, A(0)); return 0; }
static uint32_t b_reg_pickup_add(const uint32_t* a, int n) { ShEnv e = sh(); sh_registry_pickup_add(&e, A(0), (int32_t)A(1)); return 0; }
static uint32_t b_registries_reset(const uint32_t* a, int n) { (void)a; (void)n; ShEnv e = sh(); sh_registries_reset(&e); return 0; }
static uint32_t b_reg_set_car_template(const uint32_t* a, int n) { ShEnv e = sh(); sh_registry_set_car_template(&e, A(0)); return 0; }
static uint32_t b_car_choice_build(const uint32_t* a, int n) { ShEnv e = sh(); sh_car_choice_build(&e, (int32_t)A(0), A(1)); return 0; }
static uint32_t b_target_object(const uint32_t* a, int n) { ShEnv e = sh(); return sh_target_object(&e, (int32_t)A(0)); }
static uint32_t b_target_pool_reset(const uint32_t* a, int n) { (void)a; (void)n; ShEnv e = sh(); sh_target_pool_reset(&e); return 0; }
static uint32_t b_target_actors_reset(const uint32_t* a, int n) { (void)a; (void)n; ShEnv e = sh(); sh_target_actors_reset(&e); return 0; }
static uint32_t b_target_register(const uint32_t* a, int n)
{
    ShEnv e = sh();
    int32_t p[3];
    pos3(A(2), p);
    sh_target_register(&e, (int32_t)A(0), (int32_t)A(1), p);
    return 0;
}
static uint32_t b_target_pool(const uint32_t* a, int n) { (void)a; (void)n; return sh_target_pool(); }
static uint32_t b_target_track_pos(const uint32_t* a, int n)
{
    ShEnv e = sh();
    int32_t p[3];
    pos3(A(1), p);
    sh_target_track_pos(&e, (int32_t)A(0), p);
    return 0;
}

/* ---- sh_target.c ---- */

static uint32_t b_target_actor_spawn(const uint32_t* a, int n) { ShEnv e = sh_m(); sh_target_actor_spawn(&e, (int32_t)A(0), (int16_t)A(1), A(2)); return 0; }
static uint32_t b_target_actor_hit(const uint32_t* a, int n) { ShEnv e = sh_m(); sh_target_actor_hit(&e, (int32_t)A(0), A(1)); return 0; }
static uint32_t b_target_timed_event_init(const uint32_t* a, int n) { ShEnv e = sh_m(); sh_target_timed_event_init(&e, A(0)); return 0; }
static uint32_t b_target_level_event_init(const uint32_t* a, int n) { (void)a; (void)n; ShEnv e = sh_m(); sh_target_level_event_init(&e); return 0; }
static uint32_t b_target_player_event_init(const uint32_t* a, int n) { ShEnv e = sh_m(); return (uint32_t)sh_target_player_event_init(&e, A(0), (int32_t)A(1)); }
static uint32_t b_target_event_nop(const uint32_t* a, int n) { (void)a; (void)n; sh_target_event_nop(); return 0; }

/* ---- sh_sound.c: eax, edx, ebx, ecx, then 2 (3 for play_car: the third is unused) stack words ---- */

#define I(i) ((int32_t)A(i))
static uint32_t b_sound_play_car(const uint32_t* a, int n) { ShEnv e = sh(); sh_sound_play_car(&e, I(0), I(1), I(2), I(3), I(4), I(5)); return 0; }
static uint32_t b_sound_stop(const uint32_t* a, int n) { ShEnv e = sh(); sh_sound_stop(&e, I(0), I(1), I(2)); return 0; }
static uint32_t b_sound_play_loop(const uint32_t* a, int n) { ShEnv e = sh(); sh_sound_play_loop(&e, I(0), A(1), I(2), I(3), I(4), I(5)); return 0; }
static uint32_t b_sound_stop_loop(const uint32_t* a, int n) { ShEnv e = sh(); sh_sound_stop_loop(&e, I(0), A(1), I(2)); return 0; }
static uint32_t b_sound_play_generic(const uint32_t* a, int n) { ShEnv e = sh(); sh_sound_play_generic(&e, I(0), I(1), I(2), I(3), I(4), I(5)); return 0; }
static uint32_t b_sound_stop_generic(const uint32_t* a, int n) { ShEnv e = sh(); sh_sound_stop_generic(&e, I(0), I(1), I(2)); return 0; }

/* ---- sh_model.c ---- */

static uint32_t b_model_display_update(const uint32_t* a, int n)
{
    ShEnv e = sh_m();
    sh_model_display_update(&e, A(0), A(1));
    return 0;
}

/* ---- sh_screen.c ---- */

static uint32_t b_movie_skip_check(const uint32_t* a, int n) { ShEnv e = sh(); return (uint32_t)sh_movie_skip_check(&e, (int32_t)A(0)); }
static uint32_t b_skip_check(const uint32_t* a, int n) { ShEnv e = sh(); return (uint32_t)sh_skip_check(&e, A(0)); }
/* 0x004751dc: eax (the game state) is ignored, the movie name in edx */
static uint32_t b_play_movie(const uint32_t* a, int n)
{
    ShEnv e = sh_framed();
    uint32_t r = (uint32_t)sh_play_movie(&e, A(1));
    sh_framed_done();
    return r;
}
static uint32_t b_display_init_once(const uint32_t* a, int n) { (void)a; (void)n; ShEnv e = sh(); return (uint32_t)sh_display_init_once(&e); }
static uint32_t b_clear_624db8(const uint32_t* a, int n) { (void)a; (void)n; ShEnv e = sh(); sh_clear_624db8(&e); return 0; }
static uint32_t b_match_tournament_start(const uint32_t* a, int n) { (void)a; (void)n; ShEnv e = sh(); return sh_match_tournament_start(&e); }
#define SCREEN0(name, fn)                                                    \
    static uint32_t name(const uint32_t* a, int n)                           \
    {                                                                        \
        (void)a; (void)n;                                                    \
        ShEnv e = sh_framed();                                               \
        uint32_t r = fn(&e);                                                 \
        sh_framed_done();                                                    \
        return r;                                                            \
    }
SCREEN0(b_screen_save, sh_screen_save)
SCREEN0(b_screen_password, sh_screen_password)
SCREEN0(b_screen_network, sh_screen_network)
SCREEN0(b_screen_network_a, sh_screen_network_a)
SCREEN0(b_screen_network_b, sh_screen_network_b)
SCREEN0(b_screen_audio_cfg, sh_screen_audio_cfg)
SCREEN0(b_screen_history_movie, sh_screen_history_movie)
SCREEN0(b_screen_match_end, sh_screen_match_end)
/* play_dark_tooth: the ending table has no car 14 (qol_cars.h) */
static uint32_t b_screen_ending(const uint32_t* a, int n)
{
    (void)a; (void)n;
    qolc_ending(1);
    ShEnv e = sh_framed();
    uint32_t r = sh_screen_ending(&e);
    sh_framed_done();
    qolc_ending(0);
    return r;
}
static uint32_t b_screen_input_cfg(const uint32_t* a, int n)
{
    ShEnv e = sh_framed();
    uint32_t r = sh_screen_input_cfg(&e, A(0), A(1));
    sh_framed_done();
    return r;
}
static uint32_t b_screen_credits(const uint32_t* a, int n)
{
    ShEnv e = sh_framed();
    uint32_t r = sh_screen_credits(&e, A(0));
    sh_framed_done();
    return r;
}
static uint32_t b_cmd_post_when_ready(const uint32_t* a, int n) { ShEnv e = sh(); sh_cmd_post_when_ready(&e, A(0)); return 0; }
static uint32_t b_cmd_button_events(const uint32_t* a, int n) { ShEnv e = sh(); sh_cmd_button_events(&e, A(0)); return 0; }
static uint32_t b_render_reset(const uint32_t* a, int n) { (void)a; (void)n; ShEnv e = sh(); sh_render_reset(&e); return 0; }
static uint32_t b_audio_init_user_volume(const uint32_t* a, int n) { ShEnv e = sh(); sh_audio_init_user_volume(&e, A(0)); return 0; }
static uint32_t b_round_start_music(const uint32_t* a, int n) { ShEnv e = sh(); sh_round_start_music(&e, (uint8_t)A(0)); return 0; }
static uint32_t b_audio_commit(const uint32_t* a, int n) { ShEnv e = sh(); sh_audio_commit(&e, A(0)); return 0; }
static uint32_t b_timer_wait(const uint32_t* a, int n) { ShEnv e = sh(); return (uint32_t)sh_timer_wait(&e, A(0), (int32_t)A(1)); }

/* ---- sh2_flow.c ---- */

static uint32_t b_sound_init(const uint32_t* a, int n) { (void)a; (void)n; Sh2Env e = sh2(); sh2_sound_init(&e); return 0; }
static uint32_t b_sound_stop_if_on(const uint32_t* a, int n) { Sh2Env e = sh2(); sh2_sound_stop_if_on(&e, A(0)); return 0; }
static uint32_t b_sound_update(const uint32_t* a, int n) { Sh2Env e = sh2(); return sh2_sound_update(&e, A(0)); }
static uint32_t b_overlay_draw(const uint32_t* a, int n) { Sh2Env e = sh2(); sh2_overlay_draw(&e, A(0)); return 0; }
static uint32_t b_shell_loop(const uint32_t* a, int n) { (void)a; (void)n; Sh2Env e = sh2(); sh2_shell_loop(&e); return 0; }
static uint32_t b_level_files_load(const uint32_t* a, int n)
{
    Sh2Env e = sh2_framed();
    sh2_level_files_load(&e, A(0));
    sh_framed_done();
    qolc_scene_loaded(A(0));   /* [gameplay] play_dark_tooth (qol_cars.h) */
    return 0;
}
static uint32_t b_challenge_result(const uint32_t* a, int n)
{
    Sh2Env e = sh2();
    sh2_challenge_result(&e, A(0), (uint8_t)A(1), (int16_t)A(2), (int16_t)A(3));
    return 0;
}
/* 0x0047506c (ret 0x24): the four registers, then the 9 stack words */
static uint32_t b_message_pages(const uint32_t* a, int n)
{
    Sh2Env e = sh2();
    uint32_t t[9];
    for (int i = 0; i < 9; i++) t[i] = A(4 + i);
    sh2_message_pages(&e, A(0), A(1), A(2), A(3), t);
    return 0;
}
static uint32_t b_session_cars(const uint32_t* a, int n) { Sh2Env e = sh2(); sh2_session_cars(&e, A(0)); return 0; }
static uint32_t b_attract_demo(const uint32_t* a, int n) { Sh2Env e = sh2(); return sh2_attract_demo(&e, A(0)); }
static uint32_t b_net_player_names(const uint32_t* a, int n)
{
    Sh2Env e = sh2_framed();
    uint32_t r = sh2_net_player_names(&e, A(0), A(1));
    sh_framed_done();
    return r;
}

/* ---- sh2_audio.c ---- */

static uint32_t b_audio_cfg_destroy(const uint32_t* a, int n) { Sh2Env e = sh2(); return sh2_audio_cfg_destroy(&e, A(0), A(1)); }
static uint32_t b_audio_read_registry(const uint32_t* a, int n)
{
    Sh2Env e = sh2_framed();
    sh2_audio_read_settings(&e, A(0));
    sh_framed_done();
    return 0;
}
static uint32_t b_audio_apply_volume(const uint32_t* a, int n)
{
    Sh2Env e = sh2_framed();
    sh2_audio_apply_volume(&e, A(0), A(1));
    sh_framed_done();
    return 0;
}

/* ---- sh2_events.c ---- */

static uint32_t b_event_destroyed_fx(const uint32_t* a, int n)
{
    (void)a; (void)n;
    Sh2Env e = sh2_framed();
    sh2_event_destroyed_fx(&e);
    sh_framed_done();
    return 0;
}
static uint32_t b_event_special(const uint32_t* a, int n) { (void)a; (void)n; Sh2Env e = sh2(); sh2_event_special_fx(&e); return 0; }
static uint32_t b_target_player_events(const uint32_t* a, int n) { (void)a; (void)n; Sh2Env e = sh2(); sh2_target_player_events(&e); return 0; }
static uint32_t b_car_net_state(const uint32_t* a, int n) { Sh2Env e = sh2(); sh2_car_net_state(&e, A(0), (uint8_t)A(1)); return 0; }
static uint32_t b_target_actor_place(const uint32_t* a, int n) { Sh2Env e = sh2(); sh2_target_actor_place(&e, A(0)); return 0; }

/* ---- sh3_* / sh4_* / sh5_*: the screens and the session keep their locals in a frame at game
 * addresses (fb, the size each header gives; tools/emu/diff_sh3_common.py) ---- */

#define FRAMED(size, call)               \
    do {                                 \
        ShEnv e = sh();                  \
        uint32_t fb = rt_frame_push(size); \
        r = (call);                      \
        rt_frame_pop(size);              \
    } while (0)

/* 0x0047e624: esi_in is the caller's esi, returned when no opponent is left to pick; the only
 * caller, the shell loop 0x00485338, holds the title screen 0x004780fc there all along */
static uint32_t b_screen_ai_select(const uint32_t* a, int n) { (void)a; (void)n; uint32_t r; FRAMED(0x960u, sh3_ai_select(&e, fb, SH_SCREEN_TITLE)); return r; }
/* car select: [gameplay] unlock_hidden_cars / play_dark_tooth around the verified screen (qol_cars.h) */
static uint32_t b_screen_car_select(const uint32_t* a, int n)
{
    (void)a; (void)n;
    uint32_t r;
    qolc_car_select_enter();
    FRAMED(0xa70u, sh3_car_select(&e, fb));
    return r;
}
static uint32_t b_screen_level_select(const uint32_t* a, int n) { uint32_t r; FRAMED(0x748u, sh3_level_select(&e, fb, A(0))); return r; }
static uint32_t b_net_car_wait(const uint32_t* a, int n) { (void)a; (void)n; uint32_t r; FRAMED(0x328u, sh3_net_wait(&e, fb)); return r; }
static uint32_t b_screen_single_menu(const uint32_t* a, int n) { (void)a; (void)n; uint32_t r; FRAMED(0x644u, sh3_single_menu(&e, fb)); return r; }
static uint32_t b_screen_title(const uint32_t* a, int n) { (void)a; (void)n; uint32_t r; FRAMED(0x5a8u, sh3_title(&e, fb)); return r; }
/* 0x00473714(src, len, i) */
static uint32_t b_net_car_state_apply(const uint32_t* a, int n) { uint32_t r; FRAMED(0x20u, (sh4_net_car_apply(&e, fb, A(0), (int32_t)A(1), A(2)), 0)); return r; }
/* 0x00475d34(game state) */
static uint32_t b_session_run(const uint32_t* a, int n) { uint32_t r; FRAMED(0x50u, (uint32_t)sh4_session_run(&e, fb, A(0))); return r; }
static uint32_t b_target_actors_update(const uint32_t* a, int n) { (void)a; (void)n; uint32_t r; FRAMED(0x28u, (sh4_target_actors_update(&e, fb), 0)); return r; }
/* 0x00477124: ebx_in is the caller's ebx, the display init's third argument; the only caller,
 * WinMain 0x004a8b04, holds the window handle there (0x004a6dcc of the window object 0x00bdfcd0,
 * its +0) */
static uint32_t b_game_flow_init(const uint32_t* a, int n) { (void)a; (void)n; uint32_t r; FRAMED(0x3b4u, sh5_game_main(&e, fb, rt_r32(0x00bdfcd0u))); return r; }
/* 0x004753a4(game state) */
static uint32_t b_level_load_assets(const uint32_t* a, int n)
{
    uint32_t r;
    qolc_level_load(1);   /* [gameplay] play_dark_tooth (qol_cars.h) */
    FRAMED(0x154u, (sh5_level_load(&e, fb, A(0)), 0));
    qolc_level_load(0);
    return r;
}
/* 0x00470d3c(type, instance, damage): the original's al */
static uint32_t b_target_damage(const uint32_t* a, int n) { uint32_t r; FRAMED(0x0cu, sh5_target_damage(&e, fb, (int32_t)A(0), (int32_t)A(1), A(2))); return r; }

void rt_bind_shell(void)
{
    /* sh3_* / sh4_* / sh5_* */
    rt_bind(0x0047e624u, b_screen_ai_select, "screenAiSelect");
    rt_bind(0x0047baa0u, b_screen_car_select, "screen_car_select");
    rt_bind(0x0047a388u, b_screen_level_select, "screen_level_select");
    rt_bind(0x0047e010u, b_net_car_wait, "net_car_wait_screen");
    rt_bind(0x00478edcu, b_screen_single_menu, "screenSinglePlayerMenu");
    rt_bind(0x004780fcu, b_screen_title, "screenTitle");
    rt_bind(0x00473714u, b_net_car_state_apply, "net_car_state_apply");
    rt_bind(0x00475d34u, b_session_run, "sessionRun");
    rt_bind(0x00471b14u, b_target_actors_update, "targetActorsUpdate");
    rt_bind(0x00477124u, b_game_flow_init, "game_flow_init");
    rt_bind(0x004753a4u, b_level_load_assets, "level_load_assets");
    rt_bind(0x00470d3cu, b_target_damage, "targetDamage");
    /* sh_misc.c */
    rt_bind(0x00473160u, b_players_reset, "playersReset");
    rt_bind(0x00473168u, b_players_reset_ex, "playersResetEx");
    rt_bind(0x00473534u, b_net_flush, "netFlush");
    rt_bind(0x00473568u, b_net_msg_ab, "netMsgAB");
    rt_bind(0x004735b4u, b_net_msg_a32, "netMsgA32");
    rt_bind(0x004735fcu, b_net_msg_abc, "netMsgABC");
    rt_bind(0x00473650u, b_net_msg_a16_32, "netMsgA16_32");
    rt_bind(0x00485880u, b_cmd_set_value, "menuCmdSetValue");
    rt_bind(0x004858a0u, b_cmd_step_back, "menuCmdStepBack");
    rt_bind(0x00486b44u, b_audio_defaults, "audioDefaults");
    /* sh_names.c */
    rt_bind(0x00474500u, b_game_type, "game_get_type");
    rt_bind(0x00474508u, b_game_event_type, "game_get_event_type");
    rt_bind(0x00474510u, b_game_mode, "game_get_mode");
    rt_bind(0x00474b8cu, b_level_display_name, "levelDisplayName");
    rt_bind(0x00474c1cu, b_level_short_name, "levelShortName");
    rt_bind(0x00474d68u, b_car_display_name, "carDisplayName");
    rt_bind(0x00474e0cu, b_tournament_opponents, "tournamentOpponentCount");
    rt_bind(0x0047770cu, b_car_unlocked, "shellSetUnlockFlagInit");
    rt_bind(0x00477b2cu, b_menu_car_name_text, "menuCarNameText");
    rt_bind(0x00477b64u, b_menu_random_car_text, "menuRandomCarText");
    rt_bind(0x00477b74u, b_menu_level_name_text, "menuLevelNameText");
    /* sh_registry.c */
    rt_bind(0x00470320u, b_reg_set_step, "objRegistrySetStep");
    rt_bind(0x00470650u, b_reg_choice_register, "objRegistryChoiceRegister");
    rt_bind(0x0047068cu, b_reg_choice_state, "objRegistryChoiceState");
    rt_bind(0x004706c4u, b_reg_scene_choice_add, "objRegistrySceneChoiceAdd");
    rt_bind(0x00470770u, b_reg_pickup_add, "objRegistryPickupAdd");
    rt_bind(0x004707f8u, b_registries_reset, "objRegistriesReset");
    rt_bind(0x00470938u, b_reg_set_car_template, "objRegistrySetCarTemplate");
    rt_bind(0x00470964u, b_car_choice_build, "carChoiceBuild");
    rt_bind(0x00470b14u, b_target_object, "targetGetLevelObject");
    rt_bind(0x00470b20u, b_target_pool_reset, "targetPoolReset");
    rt_bind(0x00470bc0u, b_target_register, "targetRegister");
    rt_bind(0x00470d08u, b_target_pool, "targetPoolGet");
    rt_bind(0x0047129cu, b_target_actors_reset, "targetActorsReset");
    rt_bind(0x00472e50u, b_target_track_pos, "targetSetTrackVehiclePos");
    /* sh_target.c */
    rt_bind(0x004712c8u, b_target_actor_spawn, "targetActorSpawn");
    rt_bind(0x004723bcu, b_target_actor_hit, "targetActorHit");
    rt_bind(0x004724c4u, b_target_timed_event_init, "targetTimedEventInit");
    rt_bind(0x0047274cu, b_target_level_event_init, "targetLevelEventInit");
    rt_bind(0x00472a14u, b_target_player_event_init, "targetPlayerEventInit");
    rt_bind(0x00472b70u, b_target_event_nop, "targetEventNop");
    /* sh_sound.c */
    rt_bind(0x00472ed0u, b_sound_play_car, "soundPlayCar");
    rt_bind(0x00472f74u, b_sound_stop, "soundMgrStop");
    rt_bind(0x00472f8cu, b_sound_play_loop, "soundPlayLoop");
    rt_bind(0x0047304cu, b_sound_stop_loop, "soundStopLoop");
    rt_bind(0x004730c8u, b_sound_play_generic, "soundPlayGeneric");
    rt_bind(0x00473104u, b_sound_stop_generic, "soundMgrStopGeneric");
    /* sh_model.c */
    rt_bind(0x0047791cu, b_model_display_update, "modelDisplayUpdate");
    /* sh_screen.c */
    rt_bind(0x004751a4u, b_movie_skip_check, "movieSkipCheckDelayed");
    rt_bind(0x004777f4u, b_skip_check, "skipCheck");
    rt_bind(0x004751dcu, b_play_movie, "screenClearScreen");
    rt_bind(0x004775a8u, b_display_init_once, "displayInitOnce");
    rt_bind(0x004775f8u, b_clear_624db8, "clear624db8");
    rt_bind(0x00480c08u, b_match_tournament_start, "match_tournament_start");
    rt_bind(0x0047a03cu, b_screen_save, "screenSave");
    rt_bind(0x0047a0a4u, b_screen_password, "screen_password_entry");
    rt_bind(0x0047a18cu, b_screen_network, "screen_network");
    rt_bind(0x0047a1f8u, b_screen_network_a, "screen_network_sub_a");
    rt_bind(0x0047a274u, b_screen_network_b, "screen_network_sub_b");
    rt_bind(0x0047dfecu, b_cmd_post_when_ready, "menuCmdPostWhenReady");
    rt_bind(0x00477aa0u, b_cmd_button_events, "menuCmdButtonEvents");
    rt_bind(0x00484230u, b_screen_input_cfg, "screen_options_input_cfg");
    rt_bind(0x00484250u, b_screen_audio_cfg, "screen_options_audio");
    rt_bind(0x00485398u, b_render_reset, "renderReset");
    rt_bind(0x00486870u, b_audio_init_user_volume, "audio_init_user_volume");
    rt_bind(0x00473f60u, b_round_start_music, "roundStartMusic");
    rt_bind(0x004868c0u, b_audio_commit, "audio_commit_volume");
    rt_bind(0x00474154u, b_timer_wait, "timer_wait");
    rt_bind(0x00484fbcu, b_screen_history_movie, "screenHistoryMovie");
    rt_bind(0x00485040u, b_screen_credits, "screenCredits");
    rt_bind(0x00485100u, b_screen_match_end, "screenMatchEnd");
    rt_bind(0x00485290u, b_screen_ending, "screenEnding");
    /* sh2_flow.c */
    rt_bind(0x00472ec0u, b_sound_init, "sound_init");
    rt_bind(0x0047312cu, b_sound_stop_if_on, "sound_stop_if_on");
    rt_bind(0x0047314cu, b_sound_update, "sound_manager_update_thunk");
    rt_bind(0x00484260u, b_overlay_draw, "overlay_draw");
    rt_bind(0x00485338u, b_shell_loop, "game_flow_loop");
    rt_bind(0x004740a8u, b_level_files_load, "level_files_load");
    rt_bind(0x00474ee4u, b_challenge_result, "screenChallengeResult");
    rt_bind(0x0047506cu, b_message_pages, "hud_show_message");
    rt_bind(0x00475af8u, b_session_cars, "car_models_load");
    rt_bind(0x00477b90u, b_attract_demo, "attract_demo");
    rt_bind(0x0047b934u, b_net_player_names, "net_player_names_draw");
    /* sh2_audio.c */
    rt_bind(0x0048620cu, b_audio_cfg_destroy, "audio_cfg_destroy");
    rt_bind(0x004868ccu, b_audio_read_registry, "audio_read_registry");
    rt_bind(0x00486bb4u, b_audio_apply_volume, "audio_apply_volume");
    /* sh2_events.c */
    rt_bind(0x0047259cu, b_event_destroyed_fx, "event_vehicle_destroyed_fx");
    rt_bind(0x004727a8u, b_event_special, "event_special");
    rt_bind(0x00472b74u, b_target_player_events, "targetPlayerEventsUpdate");
    rt_bind(0x00473254u, b_car_net_state, "car_net_state_snapshot");
    rt_bind(0x004713f0u, b_target_actor_place, "targetActorPlace");
}

RT_BIND_AREA(shell)
