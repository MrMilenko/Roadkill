/*
 * bind_match_pause.c: the in game pause menu, its keys, drawing, banner, detail levels and the
 * display settings 0x0046b354..0x0046cc68 (src/match/mt_pause.c), called the way
 * tools/emu/diff_match_pause*.py and diff_match_settings.py called them. None takes arguments.
 *
 * MtPauseFn callbacks: (eax, edx, ebx, stack words). The draw and banner callbacks pass their
 * fourth register (ecx) as stack[0] (mt_pause.h): those go out with stack[0] in ecx.
 *
 * Emulator-only interface replaced: the settings load / save keep the registry helper's key handle
 * (and the size / mode words after it) in host locals and pass host pointers to them. The
 * original has them on its stack, and the registry helpers (game code) write through them, so the
 * adapter gives the call a game frame and the callbacks map the host locals onto it: copied in
 * before each helper call, back out after (the first helper call, reg_init, names the base).
 */
#include "rt_env.h"
#include "match/mt_pause.h"

static uint32_t pcall(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, const uint32_t* st, int k)
{
    uint32_t a[4 + 4] = {eax, edx, ebx, ecx};
    if (k > 4) k = 4;
    for (int i = 0; i < k; i++) a[4 + i] = st[i];
    return rt_call(fn, a, 4 + k);
}
/* stack words as stack words */
#define PF(nm, addr) \
    static uint32_t nm(void* u, uint32_t eax, uint32_t edx, uint32_t ebx, const uint32_t* st, int k) \
    { (void)u; return pcall(addr, eax, edx, ebx, 0, st, k); }
/* stack[0] is ecx */
#define PFC(nm, addr) \
    static uint32_t nm(void* u, uint32_t eax, uint32_t edx, uint32_t ebx, const uint32_t* st, int k) \
    { (void)u; return pcall(addr, eax, edx, ebx, k > 0 ? st[0] : 0, NULL, 0); }

PF(p_is_singleplayer, 0x0045c348u)
PF(p_view_grow, 0x0049433cu)
PF(p_screen_width, 0x004958d8u)
PF(p_view_set, 0x0049437cu)
PF(p_mode_count, 0x004b454cu)
PF(p_mode_get, 0x004b4554u)
PF(p_mode_set, 0x004b4558u)
PF(p_display_ok, 0x004958acu)
PF(p_set_step, 0x00470320u)
PF(p_fx_48e97c, 0x0048e97cu)
PF(p_fx_4ad21c, 0x004ad21cu)
PF(p_fx_48e918, 0x0048e918u)
/* 0x0048e920 takes no register arguments, its stack word goes in a[0] */
static uint32_t p_fx_48e920(void* u, uint32_t eax, uint32_t edx, uint32_t ebx, const uint32_t* st, int k)
{ (void)u; (void)eax; (void)edx; (void)ebx; return rt_call(0x0048e920u, st, k > 4 ? 4 : k); }
PF(p_schedule_event, 0x0046ca68u)
PF(p_key_state, 0x004936acu)
PF(p_key_pressed, 0x0049eb5cu)
PF(p_input_update, 0x0049364cu)
PF(p_menu_open, 0x0046be2cu)
PF(p_is_multiplayer, 0x004632b0u)
PF(p_text_height, 0x004937e0u)
PF(p_screen_height, 0x004958e4u)
PFC(p_text_begin, 0x00452d28u)
PFC(p_text_draw, 0x00451f28u)
PF(p_text_end, 0x00452d48u)
PF(p_font_scale, 0x004b3bacu)
PFC(p_font_color, 0x004b3b84u)
PFC(p_font_draw, 0x004b31d0u)
PF(p_font_width, 0x004b3a44u)

#define I32P(a) ((int32_t*)rt_ptr(a))

static const MtPause* menu(void)
{
    static MtPause p;
    static int done;
    if (!done) {
        p.input = rt_ptr(0x00510a78u);
        p.open = I32P(0x00510dc4u);
        p.released = I32P(0x00510dc8u);
        p.may_close = I32P(0x00510dccu);
        p.consumed = I32P(0x00510dd0u);
        p.page = I32P(0x00510dacu);
        p.cursor = I32P(0x00510db0u);
        p.cursor_min = I32P(0x00510db4u);
        p.cursor_max = I32P(0x00510db8u);
        p.back_item = I32P(0x00510da8u);
        p.detail = I32P(0x00510a84u);
        p.v_c84f70 = I32P(0x00c84f70u);
        p.arena = rt_ptr(0x00624ec7u);
        p.is_singleplayer = p_is_singleplayer;
        p.view_grow = p_view_grow;
        p.screen_width = p_screen_width;
        p.view_set = p_view_set;
        p.mode_count = p_mode_count;
        p.mode_get = p_mode_get;
        p.mode_set = p_mode_set;
        p.display_ok = p_display_ok;
        p.set_step = p_set_step;
        p.fx_48e97c = p_fx_48e97c;
        p.fx_4ad21c = p_fx_4ad21c;
        p.fx_48e918 = p_fx_48e918;
        p.fx_48e920 = p_fx_48e920;
        p.schedule_event = p_schedule_event;
        p.user = NULL;
        done = 1;
    }
    return &p;
}

static uint8_t* p_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

/* ---- settings: the registry helpers get game addresses ---- */

static uint8_t* key_host;     /* the port's key locals (set by reg_init) ... */
static uint32_t key_game;     /* ... and the frame standing for them */
static uint32_t key_words;    /* 3 (load) / 1 (save) */

static uint32_t to_game(uintptr_t v)
{
    if (!rt_is_host_ptr(v)) return (uint32_t)v;
    const uint8_t* p = (const uint8_t*)v;
    if (key_host && p >= key_host && p < key_host + 4 * key_words) return key_game + (uint32_t)(p - key_host);
    if (p >= rt_base && p < rt_base + RT_SIZE) return rt_addr(p);
    rt_stop("pause settings: a host pointer the game cannot reach", 0);
    return 0;
}
static uint32_t reg(uint32_t fn, int init, uintptr_t a0, uintptr_t b, uintptr_t c, uintptr_t d)
{
    if (init) key_host = (uint8_t*)a0;
    if (key_host) memcpy(rt_ptr(key_game), key_host, 4 * key_words);
    uint32_t a[4] = {to_game(a0), to_game(b), to_game(c), to_game(d)};
    uint32_t r = rt_call(fn, a, 4);
    if (key_host) memcpy(key_host, rt_ptr(key_game), 4 * key_words);
    return r;
}
#define RF(nm, addr, init) \
    static uint32_t nm(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) { (void)u; return reg(addr, init, a, b, c, d); }
RF(r_init, 0x004ac4b0u, 1)
RF(r_open, 0x004ac528u, 0)
RF(r_read_bin, 0x004ac5e4u, 0)
RF(r_read_dword, 0x004ac5bcu, 0)
RF(r_write_bin, 0x004ac65cu, 0)
RF(r_write_dword, 0x004ac614u, 0)
RF(r_close, 0x004ac590u, 0)
RF(r_view_get, 0x00494360u, 0)

static const MtSettings* settings(void)
{
    static MtSettings s;
    if (!s.menu)
        s = (MtSettings){menu(), I32P(0x00510a80u), (const uint32_t*)rt_ptr(0x00c84840u), r_init, r_open, r_read_bin,
                         r_read_dword, r_write_bin, r_write_dword, r_close, r_view_get};
    return &s;
}

static uint32_t settings_call(int save)
{
    uint8_t* sh = key_host;
    uint32_t sg = key_game, sw = key_words;
    key_host = NULL;
    key_words = save ? 1 : 3;
    key_game = rt_frame_push(16);
    int32_t r = save ? mt_settings_save(settings()) : mt_settings_load(settings());
    rt_frame_pop(16);
    key_host = sh; key_game = sg; key_words = sw;
    return (uint32_t)r;
}

/* ---- init / draw / banner ---- */

/* wsprintfA (import slot 0x00c90658, cdecl: dest, format, ...) */
static uint32_t i_wsprintf(void* u, uint32_t dest, uint32_t fmt, uint32_t a0, uint32_t b)
{
    (void)u;
    uint32_t a[4] = {dest, fmt, a0, b};
    return rt_call(0x00c90658u, a, 4);
}
/* 0x004b3a90 text height (eax font, edx text) */
static uint32_t i_text_height(void* u, uint32_t font, uint32_t text)
{
    (void)u;
    uint32_t a[4] = {font, text, 0, 0};
    return rt_call(0x004b3a90u, a, 4);
}

static const MtPauseDraw* draw_env(void)
{
    static MtPauseDraw d;
    if (!d.resolve)
        d = (MtPauseDraw){I32P(0x00510dc4u), I32P(0x00510dacu), I32P(0x00510db0u), I32P(0x00510db4u), I32P(0x00510db8u),
                          I32P(0x00510da8u), I32P(0x00510dbcu), I32P(0x00510dc0u), I32P(0x00510e68u),
                          (const uint32_t*)rt_ptr(0x004e7c24u), p_resolve, p_font_scale, p_font_color, p_font_draw,
                          p_font_width, p_screen_width, NULL};
    return &d;
}

static uint32_t b_pause_menu(const uint32_t* a, int n) { (void)a; (void)n; mt_pause_menu(menu()); return 0; }
static uint32_t b_pause_keys(const uint32_t* a, int n)
{
    (void)a; (void)n;
    MtPauseKeys k = {menu(), rt_ptr(0x00510a78u), p_key_state, p_key_pressed, p_input_update};
    return (uint32_t)mt_pause_keys(&k);
}
static uint32_t b_detail_high(const uint32_t* a, int n) { (void)a; (void)n; return (uint32_t)mt_pause_detail_high(menu()); }
static uint32_t b_detail_apply(const uint32_t* a, int n) { (void)a; (void)n; mt_detail_apply(menu()); return 0; }
static uint32_t b_settings_load(const uint32_t* a, int n) { (void)a; (void)n; return settings_call(0); }
static uint32_t b_settings_save(const uint32_t* a, int n) { (void)a; (void)n; return settings_call(1); }
static uint32_t b_pause_init(const uint32_t* a, int n)
{
    (void)a; (void)n;
    MtPauseInit s = {menu(), I32P(0x00510da8u), I32P(0x00510dbcu), I32P(0x00510dc0u), I32P(0x00510e68u),
                     (const uint32_t*)rt_ptr(0x00c848e4u), I32P(0x00510db4u), I32P(0x00510db8u), i_wsprintf, i_text_height};
    mt_pause_init(&s);
    return 0;
}
static uint32_t b_pause_draw(const uint32_t* a, int n) { (void)a; (void)n; mt_pause_draw(draw_env()); return 0; }
/* 0x0046b428 (eax table, edx dy) */
static uint32_t b_pause_draw_items(const uint32_t* a, int n) { mt_pause_draw_items(draw_env(), rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1)); return 0; }
static uint32_t b_pause_draw_modes(const uint32_t* a, int n) { (void)a; (void)n; mt_pause_draw_modes(draw_env()); return 0; }
static uint32_t b_pause_banner(const uint32_t* a, int n)
{
    (void)a; (void)n;
    MtPauseBanner b = {(uint16_t*)rt_ptr(0x00510e6cu), (int16_t*)rt_ptr(0x004e7c64u), p_is_singleplayer, p_menu_open,
                       p_is_multiplayer, p_text_height, p_screen_height, p_screen_width, p_text_begin, p_text_draw,
                       p_text_end, NULL};
    mt_pause_banner(&b);
    return 0;
}

void rt_bind_match_pause(void)
{
    rt_bind(0x0046b354u, b_pause_init, "pause_menu_init");
    rt_bind(0x0046b428u, b_pause_draw_items, "pause_menu_draw_items");
    rt_bind(0x0046b4d4u, b_pause_draw_modes, "pause_menu_draw_modes");
    rt_bind(0x0046b604u, b_pause_menu, "pause_menu");
    rt_bind(0x0046bbecu, b_pause_draw, "pause_menu_draw");
    rt_bind(0x0046be34u, b_pause_keys, "pause_keys");
    rt_bind(0x0046c17cu, b_settings_load, "settings_load");
    rt_bind(0x0046c384u, b_settings_save, "settings_save");
    rt_bind(0x0046c3f4u, b_detail_apply, "detail_apply");
    rt_bind(0x0046c5fcu, b_detail_high, "detail_set_high");
    rt_bind(0x0046cc68u, b_pause_banner, "pause_banner");
}

RT_BIND_AREA(match_pause)
