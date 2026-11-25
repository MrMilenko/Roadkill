/*
 * bind_engine_win.c: engine ports of the en4 / en7 / en8 groups (window, DirectInput / DirectSound
 * objects, dialogs, the game loop) tested with the T(...) harnesses but missing from the
 * generated table (bind_gen_t.c): each is fn(env, <registers in the T order>, <stack words>), on
 * a fresh En2Env (rt_en2env).
 */
#include "rt_env.h"
#include "engine/en4_io.h"
#include "engine/en4_obj.h"
#include "engine/en7_a.h"
#include "engine/en7_b.h"
#include "engine/en7_d.h"
#include "engine/en7_e.h"
#include "engine/en8_wndproc.h"

#define A(i) rt_arg(a, n, (i))
#define E1(name, call) \
    static uint32_t name(const uint32_t* a, int n) \
    { (void)a; (void)n; En2Env e = rt_en2env(); uint32_t r = (call); rt_en2env_done(); return r; }

E1(b_destroy_window, en4_destroy_window(&e, A(0)))                       /* 0x004a6c98 */
E1(b_dialog_show, en4_dialog_show(&e, A(0), A(1), A(2)))                 /* 0x004a9de0 */
E1(b_dsound_sound_free, en4_dsound_sound_free(&e, A(0)))                 /* 0x004aa5b8 */
E1(b_thread_stop, en4_thread_stop(&e, A(0)))                             /* 0x004abd38 */
E1(b_button_key_activate, en4_button_key_activate(&e, A(0), A(1), A(2))) /* 0x004a6020 */
E1(b_dinput_release, en4_dinput_device_release(&e, A(0)))                /* 0x004a7c94 */
E1(b_dinput_poll, en4_dinput_device_poll(&e, A(0)))                      /* 0x004a8018 */
E1(b_dsound_buffer_release, en4_dsound_buffer_release(&e, A(0)))         /* 0x004aa638 */
E1(b_window_paint, en4_window_paint(&e, A(0)))                           /* 0x004ac9c4 */
E1(b_session_find_player, en4_session_find_player(&e, A(0), A(1)))      /* 0x004acce0 */
/* 0x004a490c (EAX, EDX, EBX, ECX, 5 stack words) */
E1(b_create_text_button, en7_widget_create_text_button(&e, A(0), A(1), A(2), A(3), A(4), A(5), A(6), A(7), A(8)))
E1(b_game_loop_run, en7_game_loop_run(&e))                               /* 0x004a8e6c */
E1(b_widget_navigate, en7_widget_navigate(&e, A(0), A(1)))               /* 0x004a798c */

/* 0x00497d2c (EAX o, EDX list box). The harness scripted the list box answers; here the real
 * callees answer. */
static uint32_t b_listbox_sync(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();
    en7_session_listbox_sync(&e, A(0), A(1));
    rt_en2env_done();
    return 0;
}
/* 0x004ad624 (EAX r, EDX g, EBX b) */
static uint32_t b_screen_rect_outline(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();
    en7_screen_rect_outline(&e, A(0), A(1), A(2));
    rt_en2env_done();
    return 0;
}
/* 0x0049e4ac */
static uint32_t b_keyboard_read_state(const uint32_t* a, int n)
{
    (void)a; (void)n;
    En2Env e = rt_en2env();
    en7_keyboard_read_state(&e);
    rt_en2env_done();
    return 0;
}

/* 0x004a1f1e debug_printf(fmt, ...), cdecl varargs, no register arguments: stack word 0 = fmt
 * (a[0]), the rest variadic (a[1 ..]). The
 * port takes the GAME address of the first variadic word (under the emulator: the caller's
 * stack). Real equivalent: the caller's variadic words (all the stack words it passed after fmt)
 * are copied to a frame and its address is passed. */
static uint32_t b_debug_printf(const uint32_t* a, int n)
{
    int nv = n > 1 ? n - 1 : 0;
    uint32_t sz = 4u * (uint32_t)(nv + 1), va = rt_frame_push(sz), i;
    for (i = 0; i < (uint32_t)nv; i++)
        rt_w32(va + 4 * i, a[1 + i]);
    En2Env e = rt_en2env();
    uint32_t r = en7_debug_printf(&e, A(0), va);
    rt_en2env_done();
    rt_frame_pop(sz);
    return r;
}

/* 0x004a7154 window_dispatch (EAX obj, EDX hwnd, EBX msg, ECX wParam, stack lParam; ret 4). The
 * MENUITEMINFO / POINT / MSG locals (0x64 bytes) live at En2Env.scratch (0x800 here). esi_in:
 * the caller's esi, returned on the ORIGINAL BUG paths (see en8_wndproc.h): 0 here. */
static uint32_t b_window_dispatch(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();
    uint32_t r = en8_wndproc(&e, A(0), A(1), A(2), A(3), 0, A(4));
    rt_en2env_done();
    return r;
}

void rt_bind_engine_win(void)
{
    rt_bind(0x004a6c98u, b_destroy_window, "win32_destroy_window");
    rt_bind(0x004a9de0u, b_dialog_show, "dialog_show");
    rt_bind(0x004aa5b8u, b_dsound_sound_free, "dsound_sound_free");
    rt_bind(0x004abd38u, b_thread_stop, "thread_stop");
    rt_bind(0x004a6020u, b_button_key_activate, "button_key_activate");
    rt_bind(0x004a7c94u, b_dinput_release, "dinput_device_release");
    rt_bind(0x004a8018u, b_dinput_poll, "dinput_device_poll");
    rt_bind(0x004aa638u, b_dsound_buffer_release, "dsound_buffer_release");
    rt_bind(0x004ac9c4u, b_window_paint, "window_paint");
    rt_bind(0x004acce0u, b_session_find_player, "session_find_player");
    rt_bind(0x004a1f1eu, b_debug_printf, "debug_printf");
    rt_bind(0x004a490cu, b_create_text_button, "widget_create_text_button");
    rt_bind(0x004a8e6cu, b_game_loop_run, "game_loop_run");
    rt_bind(0x00497d2cu, b_listbox_sync, "session_listbox_sync");
    rt_bind(0x004ad624u, b_screen_rect_outline, "screen_rect_outline");
    rt_bind(0x0049e4acu, b_keyboard_read_state, "keyboard_read_state");
    rt_bind(0x004a798cu, b_widget_navigate, "widget_navigate");
    rt_bind(0x004a7154u, b_window_dispatch, "window_dispatch");
}

RT_BIND_AREA(engine_win)
