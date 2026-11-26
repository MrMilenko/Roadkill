/*
 * bind_match_mt2_save.c: the save / load game screens 0x0046cd70..0x0046f484
 * (src/match/mt2_save.c), called the way tools/emu/diff_mt2_save.py called them.
 *
 * Host side interfaces made real here (the harness recorded them by value):
 *   - the registry key object (a 4 byte local of the port, never touched by it): each adapter
 *     whose port opens one (save / load run, labels, write, read) pushes a 16 byte frame for it;
 *     the reg_* callbacks pass that address in eax and copy its bytes back to the host key.
 *   - text buffers the port keeps as host locals (get_text, set_text, str_compare, escape_amp,
 *     save_write, reg_read / reg_write data, value names, size, sprintf out): copied to a frame
 *     (in and, for outputs, back out). A pointer already inside game memory goes as it is.
 *   - 0x0046d17c's text argument (a game address) is given to the port as its host pointer.
 */
#include "rt_env.h"
#include "match/mt2_save.h"
#include "qol_cars.h"

static uint32_t key_addr;   /* the registry key object of the innermost adapter */

static int in_game(const void* p) { return (const uint8_t*)p >= rt_base && (const uint8_t*)p < rt_base + RT_SIZE; }

static uint32_t cw(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, const uint32_t* st, int k)
{
    uint32_t a[4 + 8] = {eax, edx, ebx, ecx};
    for (int i = 0; i < k && i < 8; i++) a[4 + i] = st[i];
    return rt_call(fn, a, 4 + k);
}
#define C0(fn) cw(fn, 0, 0, 0, 0, NULL, 0)
#define C1(fn, x) cw(fn, (uint32_t)(x), 0, 0, 0, NULL, 0)
#define C2(fn, x, y) cw(fn, (uint32_t)(x), (uint32_t)(y), 0, 0, NULL, 0)
#define C3(fn, x, y, z) cw(fn, (uint32_t)(x), (uint32_t)(y), (uint32_t)(z), 0, NULL, 0)

/* a host string (at most cap bytes) into a frame of cap bytes: its game address */
static uint32_t str_in(const char* p, uint32_t b, uint32_t cap)
{
    size_t l = strnlen(p, cap - 1);
    memcpy(rt_ptr(b), p, l);
    rt_ptr(b)[l] = 0;
    return b;
}

static void key_back(void* key) { if (key && !in_game(key)) memcpy(key, rt_ptr(key_addr), 4); }

static uint32_t main_window(void* u) { (void)u; return C0(0x004a8f34u); }
static uint32_t alloc_(void* u, uint32_t n) { (void)u; return C1(0x004a8f48u, n); }
static uint32_t button_create(void* u, uint32_t b, uint32_t h, uint32_t id, uint32_t t, const int32_t s[5])
{ (void)u; return cw(0x004a490cu, b, h, id, t, (const uint32_t*)s, 5); }
static uint32_t panel_create(void* u, uint32_t b, uint32_t h, uint32_t id, uint32_t st, const int32_t s[4])
{ (void)u; return cw(0x004a5290u, b, h, id, st, (const uint32_t*)s, 4); }
static uint32_t edit_create(void* u, uint32_t b, uint32_t h, uint32_t id, uint32_t t, const int32_t s[5])
{ (void)u; return cw(0x004a5f40u, b, h, id, t, (const uint32_t*)s, 5); }
static void set_colors(void* u, uint32_t w, uint32_t a, uint32_t b) { (void)u; C3(0x004a4de0u, w, a, b); }
static void set_keys(void* u, uint32_t w, uint32_t a, uint32_t b, uint32_t c, uint32_t s0)
{ (void)u; cw(0x004a4bdcu, w, a, b, c, &s0, 1); }
static void method(void* u, uint32_t w, uint32_t off, uint32_t arg)
{
    (void)u;
    if (!w) rt_stop("mt2_save: method call on widget 0 (ORIGINAL BUG: reads address 0x34)", off);
    C2(rt_r32(rt_r32(w + 0x34u) + off), w, arg);
}
static void list_clear(void* u, uint32_t w) { (void)u; C1(0x004a5630u, w); }
static void list_add(void* u, uint32_t w, uint32_t t) { (void)u; C2(0x004a5418u, w, t); }
static void nav_links(void* u, uint32_t w, uint32_t a, uint32_t b, uint32_t c, const uint32_t s[3])
{ (void)u; cw(0x004a70acu, w, a, b, c, s, 3); }
static void show(void* u, uint32_t w, int32_t c) { (void)u; C2(0x004a6ee4u, w, c); }
static void focus(void* u, uint32_t w) { (void)u; C1(0x004a6f80u, w); }
static void enable(void* u, uint32_t w, int32_t on) { (void)u; C2(0x004a6dd8u, w, on); }
static void get_text(void* u, uint32_t w, char* buf, int32_t n)
{
    (void)u;
    uint32_t sz = n > 0 ? (uint32_t)n : 1u;
    uint32_t b = rt_frame_push(sz);
    C3(0x004a6ef4u, w, b, n);
    memcpy(buf, rt_ptr(b), sz);
    rt_frame_pop(sz);
}
static void set_text(void* u, uint32_t w, const char* t)
{
    (void)u;
    if (in_game(t)) { C2(0x004a6f08u, w, rt_addr(t)); return; }
    uint32_t b = rt_frame_push(0xa0);
    C2(0x004a6f08u, w, str_in(t, b, 0xa0));
    rt_frame_pop(0xa0);
}
static void set_text_h(void* u, uint32_t w, uint32_t t) { (void)u; C2(0x004a6f08u, w, t); }
static uint32_t str_compare(void* u, const char* a, uint32_t t)
{
    (void)u;
    if (in_game(a)) return C2(0x004a91b0u, rt_addr(a), t);
    uint32_t b = rt_frame_push(0xa0);
    uint32_t r = C2(0x004a91b0u, str_in(a, b, 0xa0), t);
    rt_frame_pop(0xa0);
    return r;
}
static int32_t message_box(void* u, uint32_t h, uint32_t t, uint32_t c, uint32_t ty)
{
    (void)u;
    uint32_t a[4] = {h, t, c, ty};
    return (int32_t)rt_call(0x00c90524u, a, 4);   /* MessageBoxA import slot */
}
static void input_poll(void* u) { (void)u; C0(0x00493748u); }
static void input_update(void* u) { (void)u; C0(0x0049364cu); }
static void redraw(void* u, uint32_t w, uint32_t r, int32_t e) { (void)u; C3(0x004a6e2cu, w, r, e); }
static void screen_begin(void* u) { (void)u; C0(0x004a8e6cu); }
static uint32_t has_focus(void* u, uint32_t w) { (void)u; return C1(0x004a6f9cu, w); }
static uint32_t get_key(void* u) { (void)u; return C0(0x004a2068u); }
static void escape_amp(void* u, char* t, uint32_t flag)
{
    (void)u;
    if (in_game(t)) { C2(0x0046f66cu, rt_addr(t), flag); return; }
    uint32_t b = rt_frame_push(0x80);
    memcpy(rt_ptr(b), t, 0x80);
    C2(0x0046f66cu, b, flag);
    memcpy(t, rt_ptr(b), 0x80);
    rt_frame_pop(0x80);
}
static void save_write(void* u, char* t)
{
    (void)u;
    if (in_game(t)) { C1(0x0046d17cu, rt_addr(t)); return; }
    uint32_t b = rt_frame_push(0x80);
    memcpy(rt_ptr(b), t, 0x80);
    C1(0x0046d17cu, b);
    memcpy(t, rt_ptr(b), 0x80);
    rt_frame_pop(0x80);
}
static uint32_t key_state(void* u, uint32_t id, uint32_t b) { (void)u; return C2(0x004936acu, id, b); }
static void reg_open(void* u, void* key, uint32_t b) { (void)u; C2(0x004ac4b0u, key_addr, b); key_back(key); }
static void labels(void* u, int load) { (void)u; C0(load ? 0x0046cd70u : 0x0046d000u); }
static int32_t reg_subkey(void* u, void* key, uint32_t hkey, uint32_t b)
{
    (void)u;
    int32_t r = (int32_t)C3(0x004ac528u, key_addr, hkey, b);
    key_back(key);
    return r;
}
/* 0x004ac5e4 (key, name, data, &size): data 0xa0 bytes, size a u32, both in and out */
static int32_t reg_read_at(void* key, uint32_t name, void* data, uint32_t* size)
{
    uint32_t f = rt_frame_push(0xb0), d = f, sz = f + 0xa0;
    memcpy(rt_ptr(d), data, 0xa0);
    rt_w32(sz, *size);
    int32_t r = (int32_t)cw(0x004ac5e4u, key_addr, name, d, sz, NULL, 0);
    memcpy(data, rt_ptr(d), 0xa0);
    *size = rt_r32(sz);
    key_back(key);
    rt_frame_pop(0xb0);
    return r;
}
static int32_t reg_read(void* u, void* key, uint32_t name, void* data, uint32_t* size)
{ (void)u; return reg_read_at(key, name, data, size); }
static int32_t reg_read_s(void* u, void* key, const char* name, void* data, uint32_t* size)
{
    (void)u;
    uint32_t b = rt_frame_push(16);
    int32_t r = reg_read_at(key, str_in(name, b, 16), data, size);
    rt_frame_pop(16);
    return r;
}
static void reg_write(void* u, void* key, const char* name, const void* data, uint32_t size)
{
    (void)u;
    uint32_t cap = size < 0xa0u ? 0xa0u : size;
    uint32_t f = rt_frame_push(16 + cap);
    str_in(name, f, 16);
    memcpy(rt_ptr(f + 16), data, size);
    cw(0x004ac65cu, key_addr, f, f + 16, size, NULL, 0);
    key_back(key);
    rt_frame_pop(16 + cap);
}
static void reg_close(void* u, void* key) { (void)u; C1(0x004ac590u, key_addr); key_back(key); }
/* sprintf 0x004b27e6 (cdecl: out, fmt; no register arguments: a[0] = out): out is the port's 0xa0
 * byte record */
static void format_text(void* u, char* out, const char* fmt)
{
    (void)u;
    uint32_t f = rt_frame_push(0x140), o = f, fm = f + 0xa0;
    memcpy(rt_ptr(o), out, 0xa0);
    uint32_t st[2] = {o, in_game(fmt) ? rt_addr(fmt) : str_in(fmt, fm, 0xa0)};
    rt_call(0x004b27e6u, st, 2);
    memcpy(out, rt_ptr(o), 0xa0);
    rt_frame_pop(0x140);
}
/* sprintf (out, EXE format, n): out is the port's 8 byte value name ("Slot%d") */
static void format_slot(void* u, char* out, uint32_t fmt, int32_t n)
{
    (void)u;
    uint32_t o = rt_frame_push(0x40);
    uint32_t st[3] = {o, fmt, (uint32_t)n};
    rt_call(0x004b27e6u, st, 3);
    memcpy(out, rt_ptr(o), 8);
    rt_frame_pop(0x40);
}
static uint32_t level_count(void* u, uint32_t g, int32_t lv) { (void)u; return C2(0x00474e0cu, g, lv); }

static const MtSave* env(void)
{
    static MtSave s;
    static int done;
    if (!done) {
        s.g = (uint32_t*)rt_ptr(0x0058a950u);
        s.main_window = main_window;
        s.alloc = alloc_;
        s.button_create = button_create;
        s.panel_create = panel_create;
        s.edit_create = edit_create;
        s.set_colors = set_colors;
        s.set_keys = set_keys;
        s.method = method;
        s.list_clear = list_clear;
        s.list_add = list_add;
        s.nav_links = nav_links;
        s.show = show;
        s.focus = focus;
        s.enable = enable;
        s.get_text = get_text;
        s.set_text = set_text;
        s.set_text_h = set_text_h;
        s.str_compare = str_compare;
        s.message_box = message_box;
        s.input_poll = input_poll;
        s.input_update = input_update;
        s.redraw = redraw;
        s.screen_begin = screen_begin;
        s.has_focus = has_focus;
        s.get_key = get_key;
        s.escape_amp = escape_amp;
        s.save_write = save_write;
        s.key_state = key_state;
        s.reg_open = reg_open;
        s.labels = labels;
        s.reg_subkey = reg_subkey;
        s.reg_read = reg_read;
        s.reg_read_s = reg_read_s;
        s.reg_write = reg_write;
        s.reg_close = reg_close;
        s.format_text = format_text;
        s.format_slot = format_slot;
        s.level_count = level_count;
        s.human0 = rt_ptr(0x005030a8u);
        s.game = rt_ptr(0x00624ea8u);
        done = 1;
    }
    return &s;
}

/* a frame for the registry key object of one call */
static uint32_t key_push(void) { uint32_t old = key_addr; key_addr = rt_frame_push(16); return old; }
static void key_pop(uint32_t old) { rt_frame_pop(16); key_addr = old; }

static uint32_t b_labels_load(const uint32_t* a, int n) { (void)a; (void)n; uint32_t k = key_push(); mt2_save_labels(env(), 1); key_pop(k); return 0; }
static uint32_t b_labels_save(const uint32_t* a, int n) { (void)a; (void)n; uint32_t k = key_push(); mt2_save_labels(env(), 0); key_pop(k); return 0; }
static uint32_t b_write(const uint32_t* a, int n)
{
    uint32_t t = rt_arg(a, n, 0);
    uint32_t k = key_push();
    mt2_save_record_write(env(), t ? (char*)rt_ptr(t) : NULL);
    key_pop(k);
    return 0;
}
/* + [gameplay] play_dark_tooth's fallback for a Dark Tooth save (qol_cars.h) */
static uint32_t b_load(const uint32_t* a, int n) { (void)a; (void)n; uint32_t k = key_push(); mt2_save_record_load(env()); key_pop(k); qolc_save_loaded(); return 0; }
static uint32_t b_widgets_save(const uint32_t* a, int n) { (void)a; (void)n; mt2_save_widgets(env(), 0); return 0; }
static uint32_t b_widgets_load(const uint32_t* a, int n) { (void)a; (void)n; mt2_save_widgets(env(), 1); return 0; }
static uint32_t b_destroy_save(const uint32_t* a, int n) { (void)a; (void)n; mt2_save_destroy(env(), 0); return 0; }
static uint32_t b_destroy_load(const uint32_t* a, int n) { (void)a; (void)n; mt2_save_destroy(env(), 1); return 0; }
static uint32_t b_run_save(const uint32_t* a, int n) { (void)a; (void)n; uint32_t k = key_push(); uint32_t r = mt2_save_run(env()); key_pop(k); return r; }
static uint32_t b_run_load(const uint32_t* a, int n) { (void)a; (void)n; uint32_t k = key_push(); uint32_t r = mt2_load_run(env()); key_pop(k); return r; }
#define SLOT(nm, k) static uint32_t nm(const uint32_t* a, int n) { (void)a; (void)n; mt2_save_slot(env(), k); return 0; }
SLOT(b_slot1, 1)
SLOT(b_slot2, 2)
SLOT(b_slot3, 3)
SLOT(b_slot4, 4)
SLOT(b_slot5, 5)
SLOT(b_slot6, 6)

void rt_bind_match_mt2_save(void)
{
    rt_bind(0x0046cd70u, b_labels_load, "load_screen_labels");
    rt_bind(0x0046d000u, b_labels_save, "save_screen_labels");
    rt_bind(0x0046d17cu, b_write, "save_game_write");
    rt_bind(0x0046d290u, b_widgets_save, "save_screen_widgets");
    rt_bind(0x0046d834u, b_destroy_save, "save_screen_destroy");
    rt_bind(0x0046d8e4u, b_run_save, "save_screen_run");
    rt_bind(0x0046da2cu, b_slot1, "save_slot1_click");
    rt_bind(0x0046dd2cu, b_slot2, "save_slot2_click");
    rt_bind(0x0046e034u, b_slot3, "save_slot3_click");
    rt_bind(0x0046e33cu, b_slot4, "save_slot4_click");
    rt_bind(0x0046e644u, b_slot5, "save_slot5_click");
    rt_bind(0x0046e94cu, b_slot6, "save_slot6_click");
    rt_bind(0x0046ecb0u, b_load, "load_game_read");
    rt_bind(0x0046ee30u, b_widgets_load, "load_screen_widgets");
    rt_bind(0x0046f3d4u, b_destroy_load, "load_screen_destroy");
    rt_bind(0x0046f484u, b_run_load, "load_screen_run");
}

RT_BIND_AREA(match_mt2_save)
