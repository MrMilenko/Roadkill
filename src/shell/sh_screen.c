/*
 * sh_screen.c: small front end screens and helpers (see sh_screen.h).
 */
#include "sh_screen.h"
#include "../game/scene.h"
#include "../core/tm2rand.h"
#include "sh_names.h"

#define BG 0x009f6c20u          /* the shell's background / render state */
#define GS 0x00624ea8u          /* game state */

static uint32_t call(const ShEnv* e, uint32_t fn, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) {
    uint32_t a[6] = {a0, a1, a2, a3, a4, a5};
    return e->call ? e->call(e->user, fn, n, a) : 0;
}
#define C0(fn) call(e, fn, 0, 0, 0, 0, 0, 0, 0)
#define C1(fn, a) call(e, fn, 1, a, 0, 0, 0, 0, 0)
#define C2(fn, a, b) call(e, fn, 2, a, b, 0, 0, 0, 0)
#define C3(fn, a, b, c) call(e, fn, 3, a, b, c, 0, 0, 0)
#define C4(fn, a, b, c, d) call(e, fn, 4, a, b, c, d, 0, 0)

static int32_t ready_13_11(const ShEnv* e) {
    if ((uint8_t)C2(0x004936acu, 0x13, 0)) return 1;          /* input_check_ready, al */
    if ((uint8_t)C2(0x004936acu, 0x11, 0)) return 1;
    return 0;
}

int32_t sh_movie_skip_check(const ShEnv* e, int32_t frame) {
    C1(0x00493748u, (uint32_t)frame);                          /* input_poll */
    if (frame < 0x23) return 0;
    return ready_13_11(e);
}

int32_t sh_skip_check(const ShEnv* e, uint32_t arg) {
    C1(0x00493748u, arg);
    return ready_13_11(e);
}

int32_t sh_play_movie(const ShEnv* e, uint32_t name) {
    C3(0x00499408u, sh_local(e, 0), name, 6);                     /* file_build_path */
    C1(0x004b9674u, 0x00c850ecu);                              /* audio device */
    uint32_t a[6] = {sh_local(e, 0), 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xf};
    uint32_t track = e->call ? e->call(e->user, 0x004ba15cu, 6, a) : 0;   /* load track (2 stack args) */
    /* DECOMPILER TRAP: Ghidra shows dsound_init(&dev) then file_build_path(buf, grey, 5); the
     * machine code calls 0x004b94f4(0x00c850ec, 0x004e2559, 5, track) and then
     * file_build_path(buf, 0x004e2559, 5) with the same registers. */
    C4(0x004b94f4u, 0x00c850ecu, 0x004e2559u, 5, track);
    C3(0x00499408u, sh_local(e, 0), 0x004e2559u, 5);
    C2(0x004b9140u, sh_r32(e, 0x00626010u), sh_local(e, 0));     /* blit to the DirectDraw surface */
    C2(0x00486bb4u, 0x00624edcu, 1);                           /* audio_apply_volume */
    return (int32_t)track;
}

int32_t sh_display_init_once(const ShEnv* e) {
    if (sh_r32(e, 0x00624db4u) == 0) {
        C1(0x004b9674u, 0x00c850ecu);
        uint32_t r = C2(0x004b8fc8u, 0x00c850bcu, 0);         /* edx is the (zero) flag */
        C1(0x004a9c38u, r);
        C0(0x0049e44cu);
        C0(0x00495ad8u);
        C3(0x004b41a0u, 0x00c84840u, 1, 1);
        sh_w32(e, 0x00624db4u, 1);
    }
    return 0;
}

void sh_clear_624db8(const ShEnv* e) {
    for (uint32_t o = 0; o < 0x60; o += 4) sh_w32(e, 0x00624db8u + o, 0);
}

void sh_pick_opponents(const ShEnv* e) {
    if (sh_r8(e, GS + 0x21)) {
        sh_w8(e, GS + 0x0d, 0xe);
        return;
    }
    uint8_t saved = sh_r8(e, 0x009f6c1cu);
    sh_w8(e, 0x009f6c1cu, 1);
    for (uint8_t i = 0; i < sh_r8(e, GS + 2); i++) {
        int32_t r;
        do r = tm2_rand() % 13;
        while (C1(0x0047766cu, (uint8_t)r) == 0);              /* car_is_selectable */
        sh_w8(e, GS + 0x0d + i, (uint8_t)r);
    }
    sh_w8(e, 0x009f6c1cu, saved);
}

uint32_t sh_match_tournament_start(const ShEnv* e) {
    sh_pick_opponents(e);
    if ((int8_t)sh_r8(e, GS + 0x1f) > 1 && sh_r8(e, GS) == 1) return SH_SCREEN_SAVE;
    return SH_SCREEN_GLOBE;
}

/* background image (path kind 1), render setup, modal loop: its result (the modal gets the
 * 0x00495b14 result in eax) */
static uint32_t modal(const ShEnv* e, uint32_t path, uint32_t fn) {
    C3(0x00499408u, sh_local(e, 0), path, 1);
    C4(0x004a2ea0u, BG, sh_local(e, 0), 0, 0);                    /* render_set_background */
    C4(0x004a2ffcu, BG, 0, 0, 0);                              /* render_configure */
    return C1(fn, C0(0x00495b14u));
}

static uint32_t done(const ShEnv* e, uint32_t next) {
    C1(0x004a2fdcu, BG);
    return next;
}

uint32_t sh_screen_save(const ShEnv* e) {
    uint32_t r = modal(e, 0x004e2887u, 0x0046d8e4u);
    return done(e, r == 0xffffffffu ? SH_SCREEN_TITLE : SH_SCREEN_GLOBE);
}

uint32_t sh_screen_password(const ShEnv* e) {
    int32_t r = (int32_t)modal(e, 0x004e289cu, 0x0046f484u);
    uint32_t next;
    if (r > 0) {
        sh_pick_opponents(e);
        next = SH_SCREEN_GLOBE;
    } else {
        next = r == -1 ? SH_SCREEN_TITLE : SH_SCREEN_SINGLE;
    }
    return done(e, next);
}

uint32_t sh_screen_network(const ShEnv* e) {
    uint32_t r = modal(e, 0x004e28b1u, 0x00465110u);
    return done(e, r == 2 ? 0x0047a1f8u : r == 3 ? 0x0047a274u : SH_SCREEN_TITLE);
}

static uint32_t net_next(uint32_t r) {
    if (r == 0) return SH_SCREEN_NETWORK;
    if (r == 4) return 0x0047a308u;
    return SH_SCREEN_TITLE;
}

uint32_t sh_screen_network_a(const ShEnv* e) { return done(e, net_next(modal(e, 0x004e28c6u, 0x00468aa4u))); }
uint32_t sh_screen_network_b(const ShEnv* e) { return done(e, net_next(modal(e, 0x004e28dcu, 0x00467740u))); }

void sh_cmd_post_when_ready(const ShEnv* e, uint32_t cmd) {
    if (C1(0x004632f4u, 0x10) == 0) return;
    uint32_t sc = sh_r32(e, cmd + 0xc);
    uint8_t* p = wmem_ptr(&e->mem, sc, SCENE_SIZE);
    if (p) scene_post_event(p, 2, cmd);
}

void sh_cmd_button_events(const ShEnv* e, uint32_t cmd) {
    C1(0x0046ace8u, cmd);
    for (uint32_t k = 0; k < 3; k++) {
        int32_t b = (int8_t)sh_r8(e, cmd + 0x24 + k);
        if (C1(0x0044eee4u, (uint32_t)b) == 0) continue;
        uint32_t sc = sh_r32(e, cmd + 0xc);
        sh_w8(e, cmd + 0x27, k);
        uint32_t item = sh_r32(e, sc + 0xbc + 4 * sh_r32(e, sc + 0x48));
        uint8_t* p = wmem_ptr(&e->mem, sc, SCENE_SIZE);
        if (p) scene_post_event(p, (uint8_t)(0x12 + k), item);
    }
}

uint32_t sh_screen_input_cfg(const ShEnv* e, uint32_t back, uint32_t next) {
    uint32_t r = C0(0x0045f2dcu);
    if (r == 0) return back;
    if (r == 1) return next;
    return SH_SCREEN_OPTIONS;
}

uint32_t sh_screen_audio_cfg(const ShEnv* e) {
    C1(0x00486f18u, 0x00624edcu);
    return SH_SCREEN_OPTIONS;
}

void sh_render_reset(const ShEnv* e) { C4(0x004a2ffcu, BG, 0, 0, 0); }

void sh_audio_commit(const ShEnv* e, uint32_t cfg) { C2(0x00486bb4u, cfg, 0); }

int32_t sh_timer_wait(const ShEnv* e, uint32_t min_us, int32_t max_us) {
    uint32_t start = C0(0x00493038u), eax = start, d;
    for (;;) {
        do {
            C1(0x00493748u, eax);                              /* input_poll */
            eax = C0(0x00493038u);
            d = eax - start;
        } while (d < min_us);
        static const uint32_t btn[4] = {0x12, 0x11, 0x16, 0x14};
        for (int k = 0; k < 4; k++) {
            eax = C2(0x004936acu, btn[k], 0);
            if ((uint8_t)eax) return 1;
        }
        if (max_us >= 0 && d >= (uint32_t)max_us) return 0;
    }
}

/* fades: 0x0046f918 out (0x1b), 0x0046f8b0 in (0x1b, 0, 0, 99); 0x0046f94c / 0x0046f958 sync.
 * 0x0046f780 and 0x0046f828 are empty functions in the PC build (bare ret): not called here. */
static void sync2(const ShEnv* e) {
    C0(0x0046f94cu);
    C0(0x0046f958u);
}

uint32_t sh_screen_history_movie(const ShEnv* e) {
    C1(0x0046f918u, 0x1b);
    sync2(e);
    sh_play_movie(e, 0x004e2a8fu);
    C1(0x00495890u, 0);
    C4(0x0046f8b0u, 0x1b, 0, 0, 0x63);
    sync2(e);
    return 0x0048429cu;
}

/* a full screen TIM (path kind 1 into buffer buf) loaded and drawn; 0x00495b14 presents it */
static void draw_tim(const ShEnv* e, uint32_t buf, uint32_t name) {
    C3(0x00499408u, buf, name, 1);
    C1(0x0046f784u, buf);                                      /* bg_tim_load */
    C0(0x0046f7c4u);                                           /* bg_tim_draw */
}
static void show_tim(const ShEnv* e, uint32_t buf, uint32_t name) {
    draw_tim(e, buf, name);
    C0(0x00495b14u);
}

uint32_t sh_screen_credits(const ShEnv* e, uint32_t next) {
    C4(0x0046f8b0u, 0x1b, 0, 0, 0x63);
    sync2(e);
    for (uint32_t i = 1; i <= 7; i++) {
        uint32_t a[4] = {sh_local(e, 0), 0x004e2ab0u, 0x004e2aa0u, i};
        if (e->call) e->call(e->user, 0x004b27e6u, 4, a);     /* sprintf (cdecl) */
        show_tim(e, sh_local(e, 1), sh_local(e, 0));
        if (sh_timer_wait(e, 0, 10000000)) break;
    }
    return next == 0x0048429cu ? next : SH_SCREEN_TITLE;
}

uint32_t sh_screen_match_end(const ShEnv* e) {
    C1(0x00495890u, 0);
    uint8_t r = (uint8_t)C1(0x00475d34u, GS);                 /* sessionRun */
    uint8_t t = sh_r8(e, GS);
    if (t != 1 && t != 2) {
        if (t == 5 || t == 6 || t == 7) return SH_SCREEN_NETWORK;
        return SH_SCREEN_TITLE;
    }
    if (r == 1 || r == 2) {
        sh_w8(e, GS + 0x1f, 1);
        return SH_SCREEN_TITLE;
    }
    if (sh_r8(e, GS + 0x1f) == 4 && sh_r8(e, 0x00624ed0u) == 0) {
        draw_tim(e, sh_local(e, 0), 0x004e2abbu);                 /* stop screen (end of the demo) */
        sh_timer_wait(e, 5000000, 10000000);
        C0(0x00495b14u);
        return SH_SCREEN_TITLE;
    }
    int8_t ev = (int8_t)sh_r8(e, GS + 0x1f);
    if (ev < 8) {
        ev++;
        sh_w8(e, GS + 0x1f, (uint8_t)ev);
        sh_w8(e, GS + 2, (uint8_t)(sh_tournament_opponents((uint8_t)ev) - sh_r8(e, GS + 1)));
        return 0x00480c08u;
    }
    if (sh_r8(e, GS) == 1) return 0x00485290u;
    show_tim(e, sh_local(e, 1), 0x004e2acdu);                     /* co-op win */
    return SH_SCREEN_TITLE;
}

uint32_t sh_screen_ending(const ShEnv* e) {
    for (int32_t i = 0; i < (int32_t)sh_r8(e, GS + 1); i++) {
        int32_t car = (int8_t)sh_r8(e, GS + 5 + (uint32_t)i);
        sh_play_movie(e, sh_r32(e, 0x004e81d0u + (uint32_t)(car * 12)));
        if ((int8_t)sh_r8(e, GS + 5 + (uint32_t)i) != 4) sh_play_movie(e, 0x004e2ae0u);
    }
    C1(0x00495890u, 0);
    return 0x00485040u;
}

void sh_audio_init_user_volume(const ShEnv* e, uint32_t cfg) {
    sh_w32(e, cfg, C0(0x004a8f34u));
    C4(0x004ac528u, cfg + 0x112c, 0x80000002u, 0, 0x14);
    C2(0x004868ccu, cfg, 0x004e2cf8u);
    C4(0x004ac65cu, cfg + 0x112c, 0x004e2cf8u, cfg + 0x38, 0x14);
    C2(0x00486bb4u, cfg, 1);
}

void sh_round_start_music(const ShEnv* e, uint8_t level) {
    C1(0x00457c80u, level);
    int32_t track;
    if (sh_r8(e, GS) == 0) track = 10;
    else if (sh_r8(e, 0x00621181u)) track = 11;              /* 0x0046fc04 */
    else if (level == 10) track = 8;
    else if (level == 11) track = 2;
    else if (level == 12) track = 6;
    else track = (int8_t)level + 1;
    uint32_t iface = sh_r32(e, 0x00626014u);
    C2(sh_r32(e, sh_r32(e, iface + 8) + 4), iface, (uint32_t)track);   /* iface->vtable->play(track) */
    int32_t vol = (int32_t)C0(0x0046f9dcu), mn = (int32_t)sh_r32(e, 0x00c8507cu);
    sh_w32(e, 0x00c85098u, (uint32_t)(vol < mn ? mn : vol));
    int32_t v = (int32_t)(((uint32_t)vol << 16) - (uint32_t)vol) / 100;
    if (v > 0xffff) v = 0xffff;
    C2(0x004b8f48u, 0x00c850bcu, (uint32_t)v + ((uint32_t)v << 16));
}
