/*
 * bind_match_inputcfg.c: the input configuration screen 0x0045f2dc..0x00461dc0
 * (src/match/mt_inputcfg.c), called the way tools/emu/diff_match_inputcfg.py called them.
 *
 * Host side interfaces made real here (the harness only recorded them):
 *   set_rect(host rect): SetRect through its import slot on a 16 byte frame (the original's local)
 *   status_set / str_compare (host strings): a string already in game memory goes by its address,
 *     a port local is copied to a frame first; status_get(host out): read into a 0x100 byte frame,
 *     copied out (the port's buffer is 0x100 bytes).
 */
#include "rt_env.h"
#include "match/mt_inputcfg.h"

static uint32_t callv(uint32_t fn, const uint32_t* a, int n) { return rt_call(fn, a, n); }
#define C1(fn, x) do { uint32_t _a[1] = {(uint32_t)(x)}; callv(fn, _a, 1); } while (0)
#define C2(fn, x, y) do { uint32_t _a[2] = {(uint32_t)(x), (uint32_t)(y)}; callv(fn, _a, 2); } while (0)
static uint32_t r1(uint32_t fn, uint32_t x) { uint32_t a[1] = {x}; return rt_call(fn, a, 1); }
static uint32_t r2(uint32_t fn, uint32_t x, uint32_t y) { uint32_t a[2] = {x, y}; return rt_call(fn, a, 2); }

static int in_game(const void* p) { return (const uint8_t*)p >= rt_base && (const uint8_t*)p < rt_base + RT_SIZE; }

/* ---- the shell context (0x0045f080 / 0x0045f1e8 run inside inputcfg_main) ---- */
static uint8_t* resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }
static uint32_t s_save_data(void* u, uint32_t a) { (void)u; return r1(0x004a81e8u, a); }
static uint32_t s_unlocked(void* u, int32_t w) { (void)u; return r1(0x0044eee4u, (uint32_t)w); }
static void s_net_set_type(void* u, uint32_t net, int32_t t) { (void)u; C2(0x004aaa74u, net, t); }
static void s_button_select(void* u, uint32_t fn, uint32_t obj, int32_t on) { (void)u; C2(fn, obj, on); }

/* ---- callbacks ---- */
static void widget_call(void* u, uint32_t fn, uint32_t w, int32_t arg) { (void)u; C2(fn, w, arg); }
static void list_free(void* u, uint32_t l) { (void)u; C1(0x004a2fdcu, l); }
static void set_rect(void* u, const int32_t r[4])
{
    (void)u;
    uint32_t rc = rt_frame_push(16);
    uint32_t a[5] = {rc, (uint32_t)r[0], (uint32_t)r[1], (uint32_t)r[2], (uint32_t)r[3]};
    rt_call(0x00c90574u, a, 5);   /* SetRect import slot */
    rt_frame_pop(16);
}
static void invalidate(void* u, uint32_t h, uint32_t r, int32_t e)
{
    (void)u;
    uint32_t a[3] = {h, r, (uint32_t)e};
    rt_call(0x00c904f8u, a, 3);   /* InvalidateRect import slot */
}
static void screen_begin(void* u) { (void)u; rt_call(0x004a8e6cu, NULL, 0); }
static void create_widgets(void* u) { (void)u; rt_call(0x0045f3b4u, NULL, 0); }
static uint32_t num_players(void* u) { (void)u; return rt_call(0x004a2470u, NULL, 0); }
static void input_poll(void* u) { (void)u; rt_call(0x00493748u, NULL, 0); }
static uint32_t key_state(void* u, int32_t id, int32_t b) { (void)u; return r2(0x004936acu, (uint32_t)id, (uint32_t)b); }
static uint32_t device_mode(void* u, int32_t p) { (void)u; return r1(0x004932a0u, (uint32_t)p); }
static void widget_enable(void* u, uint32_t w, int32_t on) { (void)u; C2(0x004a6dd8u, w, on); }
static uint32_t widget_get(void* u, uint32_t fn, uint32_t w) { (void)u; return r1(fn, w); }
static void busy_click(void* u, uint32_t w) { (void)u; C1(0x004a6f80u, w); }
static void set_device(void* u, int32_t p, int32_t on) { (void)u; C2(0x004932c4u, p, on); }
static void key_set_a(void* u, uint32_t k, uint32_t c) { (void)u; C2(0x004a1f6cu, k, c); }
static void key_set_b(void* u, uint32_t k, uint32_t c) { (void)u; C2(0x004a274cu, k, c); }
static void keys_save(void* u) { (void)u; rt_call(0x004a9c38u, NULL, 0); }
static void keys_default_a(void* u) { (void)u; rt_call(0x004a2018u, NULL, 0); }
static uint32_t key_get_a(void* u, uint32_t k) { (void)u; return r1(0x004a1f8cu, k); }
static void keys_default_b(void* u) { (void)u; rt_call(0x004a27d4u, NULL, 0); }
static uint32_t key_get_b(void* u, uint32_t k) { (void)u; return r1(0x004a2758u, k); }
static void text_set(void* u, uint32_t w, uint32_t t) { (void)u; C2(0x004a6f08u, w, t); }
static void render_mode(void* u, int32_t a, int32_t b) { (void)u; C2(0x0046086cu, a, b); }
static uint32_t main_window(void* u) { (void)u; return rt_call(0x004a8f34u, NULL, 0); }
static void message_box(void* u, uint32_t h, uint32_t t, uint32_t c, uint32_t ty)
{
    (void)u;
    uint32_t a[4] = {h, t, c, ty};
    rt_call(0x00c90524u, a, 4);   /* MessageBoxA import slot */
}
static void input_update(void* u) { (void)u; rt_call(0x0049364cu, NULL, 0); }
static void widget_color(void* u, uint32_t fn, uint32_t w, int32_t rgb) { (void)u; C2(fn, w, rgb); }
static void widget_redraw(void* u, uint32_t w, int32_t a, int32_t b)
{
    (void)u;
    uint32_t x[3] = {w, (uint32_t)a, (uint32_t)b};
    rt_call(0x004a6e2cu, x, 3);
}
static void dlg_setup(void* u, int32_t p, int32_t i) { (void)u; C2(0x0045fc3cu, p, i); }
static void dlg_step_left(void* u, int32_t i) { (void)u; C1(0x004605b0u, i); }
static void dlg_step_right(void* u, int32_t i) { (void)u; C1(0x00460b04u, i); }
static void list_redraw(void* u, uint32_t l, int32_t a, int32_t b, int32_t c2)
{
    (void)u;
    uint32_t x[4] = {l, (uint32_t)a, (uint32_t)b, (uint32_t)c2};
    rt_call(0x004a2ffcu, x, 4);
}
static void refresh_right(void* u, int32_t a, int32_t b) { (void)u; C2(0x00460ea0u, a, b); }
static void status_clear(void* u, uint32_t w) { (void)u; C1(0x004a5630u, w); }
static void status_set(void* u, uint32_t w, const char* t)
{
    (void)u;
    if (in_game(t)) { C2(0x004a5418u, w, rt_addr(t)); return; }
    uint32_t b = rt_frame_push(0x100);
    strncpy((char*)rt_ptr(b), t, 0xff);
    C2(0x004a5418u, w, b);
    rt_frame_pop(0x100);
}
static void status_get(void* u, uint32_t w, int32_t a, char* out)
{
    (void)u;
    uint32_t b = rt_frame_push(0x100);
    uint32_t x[3] = {w, (uint32_t)a, b};
    rt_call(0x004a559cu, x, 3);
    memcpy(out, rt_ptr(b), 0x100);
    out[0xff] = 0;
    rt_frame_pop(0x100);
}
static int32_t str_compare(void* u, const char* s1, const char* s2)
{
    (void)u;
    uint32_t b = rt_frame_push(0x200), p1 = b, p2 = b + 0x100;
    if (in_game(s1)) p1 = rt_addr(s1); else strncpy((char*)rt_ptr(p1), s1, 0xff);
    if (in_game(s2)) p2 = rt_addr(s2); else strncpy((char*)rt_ptr(p2), s2, 0xff);
    int32_t r = (int32_t)r2(0x004a91b0u, p1, p2);
    rt_frame_pop(0x200);
    return r;
}

static const MtInputCfg* env(void)
{
    static MtShell s;
    static MtInputCfg c;
    static int done;
    if (!done) {
        s.state_table = rt_ptr(0x0050e5ccu);
        s.cars = rt_ptr(0x0050e6d4u);
        s.arena = rt_ptr(0x00624ec7u);
        s.arena_cur = (const int32_t*)rt_ptr(0x0050fa70u);
        s.game_type = rt_ptr(0x00624ea8u);
        s.buttons = (const uint32_t*)rt_ptr(0x0050fa7cu);
        s.resolve = resolve;
        s.save_data = s_save_data;
        s.unlocked = s_unlocked;
        s.net_set_type = s_net_set_type;
        s.button_select = s_button_select;

        c.widgets = (const uint32_t*)rt_ptr(0x0050e100u);
        c.exit_flag = (int32_t*)rt_ptr(0x0050e100u);
        c.result = (int32_t*)rt_ptr(0x0050e104u);
        c.slots = rt_ptr(0x0050e6ccu);
        c.shell = &s;
        c.resolve = resolve;
        c.widget_call = widget_call;
        c.list_free = list_free;
        c.set_rect = set_rect;
        c.invalidate = invalidate;
        c.screen_begin = screen_begin;
        c.create_widgets = create_widgets;
        c.num_players = num_players;
        c.input_poll = input_poll;
        c.key_state = key_state;
        c.device_mode = device_mode;
        c.widget_enable = widget_enable;
        c.busy = (const int32_t*)rt_ptr(0x0050e5b8u);
        c.keytab = rt_ptr(0x004e7a48u);
        c.names_left = (const uint32_t*)rt_ptr(0x004e755cu);
        c.names_right = (const uint32_t*)rt_ptr(0x004e797cu);
        c.widget_get = widget_get;
        c.busy_click = busy_click;
        c.set_device = set_device;
        c.key_set_a = key_set_a;
        c.key_set_b = key_set_b;
        c.keys_save = keys_save;
        c.keys_default_a = keys_default_a;
        c.key_get_a = key_get_a;
        c.keys_default_b = keys_default_b;
        c.key_get_b = key_get_b;
        c.text_set = text_set;
        c.render_mode = render_mode;
        c.main_window = main_window;
        c.message_box = message_box;
        c.input_update = input_update;
        c.dlg = (int32_t*)rt_ptr(0x0050e5b8u);
        c.widget_color = widget_color;
        c.widget_redraw = widget_redraw;
        c.dlg_setup = dlg_setup;
        c.dlg_step_left = dlg_step_left;
        c.dlg_step_right = dlg_step_right;
        c.list_redraw = list_redraw;
        c.refresh_right = refresh_right;
        c.status_clear = status_clear;
        c.status_set = status_set;
        c.status_get = status_get;
        c.str_compare = str_compare;
        done = 1;
    }
    return &c;
}

#define R(i) rt_arg(a, n, i)
#define V0(nm, call) static uint32_t nm(const uint32_t* a, int n) { (void)a; (void)n; call; return 0; }
V0(b_destroy_widgets, mt_inputcfg_destroy_widgets(env()))
V0(b_destroy_car_panel, mt_inputcfg_destroy_car_panel(env()))
V0(b_device_left, mt_inputcfg_device_left(env()))
V0(b_device_right, mt_inputcfg_device_right(env()))
V0(b_accept, mt_inputcfg_accept(env()))
V0(b_defaults_left, mt_inputcfg_defaults_left(env()))
V0(b_defaults_right, mt_inputcfg_defaults_right(env()))
static uint32_t b_main(const uint32_t* a, int n) { (void)a; (void)n; return (uint32_t)mt_inputcfg_main(env()); }
static uint32_t b_device_button(const uint32_t* a, int n) { mt_inputcfg_device_button(env(), R(0)); return 0; }
static uint32_t b_device_toggle_right(const uint32_t* a, int n) { mt_inputcfg_device_toggle_right(env(), R(0)); return 0; }
static uint32_t b_rebind_left(const uint32_t* a, int n) { mt_inputcfg_rebind_left(env(), R(0)); return 0; }
static uint32_t b_rebind_right(const uint32_t* a, int n) { mt_inputcfg_rebind_right(env(), R(0)); return 0; }
static uint32_t b_mark_left(const uint32_t* a, int n) { mt_inputcfg_mark_left(env(), (int32_t)R(0), (int32_t)R(1)); return 0; }
static uint32_t b_mark_right(const uint32_t* a, int n) { mt_inputcfg_mark_right(env(), (int32_t)R(0), (int32_t)R(1)); return 0; }

void rt_bind_match_inputcfg(void)
{
    rt_bind(0x0045f2dcu, b_main, "inputcfg_main");
    rt_bind(0x004602b0u, b_destroy_widgets, "inputcfg_destroy_widgets");
    rt_bind(0x00460370u, b_destroy_car_panel, "inputcfg_destroy_car_panel");
    rt_bind(0x00460438u, b_device_left, "inputcfg_device_left");
    rt_bind(0x004604f4u, b_device_right, "inputcfg_device_right");
    rt_bind(0x0046086cu, b_mark_left, "inputcfg_mark_left");
    rt_bind(0x00460ea0u, b_mark_right, "inputcfg_mark_right");
    rt_bind(0x00461924u, b_accept, "inputcfg_accept");
    rt_bind(0x004619e4u, b_defaults_left, "inputcfg_defaults_left");
    rt_bind(0x00461a68u, b_defaults_right, "inputcfg_defaults_right");
    rt_bind(0x00461ae0u, b_device_button, "inputcfg_device_button");
    rt_bind(0x00461b48u, b_device_toggle_right, "inputcfg_device_toggle_right");
    rt_bind(0x00461c10u, b_rebind_left, "inputcfg_rebind_left");
    rt_bind(0x00461dc0u, b_rebind_right, "inputcfg_rebind_right");
}

RT_BIND_AREA(match_inputcfg)
