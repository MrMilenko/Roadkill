/*
 * win32_user.c: the USER32 imports (windows, the message loop, input state, resources the
 * windows use) on SDL2, port code.
 *
 * Windows: host records behind HWND values 0x00030000 + 0x10 i. A top level window (no parent,
 * not WS_CHILD) is an SDL window of the asked client size (AdjustWindowRect adds no frame), made
 * hidden and shown by ShowWindow; child windows and dialogs have no SDL side. CreateWindowExA
 * sends WM_NCCREATE / WM_CREATE (CREATESTRUCT in the frame stack) to the class procedure, and
 * DestroyWindow WM_DESTROY, synchronously as Windows does; the first ShowWindow of a top level
 * window sends WM_ACTIVATEAPP / WM_ACTIVATE / WM_SETFOCUS. A window procedure that is an import
 * slot (DefWindowProcA, the game's classes register its slot value) and a game procedure both
 * get the four stdcall words at a[0..3] (compact arguments, docs/RUNTIME.md).
 *
 * Messages: one queue, each message tagged with its thread. PeekMessageA on the main thread
 * first turns SDL events into messages: keys -> WM_KEYDOWN / WM_KEYUP (WM_SYSKEY* with Alt)
 * with Windows virtual keys and scan codes in lParam, the mouse -> WM_MOUSEMOVE / WM_*BUTTON*,
 * focus -> WM_ACTIVATEAPP + WM_ACTIVATE, expose -> WM_PAINT, close -> WM_CLOSE.
 * TranslateMessage posts WM_CHAR for keys that make a character.
 *
 * Dialogs, the system control classes (Button, ComboBox, msctls_trackbar32, ...) and painting
 * on their DCs are src/host/dialogs: the system classes get its window procedures, mouse
 * messages go to the control under the cursor (w32dlg_mouse_target), PeekMessageA repaints.
 *
 * STUBS: menus (no menu bar: LoadMenuA a fake handle, Get/SetMenuItemInfoA fail / succeed doing
 * nothing); accelerators (never translate); MessageBoxA prints to stderr and answers the first
 * button (Retry / Cancel boxes: Cancel; TM2_MSGBOX=<id> overrides); ClipCursor, FlashWindow,
 * MessageBeep do nothing.
 */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "win32_host.h"
#include "host/settings/settings.h"
#include "host/dialogs/dialogs.h"

#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ---- classes and windows ---- */

typedef struct { char name[64]; uint32_t atom, proc, style, extra; } Cls;
static Cls classes[32];
static int nclasses;

typedef struct { uint32_t atom; char str[64]; uint32_t val; } Prop;
typedef struct Win {
    int alive;
    uint32_t hwnd, proc, style, exstyle, id, userdata, parent, menu, inst;
    int x, y, w, h, visible, shown_once;
    SDL_Window* sdl;
    Prop props[16];
    int nprops;
    uint32_t extra[8];
} Win;
#define NWIN 128
#define HWND_BASE 0x00030000u
static Win wins[NWIN];
static uint32_t focus, active, main_hwnd;
static int sdl_ok = -1;

static Win* win(uint32_t h)
{
    if (h < HWND_BASE || (h - HWND_BASE) % 0x10u) return NULL;
    uint32_t i = (h - HWND_BASE) / 0x10u;
    return i < NWIN && wins[i].alive ? &wins[i] : NULL;
}

struct SDL_Window* win32_sdl_window(void) { Win* w = win(main_hwnd); return w ? w->sdl : NULL; }
uint32_t win32_main_hwnd(void) { return main_hwnd; }

static int sdl_init(void)
{
    if (sdl_ok < 0) {
        sdl_ok = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0;
        if (!sdl_ok) fprintf(stderr, "tm2: SDL video: %s (running without a window)\n", SDL_GetError());
    }
    return sdl_ok;
}

uint32_t w32_call_wndproc(uint32_t proc, uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp)
{
    if (!proc) return 0;
    uint32_t v[4] = {hwnd, msg, wp, lp};   /* compact: no register arguments, the stack words */
    return rt_call(proc, v, 4);
}
static uint32_t send(uint32_t h, uint32_t msg, uint32_t wp, uint32_t lp)
{
    Win* w = win(h);
    return w ? w32_call_wndproc(w->proc, h, msg, wp, lp) : 0;
}

/* ---- the message queue ---- */

typedef struct { uint32_t tid, hwnd, msg, wp, lp, time; int32_t x, y; } Msg;
#define NQ 1024
static Msg q[NQ];
static int nq;
static int32_t mouse_x, mouse_y;   /* screen coordinates */
static uint32_t last_pos;

static void post(uint32_t tid, uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp)
{
    if (nq == NQ) { memmove(q, q + 1, sizeof(Msg) * (NQ - 1)); nq--; }
    q[nq++] = (Msg){tid, hwnd, msg, wp, lp, w32_ms(), mouse_x, mouse_y};
}

/* Windows virtual key of an SDL scan code (0: none) */
static uint32_t vk_of(SDL_Scancode s)
{
    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) return 'A' + (uint32_t)(s - SDL_SCANCODE_A);
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_9) return '1' + (uint32_t)(s - SDL_SCANCODE_1);
    if (s == SDL_SCANCODE_0) return '0';
    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F12) return 0x70 + (uint32_t)(s - SDL_SCANCODE_F1);
    if (s >= SDL_SCANCODE_KP_1 && s <= SDL_SCANCODE_KP_9) return 0x61 + (uint32_t)(s - SDL_SCANCODE_KP_1);
    switch (s) {
    case SDL_SCANCODE_KP_0: return 0x60;
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return 0x0d;
    case SDL_SCANCODE_ESCAPE: return 0x1b;
    case SDL_SCANCODE_BACKSPACE: return 0x08;
    case SDL_SCANCODE_TAB: return 0x09;
    case SDL_SCANCODE_SPACE: return 0x20;
    case SDL_SCANCODE_LEFT: return 0x25;
    case SDL_SCANCODE_UP: return 0x26;
    case SDL_SCANCODE_RIGHT: return 0x27;
    case SDL_SCANCODE_DOWN: return 0x28;
    case SDL_SCANCODE_LSHIFT: case SDL_SCANCODE_RSHIFT: return 0x10;
    case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_RCTRL: return 0x11;
    case SDL_SCANCODE_LALT: case SDL_SCANCODE_RALT: return 0x12;
    case SDL_SCANCODE_PAUSE: return 0x13;
    case SDL_SCANCODE_CAPSLOCK: return 0x14;
    case SDL_SCANCODE_PAGEUP: return 0x21;
    case SDL_SCANCODE_PAGEDOWN: return 0x22;
    case SDL_SCANCODE_END: return 0x23;
    case SDL_SCANCODE_HOME: return 0x24;
    case SDL_SCANCODE_INSERT: return 0x2d;
    case SDL_SCANCODE_DELETE: return 0x2e;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x6a;
    case SDL_SCANCODE_KP_PLUS: return 0x6b;
    case SDL_SCANCODE_KP_MINUS: return 0x6d;
    case SDL_SCANCODE_KP_PERIOD: return 0x6e;
    case SDL_SCANCODE_KP_DIVIDE: return 0x6f;
    case SDL_SCANCODE_NUMLOCKCLEAR: return 0x90;
    case SDL_SCANCODE_SCROLLLOCK: return 0x91;
    case SDL_SCANCODE_SEMICOLON: return 0xba;
    case SDL_SCANCODE_EQUALS: return 0xbb;
    case SDL_SCANCODE_COMMA: return 0xbc;
    case SDL_SCANCODE_MINUS: return 0xbd;
    case SDL_SCANCODE_PERIOD: return 0xbe;
    case SDL_SCANCODE_SLASH: return 0xbf;
    case SDL_SCANCODE_GRAVE: return 0xc0;
    case SDL_SCANCODE_LEFTBRACKET: return 0xdb;
    case SDL_SCANCODE_BACKSLASH: return 0xdc;
    case SDL_SCANCODE_RIGHTBRACKET: return 0xdd;
    case SDL_SCANCODE_APOSTROPHE: return 0xde;
    default: return 0;
    }
}
/* the PC set 1 scan code Windows puts in lParam bits 16..23 (24: extended) */
static uint32_t pc_scan(uint32_t vk, SDL_Scancode s)
{
    static const uint8_t letters[26] = {0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
                                        0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c};
    if (vk >= 'A' && vk <= 'Z') return letters[vk - 'A'];
    if (vk >= '1' && vk <= '9') return 2 + vk - '1';
    if (vk == '0') return 0x0b;
    if (vk >= 0x70 && vk <= 0x79) return 0x3b + vk - 0x70;
    switch (vk) {
    case 0x1b: return 0x01;
    case 0x08: return 0x0e;
    case 0x09: return 0x0f;
    case 0x0d: return s == SDL_SCANCODE_KP_ENTER ? 0x11c : 0x1c;
    case 0x20: return 0x39;
    case 0x10: return s == SDL_SCANCODE_RSHIFT ? 0x36 : 0x2a;
    case 0x11: return s == SDL_SCANCODE_RCTRL ? 0x11d : 0x1d;
    case 0x12: return s == SDL_SCANCODE_RALT ? 0x138 : 0x38;
    case 0x25: return 0x14b;
    case 0x26: return 0x148;
    case 0x27: return 0x14d;
    case 0x28: return 0x150;
    case 0x24: return 0x147;
    case 0x23: return 0x14f;
    case 0x21: return 0x149;
    case 0x22: return 0x151;
    case 0x2d: return 0x152;
    case 0x2e: return 0x153;
    case 0x7a: return 0x57;
    case 0x7b: return 0x58;
    default: return 0;
    }
}

static int buttons;   /* SDL mouse button mask */
static uint32_t mk_flags(void)
{
    const uint8_t* k = host_keyboard_state(NULL);
    uint32_t f = 0;
    if (buttons & SDL_BUTTON_LMASK) f |= 1;
    if (buttons & SDL_BUTTON_RMASK) f |= 2;
    if (k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT]) f |= 4;
    if (k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL]) f |= 8;
    if (buttons & SDL_BUTTON_MMASK) f |= 0x10;
    return f;
}

static uint32_t hwnd_of_sdl(uint32_t id)
{
    for (int i = 0; i < NWIN; i++) if (wins[i].alive && wins[i].sdl && SDL_GetWindowID(wins[i].sdl) == id) return wins[i].hwnd;
    return main_hwnd;
}

static void pump(void)
{
    if (sdl_ok != 1) return;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            if (main_hwnd) post(1, main_hwnd, 0x10, 0, 0);   /* WM_CLOSE */
            break;
        case SDL_KEYDOWN: case SDL_KEYUP: {
            uint32_t vk = vk_of(ev.key.keysym.scancode);
            if (!vk) break;
            int up = ev.type == SDL_KEYUP;
            uint32_t sc = pc_scan(vk, ev.key.keysym.scancode);
            uint32_t lp = 1u | (sc & 0xff) << 16 | (sc & 0x100 ? 1u << 24 : 0);
            int alt = (ev.key.keysym.mod & KMOD_ALT) != 0;
            if (alt) lp |= 1u << 29;
            if (!up && ev.key.repeat) lp |= 1u << 30;
            if (up) lp |= 3u << 30;
            uint32_t m = up ? 0x101 : 0x100;
            if (alt || vk == 0x12 || vk == 0x79) m += 4;   /* WM_SYSKEYDOWN / UP: Alt, F10 */
            post(1, focus ? focus : hwnd_of_sdl(ev.key.windowID), m, vk, lp);
            break;
        }
        case SDL_MOUSEMOTION: {
            uint32_t h = hwnd_of_sdl(ev.motion.windowID);
            Win* w = win(h);
            mouse_x = ev.motion.x + (w ? w->x : 0);
            mouse_y = ev.motion.y + (w ? w->y : 0);
            int32_t mx = ev.motion.x, my = ev.motion.y;
            if (!(h = w32dlg_mouse_target(h, &mx, &my, 0))) break;   /* the control under the cursor */
            post(1, h, 0x200, mk_flags(), (uint32_t)(mx & 0xffff) | (uint32_t)my << 16);
            break;
        }
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: {
            int down = ev.type == SDL_MOUSEBUTTONDOWN;
            int b = ev.button.button;
            if (down) buttons |= SDL_BUTTON(b); else buttons &= ~SDL_BUTTON(b);
            uint32_t m = b == SDL_BUTTON_LEFT ? 0x201 : b == SDL_BUTTON_RIGHT ? 0x204 : b == SDL_BUTTON_MIDDLE ? 0x207 : 0;
            if (!m) break;
            if (!down) m += 1;
            else if (ev.button.clicks == 2) m += 2;   /* WM_*BUTTONDBLCLK */
            int32_t mx = ev.button.x, my = ev.button.y;
            uint32_t h = w32dlg_mouse_target(hwnd_of_sdl(ev.button.windowID), &mx, &my, down);
            if (h) post(1, h, m, mk_flags(), (uint32_t)(mx & 0xffff) | (uint32_t)my << 16);
            break;
        }
        case SDL_WINDOWEVENT: {
            uint32_t h = hwnd_of_sdl(ev.window.windowID);
            switch (ev.window.event) {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                post(1, h, 0x1c, 1, 0);   /* WM_ACTIVATEAPP */
                post(1, h, 0x06, 1, 0);   /* WM_ACTIVATE */
                active = focus = h;
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                /* the original pauses while deactivated; TM2_KEEP_ACTIVE=1 (automated runs, a
                 * window started from a terminal that keeps the focus) never deactivates */
                if (settings_bool("debug.keep_active", 0)) break;
                post(1, h, 0x06, 0, 0);
                post(1, h, 0x1c, 0, 0);
                break;
            case SDL_WINDOWEVENT_EXPOSED: post(1, h, 0x0f, 0, 0); break;   /* WM_PAINT */
            case SDL_WINDOWEVENT_MOVED: {
                Win* w = win(h);
                if (w) { w->x = ev.window.data1; w->y = ev.window.data2; }
                break;
            }
            }
            break;
        }
        }
    }
}

/* PeekMessageA(&msg, hwnd, min, max, remove) */
static uint32_t u_PeekMessageA(const uint32_t* a, int n)
{
    uint32_t tid = w32_thread_id();
    if (tid == 1) { w32_yield(); win32_keys_tick(); pump(); w32dlg_tick(); }
    uint32_t h = W(1), lo = W(2), hi = W(3);
    for (int i = 0; i < nq; i++) {
        Msg* m = &q[i];
        if (m->tid != tid) continue;
        if (h && m->hwnd != h && m->msg != 0x12) continue;
        if ((lo || hi) && (m->msg < lo || m->msg > hi)) continue;
        uint32_t p = W(0);
        rt_w32(p, m->hwnd);
        rt_w32(p + 4, m->msg);
        rt_w32(p + 8, m->wp);
        rt_w32(p + 12, m->lp);
        rt_w32(p + 16, m->time);
        rt_w32(p + 20, (uint32_t)m->x);
        rt_w32(p + 24, (uint32_t)m->y);
        last_pos = (uint32_t)(m->x & 0xffff) | (uint32_t)m->y << 16;
        if (W(4) & 1) { memmove(q + i, q + i + 1, sizeof(Msg) * (size_t)(nq - i - 1)); nq--; }
        return 1;
    }
    return 0;
}
static uint32_t u_WaitMessage(const uint32_t* a, int n)
{
    (void)a; (void)n;
    if (w32_thread_id() != 1 || sdl_ok != 1) { w32_block_begin(); SDL_Delay(5); w32_block_end(); return 1; }
    for (int k = 0; k < 20 && nq == 0; k++) {
        if (SDL_WaitEventTimeout(NULL, 5)) break;
    }
    pump();
    return 1;
}
static uint32_t u_DispatchMessageA(const uint32_t* a, int n)
{
    uint32_t p = W(0);
    uint32_t h = rt_r32(p), m = rt_r32(p + 4);
    if (!h) return 0;
    return send(h, m, rt_r32(p + 8), rt_r32(p + 12));
}
static uint32_t u_TranslateMessage(const uint32_t* a, int n)
{
    uint32_t p = W(0);
    uint32_t m = rt_r32(p + 4), vk = rt_r32(p + 8);
    if (m != 0x100 && m != 0x104) return 0;
    const uint8_t* k = host_keyboard_state(NULL);
    int shift = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT];
    uint32_t c = 0;
    if (vk >= 'A' && vk <= 'Z') c = shift ? vk : vk + 32;
    else if (vk >= '0' && vk <= '9' && !shift) c = vk;
    else if (vk == 0x20 || vk == 0x0d || vk == 0x08 || vk == 0x1b || vk == 0x09) c = vk;
    else if (vk >= 0x60 && vk <= 0x69) c = '0' + vk - 0x60;
    else {
        static const struct { uint32_t vk; char lo, hi; } o[] = {{0xba, ';', ':'}, {0xbb, '=', '+'}, {0xbc, ',', '<'},
            {0xbd, '-', '_'}, {0xbe, '.', '>'}, {0xbf, '/', '?'}, {0xc0, '`', '~'}, {0xdb, '[', '{'},
            {0xdc, '\\', '|'}, {0xdd, ']', '}'}, {0xde, '\'', '"'}};
        for (size_t i = 0; i < sizeof o / sizeof o[0]; i++) if (o[i].vk == vk) c = (uint32_t)(shift ? o[i].hi : o[i].lo);
        if (shift && vk >= '0' && vk <= '9') c = (uint32_t)")!@#$%^&*("[vk - '0'];
    }
    if (!c) return 0;
    /* in front of what is queued, as Windows delivers WM_CHAR right after its key down */
    if (nq == NQ) nq--;
    memmove(q + 1, q, sizeof(Msg) * (size_t)nq);
    nq++;
    q[0] = (Msg){w32_thread_id(), rt_r32(p), m == 0x104 ? 0x106u : 0x102u, c, rt_r32(p + 12), w32_ms(), mouse_x, mouse_y};
    return 1;
}
static uint32_t u_TranslateAcceleratorA(const uint32_t* a, int n) { (void)a; (void)n; return 0; }
static uint32_t u_PostQuitMessage(const uint32_t* a, int n) { post(w32_thread_id(), 0, 0x12, W(0), 0); return 0; }
static uint32_t u_PostThreadMessageA(const uint32_t* a, int n) { post(W(0), 0, W(1), W(2), W(3)); return 1; }
static uint32_t u_SendMessageA(const uint32_t* a, int n) { return send(W(0), W(1), W(2), W(3)); }
static uint32_t u_CallWindowProcA(const uint32_t* a, int n) { return w32_call_wndproc(W(0), W(1), W(2), W(3), W(4)); }
static uint32_t u_GetMessagePos(const uint32_t* a, int n) { (void)a; (void)n; return last_pos; }

/* DefWindowProcA(hwnd, msg, wp, lp) */
static uint32_t u_DestroyWindow(const uint32_t* a, int n);
static uint32_t u_DefWindowProcA(const uint32_t* a, int n)
{
    switch (W(1)) {
    case 0x81: return 1;                         /* WM_NCCREATE */
    case 0x10: { uint32_t v[1] = {W(0)}; u_DestroyWindow(v, 1); return 0; }   /* WM_CLOSE */
    case 0x0f: return 0;
    case 0x14: return 1;                         /* WM_ERASEBKGND */
    case 0x20: return 0;                         /* WM_SETCURSOR */
    default: return 0;
    }
}

/* ---- classes and windows ---- */

static Cls* cls_find(uint32_t name)
{
    for (int i = 0; i < nclasses; i++) {
        if (name < 0x10000u ? classes[i].atom == name : !strcasecmp(classes[i].name, w32_str(name))) return &classes[i];
    }
    return NULL;
}
/* RegisterClassA(&WNDCLASSA) */
static uint32_t u_RegisterClassA(const uint32_t* a, int n)
{
    uint32_t wc = W(0);
    if (nclasses == 32) return 0;
    Cls* c = &classes[nclasses];
    uint32_t name = rt_r32(wc + 0x24);
    snprintf(c->name, sizeof c->name, "%s", name >= 0x10000u ? w32_str(name) : "#");
    c->style = rt_r32(wc);
    c->proc = rt_r32(wc + 4);
    c->extra = rt_r32(wc + 0x0c);
    c->atom = 0xc000u + (uint32_t)nclasses;
    nclasses++;
    return c->atom;
}

/* CreateWindowExA(ex, class, title, style, x, y, w, h, parent, menu, inst, param) */
static uint32_t u_CreateWindowExA(const uint32_t* a, int n)
{
    Cls* c = cls_find(W(1));
    const char* cname = W(1) >= 0x10000u ? w32_str(W(1)) : "";
    int i;
    for (i = 1; i < NWIN && wins[i].alive; i++) {}
    if (i == NWIN) return 0;
    Win* w = &wins[i];
    memset(w, 0, sizeof *w);
    w->alive = 1;
    w->hwnd = HWND_BASE + 0x10u * (uint32_t)i;
    /* the system classes (BUTTON, EDIT, STATIC, LISTBOX, ...): no behaviour, DefWindowProcA */
    w->proc = c ? c->proc : w32dlg_class_proc(cname);   /* the system classes: src/host/dialogs */
    if (!w->proc) w->proc = rt_import_slot("USER32.dll!DefWindowProcA");
    if (!c && !w32dlg_class_proc(cname)) fprintf(stderr, "tm2: CreateWindowExA class %s: a plain window\n", cname);
    w->exstyle = W(0);
    w->style = W(3);
    w->x = W(4) == 0x80000000u ? 0 : (int32_t)W(4);
    w->y = W(5) == 0x80000000u ? 0 : (int32_t)W(5);
    w->w = W(6) == 0x80000000u ? 640 : (int32_t)W(6);
    w->h = W(7) == 0x80000000u ? 480 : (int32_t)W(7);
    w->parent = W(8);
    w->menu = W(9);
    w->id = (w->style & 0x40000000u) ? W(9) : 0;
    w->inst = W(10);
    int top = !w->parent && !(w->style & 0x40000000u);
    if (top && sdl_init()) {
        w->sdl = SDL_CreateWindow(W(2) ? w32_str(W(2)) : "", w->x > 0 ? w->x : SDL_WINDOWPOS_CENTERED,
                                  w->y > 0 ? w->y : SDL_WINDOWPOS_CENTERED, w->w > 0 ? w->w : 640,
                                  w->h > 0 ? w->h : 480, SDL_WINDOW_HIDDEN);
        if (!w->sdl) fprintf(stderr, "tm2: SDL_CreateWindow: %s\n", SDL_GetError());
        if (!main_hwnd) main_hwnd = w->hwnd;
    }
    /* CREATESTRUCTA {params, inst, menu, parent, cy, cx, y, x, style, name, class, exstyle} */
    uint32_t cs = rt_frame_push(0x30);
    uint32_t v[12] = {W(11), W(10), W(9), W(8), (uint32_t)w->h, (uint32_t)w->w, (uint32_t)w->y, (uint32_t)w->x,
                      W(3), W(2), W(1), W(0)};
    for (int k = 0; k < 12; k++) rt_w32(cs + 4u * (uint32_t)k, v[k]);
    uint32_t h = w->hwnd;
    send(h, 0x81, 0, cs);                    /* WM_NCCREATE (its answer is not checked) */
    int32_t r = (int32_t)send(h, 0x01, 0, cs);   /* WM_CREATE */
    rt_frame_pop(0x30);
    if (r == -1) {
        if (w->sdl) SDL_DestroyWindow(w->sdl);
        if (main_hwnd == h) main_hwnd = 0;
        w->alive = 0;
        return 0;
    }
    if (w->style & 0x10000000u) { uint32_t s[2] = {h, 5}; (void)s; w->visible = 1; if (w->sdl) SDL_ShowWindow(w->sdl); }
    return h;
}

static uint32_t u_DestroyWindow(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    if (!w) return 0;
    uint32_t h = w->hwnd;
    for (int i = 0; i < NWIN; i++) if (wins[i].alive && wins[i].parent == h) { uint32_t v[1] = {wins[i].hwnd}; u_DestroyWindow(v, 1); }
    send(h, 0x02, 0, 0);   /* WM_DESTROY */
    send(h, 0x82, 0, 0);   /* WM_NCDESTROY */
    w = win(h);
    if (!w) return 1;
    if (w->sdl) SDL_DestroyWindow(w->sdl);
    w->alive = 0;
    if (focus == h) focus = 0;
    if (active == h) active = 0;
    if (main_hwnd == h) main_hwnd = 0;
    return 1;
}

static void activate(uint32_t h)
{
    uint32_t old = focus;
    active = focus = h;
    send(h, 0x1c, 1, 0);      /* WM_ACTIVATEAPP */
    send(h, 0x86, 1, 0);      /* WM_NCACTIVATE */
    send(h, 0x06, 1, 0);      /* WM_ACTIVATE (WA_ACTIVE) */
    send(h, 0x07, old, 0);    /* WM_SETFOCUS */
}

/* ShowWindow(hwnd, cmd): the previous visibility */
static uint32_t u_ShowWindow(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    if (!w) return 0;
    uint32_t was = (uint32_t)w->visible, h = w->hwnd;
    int show = W(1) != 0 && W(1) != 6 && W(1) != 2 && W(1) != 7;   /* SW_HIDE, SW_MINIMIZE, ... */
    w->visible = show;
    if (w->sdl) { if (show) SDL_ShowWindow(w->sdl); else SDL_HideWindow(w->sdl); }
    if (show) {
        send(h, 0x18, 1, 0);   /* WM_SHOWWINDOW */
        w = win(h);
        if (w && !w->shown_once && !w->parent) {
            w->shown_once = 1;
            activate(h);
            send(h, 0x05, 0, (uint32_t)(w->w & 0xffff) | (uint32_t)w->h << 16);   /* WM_SIZE */
            send(h, 0x03, 0, (uint32_t)(w->x & 0xffff) | (uint32_t)w->y << 16);   /* WM_MOVE */
        }
    }
    return was;
}
static uint32_t u_UpdateWindow(const uint32_t* a, int n) { return win(W(0)) != NULL; }
static uint32_t u_InvalidateRect(const uint32_t* a, int n) { w32dlg_invalidate(W(0)); return 1; }

static uint32_t u_GetWindowLongA(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    if (!w) return 0;
    switch ((int32_t)W(1)) {
    case -4: return w->proc;
    case -6: return w->inst;
    case -8: return w->parent;
    case -12: return w->id;
    case -16: return w->style | (w->visible ? 0x10000000u : 0);
    case -20: return w->exstyle;
    case -21: return w->userdata;
    default: return (int32_t)W(1) >= 0 && W(1) < 32 ? w->extra[W(1) / 4] : 0;
    }
}
static uint32_t u_SetWindowLongA(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    if (!w) return 0;
    uint32_t v = W(2), old = u_GetWindowLongA(a, n);
    switch ((int32_t)W(1)) {
    case -4: w->proc = v; break;
    case -6: w->inst = v; break;
    case -8: w->parent = v; break;
    case -12: w->id = v; break;
    case -16: w->style = v & ~0x10000000u; w->visible = (v & 0x10000000u) != 0; break;
    case -20: w->exstyle = v; break;
    case -21: w->userdata = v; break;
    default: if ((int32_t)W(1) >= 0 && W(1) < 32) w->extra[W(1) / 4] = v; break;
    }
    return old;
}
static uint32_t u_SetClassLongA(const uint32_t* a, int n) { (void)a; (void)n; return 0; }

static void put_rect(uint32_t r, int l, int t, int rr, int b)
{
    rt_w32(r, (uint32_t)l);
    rt_w32(r + 4, (uint32_t)t);
    rt_w32(r + 8, (uint32_t)rr);
    rt_w32(r + 12, (uint32_t)b);
}
static uint32_t u_GetClientRect(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    if (!w) return 0;
    put_rect(W(1), 0, 0, w->w, w->h);
    return 1;
}
static uint32_t u_GetWindowRect(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    if (!w) return 0;
    put_rect(W(1), w->x, w->y, w->x + w->w, w->y + w->h);
    return 1;
}
/* the screen origin of a window's client area (parents' positions add up) */
static void origin(Win* w, int* x, int* y)
{
    *x = *y = 0;
    for (int k = 0; w && k < 16; k++, w = win(w->parent)) { *x += w->x; *y += w->y; }
}
static uint32_t u_ClientToScreen(const uint32_t* a, int n)
{
    int x, y;
    origin(win(W(0)), &x, &y);
    rt_w32(W(1), rt_r32(W(1)) + (uint32_t)x);
    rt_w32(W(1) + 4, rt_r32(W(1) + 4) + (uint32_t)y);
    return 1;
}
static uint32_t u_ScreenToClient(const uint32_t* a, int n)
{
    int x, y;
    origin(win(W(0)), &x, &y);
    rt_w32(W(1), rt_r32(W(1)) - (uint32_t)x);
    rt_w32(W(1) + 4, rt_r32(W(1) + 4) - (uint32_t)y);
    return 1;
}
static void move(Win* w, int x, int y, int cx, int cy, int sz, int mv)
{
    uint32_t h = w->hwnd;
    if (mv) { w->x = x; w->y = y; }
    if (sz) { w->w = cx; w->h = cy; }
    if (w->sdl) {
        if (sz && cx > 0 && cy > 0) SDL_SetWindowSize(w->sdl, cx, cy);
        if (mv) SDL_SetWindowPosition(w->sdl, x, y);
    }
    if (sz) send(h, 0x05, 0, (uint32_t)(cx & 0xffff) | (uint32_t)cy << 16);
    w = win(h);
    if (w && mv) send(h, 0x03, 0, (uint32_t)(w->x & 0xffff) | (uint32_t)w->y << 16);
}
/* MoveWindow(hwnd, x, y, w, h, repaint) */
static uint32_t u_MoveWindow(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    if (!w) return 0;
    move(w, (int32_t)W(1), (int32_t)W(2), (int32_t)W(3), (int32_t)W(4), 1, 1);
    return 1;
}
/* SetWindowPos(hwnd, after, x, y, cx, cy, flags) */
static uint32_t u_SetWindowPos(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    if (!w) return 0;
    uint32_t f = W(6);
    move(w, (int32_t)W(2), (int32_t)W(3), (int32_t)W(4), (int32_t)W(5), !(f & 1), !(f & 2));
    w = win(W(0));
    if (w && (f & 0x40)) { w->visible = 1; if (w->sdl) SDL_ShowWindow(w->sdl); }
    if (w && (f & 0x80)) { w->visible = 0; if (w->sdl) SDL_HideWindow(w->sdl); }
    return 1;
}
static uint32_t u_AdjustWindowRect(const uint32_t* a, int n) { (void)a; (void)n; return 1; }   /* no frame */
static uint32_t u_GetParent(const uint32_t* a, int n) { Win* w = win(W(0)); return w ? w->parent : 0; }
static uint32_t u_GetFocus(const uint32_t* a, int n) { (void)a; (void)n; return focus; }
static uint32_t u_SetFocus(const uint32_t* a, int n)
{
    uint32_t old = focus;
    if (W(0) && win(W(0)) && W(0) != focus) {
        if (old) send(old, 0x08, W(0), 0);   /* WM_KILLFOCUS */
        focus = W(0);
        send(focus, 0x07, old, 0);           /* WM_SETFOCUS */
    }
    return old;
}
static uint32_t u_GetActiveWindow(const uint32_t* a, int n) { (void)a; (void)n; return active; }
static uint32_t u_SetActiveWindow(const uint32_t* a, int n) { uint32_t o = active; if (win(W(0))) active = W(0); return o; }
static uint32_t u_SetForegroundWindow(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    if (w && w->sdl) SDL_RaiseWindow(w->sdl);
    return w != NULL;
}
static uint32_t u_EnableWindow(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    if (!w) return 0;
    uint32_t was = (w->style & 0x08000000u) != 0;
    if (W(1)) w->style &= ~0x08000000u; else w->style |= 0x08000000u;
    return was;
}
static uint32_t u_IsIconic(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    return w && w->sdl && (SDL_GetWindowFlags(w->sdl) & SDL_WINDOW_MINIMIZED) ? 1u : 0u;
}
static uint32_t u_GetDlgItem(const uint32_t* a, int n)
{
    for (int i = 0; i < NWIN; i++) if (wins[i].alive && wins[i].parent == W(0) && wins[i].id == W(1)) return wins[i].hwnd;
    return 0;
}

/* properties: the name is an atom (< 0x10000) or a string */
static Prop* prop(Win* w, uint32_t name, int make)
{
    const char* s = name >= 0x10000u ? w32_str(name) : NULL;
    for (int i = 0; i < w->nprops; i++) {
        Prop* p = &w->props[i];
        if (s ? !p->atom && !strcasecmp(p->str, s) : p->atom == name) return p;
    }
    if (!make || w->nprops == 16) return NULL;
    Prop* p = &w->props[w->nprops++];
    p->atom = s ? 0 : name;
    snprintf(p->str, sizeof p->str, "%s", s ? s : "");
    p->val = 0;
    return p;
}
static uint32_t u_SetPropA(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    Prop* p = w ? prop(w, W(1), 1) : NULL;
    if (!p) return 0;
    p->val = W(2);
    return 1;
}
static uint32_t u_GetPropA(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    Prop* p = w ? prop(w, W(1), 0) : NULL;
    return p ? p->val : 0;
}
static uint32_t u_RemovePropA(const uint32_t* a, int n)
{
    Win* w = win(W(0));
    Prop* p = w ? prop(w, W(1), 0) : NULL;
    if (!p) return 0;
    uint32_t v = p->val;
    *p = w->props[--w->nprops];
    return v;
}

static uint32_t wm_next = 0xc100;
static struct { char name[64]; uint32_t id; } wmsgs[32];
static int nwmsgs;
static uint32_t u_RegisterWindowMessageA(const uint32_t* a, int n)
{
    const char* s = w32_str(W(0));
    for (int i = 0; i < nwmsgs; i++) if (!strcasecmp(wmsgs[i].name, s)) return wmsgs[i].id;
    if (nwmsgs == 32) return 0;
    snprintf(wmsgs[nwmsgs].name, sizeof wmsgs[0].name, "%s", s);
    wmsgs[nwmsgs].id = wm_next++;
    return wmsgs[nwmsgs++].id;
}

/* ---- input state ---- */

static uint32_t u_GetAsyncKeyState(const uint32_t* a, int n)
{
    uint32_t vk = W(0) & 0xff;
    if (sdl_ok == 1) SDL_PumpEvents();
    uint32_t mb = sdl_ok == 1 ? SDL_GetMouseState(NULL, NULL) : 0;
    if (vk == 1) return mb & SDL_BUTTON_LMASK ? 0x8000u : 0;
    if (vk == 2) return mb & SDL_BUTTON_RMASK ? 0x8000u : 0;
    if (vk == 4) return mb & SDL_BUTTON_MMASK ? 0x8000u : 0;
    if (sdl_ok != 1) return 0;
    int nk;
    const uint8_t* k = host_keyboard_state(&nk);
    static const SDL_Scancode side[6] = {SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_LCTRL,
                                         SDL_SCANCODE_RCTRL, SDL_SCANCODE_LALT, SDL_SCANCODE_RALT};
    if (vk >= 0xa0 && vk <= 0xa5) return k[side[vk - 0xa0]] ? 0x8000u : 0;   /* VK_LSHIFT .. VK_RMENU */
    for (int s = 0; s < nk; s++) if (k[s] && vk_of((SDL_Scancode)s) == vk) return 0x8000u;
    return 0;
}
static uint32_t u_GetCursorPos(const uint32_t* a, int n)
{
    rt_w32(W(0), (uint32_t)mouse_x);
    rt_w32(W(0) + 4, (uint32_t)mouse_y);
    return 1;
}
static uint32_t u_SetCursorPos(const uint32_t* a, int n)
{
    Win* w = win(main_hwnd);
    mouse_x = (int32_t)W(0);
    mouse_y = (int32_t)W(1);
    if (w && w->sdl && (SDL_GetWindowFlags(w->sdl) & SDL_WINDOW_INPUT_FOCUS)) SDL_WarpMouseInWindow(w->sdl, mouse_x - w->x, mouse_y - w->y);
    return 1;
}
static int cursor_count;
static uint32_t u_ShowCursor(const uint32_t* a, int n)
{
    cursor_count += W(0) ? 1 : -1;
    if (sdl_ok == 1) SDL_ShowCursor(cursor_count >= 0 ? SDL_ENABLE : SDL_DISABLE);
    return (uint32_t)cursor_count;
}
static uint32_t u_ClipCursor(const uint32_t* a, int n) { (void)a; (void)n; return 1; }

/* ---- resources and the rest ---- */

static uint32_t fake_handle(void) { return w32_handle_new(H_MISC, NULL); }
static uint32_t u_LoadIconA(const uint32_t* a, int n) { (void)a; (void)n; return fake_handle(); }
static uint32_t u_LoadCursorA(const uint32_t* a, int n) { (void)a; (void)n; return fake_handle(); }
static uint32_t u_LoadBitmapA(const uint32_t* a, int n) { uint32_t b = w32dlg_LoadBitmapA(a, n); return b ? b : fake_handle(); }
static uint32_t u_LoadImageA(const uint32_t* a, int n) { (void)a; (void)n; return fake_handle(); }
static uint32_t u_LoadMenuA(const uint32_t* a, int n) { (void)a; (void)n; return fake_handle(); }
static uint32_t u_LoadAcceleratorsA(const uint32_t* a, int n) { (void)a; (void)n; return fake_handle(); }
static uint32_t u_CreateAcceleratorTableA(const uint32_t* a, int n) { (void)a; (void)n; return fake_handle(); }
static uint32_t u_DestroyAcceleratorTable(const uint32_t* a, int n) { w32_handle_free(W(0)); return 1; }
static uint32_t u_GetMenu(const uint32_t* a, int n) { Win* w = win(W(0)); return w ? w->menu : 0; }
static uint32_t u_SetMenu(const uint32_t* a, int n) { Win* w = win(W(0)); if (w) w->menu = W(1); return w != NULL; }
static uint32_t u_DrawMenuBar(const uint32_t* a, int n) { (void)a; (void)n; return 1; }
static uint32_t u_GetMenuItemInfoA(const uint32_t* a, int n) { (void)a; (void)n; return 0; }
static uint32_t u_SetMenuItemInfoA(const uint32_t* a, int n) { (void)a; (void)n; return 1; }

/* LoadStringA(inst, id, buf, max): the string table (RT_STRING 6, block id / 16 + 1) */
static uint32_t u_LoadStringA(const uint32_t* a, int n)
{
    uint32_t id = W(1), buf = W(2), max = W(3);
    if (!buf || !max) return 0;
    rt_w8(buf, 0);
    uint32_t de = w32_find_resource(6, id / 16 + 1);
    if (!de) return 0;
    uint32_t p = 0x00400000u + rt_r32(de);
    for (uint32_t k = 0; k < id % 16; k++) p += 2 + 2u * rt_r16(p);
    uint32_t len = rt_r16(p), o = 0;
    for (; o < len && o + 1 < max; o++) {
        uint16_t c = rt_r16(p + 2 + 2 * o);
        rt_w8(buf + o, c < 0x100 ? (uint8_t)c : '?');
    }
    rt_w8(buf + o, 0);
    return o;
}

/* MessageBoxA(hwnd, text, caption, type) */
static uint32_t u_MessageBoxA(const uint32_t* a, int n)
{
    fprintf(stderr, "tm2: MessageBox [%s] %s\n", W(2) ? w32_str(W(2)) : "", W(1) ? w32_str(W(1)) : "");
    const char* o = settings_str("debug.msgbox", NULL);
    if (o && *o) return (uint32_t)atoi(o);
    switch (W(3) & 0xf) {
    case 0: return 1;   /* MB_OK: IDOK */
    case 1: return 1;   /* MB_OKCANCEL: IDOK */
    case 2: return 3;   /* MB_ABORTRETRYIGNORE: IDABORT */
    case 3: return 6;   /* MB_YESNOCANCEL: IDYES */
    case 4: return 6;   /* MB_YESNO: IDYES */
    case 5: return 2;   /* MB_RETRYCANCEL: IDCANCEL */
    default: return 1;
    }
}
static uint32_t u_MessageBeep(const uint32_t* a, int n) { (void)a; (void)n; return 1; }
static uint32_t u_FlashWindow(const uint32_t* a, int n) { (void)a; (void)n; return 0; }

/* dialogs: src/host/dialogs */
static uint32_t u_DialogBoxParamA(const uint32_t* a, int n) { return w32dlg_DialogBoxParamA(a, n); }
static uint32_t u_CreateDialogParamA(const uint32_t* a, int n) { return w32dlg_CreateDialogParamA(a, n); }
static uint32_t u_EndDialog(const uint32_t* a, int n) { return w32dlg_EndDialog(a, n); }
static uint32_t u_IsDialogMessageA(const uint32_t* a, int n) { return w32dlg_IsDialogMessageA(a, n); }

/* rectangles */
static uint32_t u_SetRect(const uint32_t* a, int n) { put_rect(W(0), (int32_t)W(1), (int32_t)W(2), (int32_t)W(3), (int32_t)W(4)); return 1; }
static uint32_t u_OffsetRect(const uint32_t* a, int n)
{
    uint32_t r = W(0);
    put_rect(r, (int32_t)(rt_r32(r) + W(1)), (int32_t)(rt_r32(r + 4) + W(2)), (int32_t)(rt_r32(r + 8) + W(1)), (int32_t)(rt_r32(r + 12) + W(2)));
    return 1;
}
static uint32_t u_IntersectRect(const uint32_t* a, int n)
{
    int32_t l = (int32_t)rt_r32(W(1)), t = (int32_t)rt_r32(W(1) + 4), r = (int32_t)rt_r32(W(1) + 8), b = (int32_t)rt_r32(W(1) + 12);
    int32_t l2 = (int32_t)rt_r32(W(2)), t2 = (int32_t)rt_r32(W(2) + 4), r2 = (int32_t)rt_r32(W(2) + 8), b2 = (int32_t)rt_r32(W(2) + 12);
    if (l2 > l) l = l2;
    if (t2 > t) t = t2;
    if (r2 < r) r = r2;
    if (b2 < b) b = b2;
    if (l >= r || t >= b) { put_rect(W(0), 0, 0, 0, 0); return 0; }
    put_rect(W(0), l, t, r, b);
    return 1;
}

/* GetSystemMetrics: the screen is the desktop SDL reports (640 x 480 without one) */
static uint32_t u_GetSystemMetrics(const uint32_t* a, int n)
{
    SDL_Rect r = {0, 0, 640, 480};
    if (sdl_init()) SDL_GetDisplayBounds(0, &r);
    switch (W(0)) {
    case 0: case 16: case 78: return (uint32_t)r.w;   /* SM_CXSCREEN, SM_CXFULLSCREEN, SM_CXVIRTUALSCREEN */
    case 1: case 17: case 79: return (uint32_t)r.h;
    case 2: case 3: case 20: case 21: return 16;      /* scroll bar sizes */
    case 11: case 12: return 32;                      /* SM_CXICON */
    case 13: case 14: return 32;                      /* SM_CXCURSOR */
    case 19: return 1;                                /* SM_MOUSEPRESENT */
    case 43: return 3;                                /* SM_CMOUSEBUTTONS */
    default: return 0;                                /* frames, captions, menus: none */
    }
}
static uint32_t u_GetSysColor(const uint32_t* a, int n) { return W(0) == 5 ? 0xffffffu : W(0) == 8 ? 0 : 0xc0c0c0u; }

/* painting on a window: DCs on the controls' pictures (src/host/dialogs); the game's own window
 * gets a DC that only measures (the game draws through DirectDraw) */
static uint32_t u_GetDC(const uint32_t* a, int n) { return w32dlg_GetDC(a, n); }
static uint32_t u_ReleaseDC(const uint32_t* a, int n) { return w32dlg_ReleaseDC(a, n); }
static uint32_t u_BeginPaint(const uint32_t* a, int n) { return w32dlg_BeginPaint(a, n); }
static uint32_t u_EndPaint(const uint32_t* a, int n) { return w32dlg_EndPaint(a, n); }
static uint32_t u_FillRect(const uint32_t* a, int n) { return w32dlg_FillRect(a, n); }
static uint32_t u_FrameRect(const uint32_t* a, int n) { return w32dlg_FrameRect(a, n); }
static uint32_t u_DrawTextA(const uint32_t* a, int n) { return w32dlg_DrawTextA(a, n); }

void rt_bind_win32_user(void)
{
    static const struct { const char* n; RtFn f; } t[] = {
        {"AdjustWindowRect", u_AdjustWindowRect}, {"BeginPaint", u_BeginPaint}, {"CallWindowProcA", u_CallWindowProcA},
        {"ClientToScreen", u_ClientToScreen}, {"ClipCursor", u_ClipCursor},
        {"CreateAcceleratorTableA", u_CreateAcceleratorTableA}, {"CreateDialogParamA", u_CreateDialogParamA},
        {"CreateWindowExA", u_CreateWindowExA}, {"DefWindowProcA", u_DefWindowProcA},
        {"DestroyAcceleratorTable", u_DestroyAcceleratorTable}, {"DestroyWindow", u_DestroyWindow},
        {"DialogBoxParamA", u_DialogBoxParamA}, {"DispatchMessageA", u_DispatchMessageA}, {"DrawMenuBar", u_DrawMenuBar},
        {"DrawTextA", u_DrawTextA}, {"EnableWindow", u_EnableWindow}, {"EndDialog", u_EndDialog},
        {"EndPaint", u_EndPaint}, {"FillRect", u_FillRect}, {"FlashWindow", u_FlashWindow}, {"FrameRect", u_FrameRect},
        {"GetAsyncKeyState", u_GetAsyncKeyState}, {"GetClientRect", u_GetClientRect}, {"GetCursorPos", u_GetCursorPos},
        {"GetDC", u_GetDC}, {"GetDlgItem", u_GetDlgItem}, {"GetFocus", u_GetFocus}, {"GetMenu", u_GetMenu},
        {"GetMenuItemInfoA", u_GetMenuItemInfoA}, {"GetMessagePos", u_GetMessagePos}, {"GetParent", u_GetParent},
        {"GetPropA", u_GetPropA}, {"GetSysColor", u_GetSysColor}, {"GetSystemMetrics", u_GetSystemMetrics},
        {"GetWindowLongA", u_GetWindowLongA}, {"GetWindowRect", u_GetWindowRect}, {"IntersectRect", u_IntersectRect},
        {"InvalidateRect", u_InvalidateRect}, {"IsDialogMessageA", u_IsDialogMessageA}, {"IsIconic", u_IsIconic},
        {"LoadAcceleratorsA", u_LoadAcceleratorsA}, {"LoadBitmapA", u_LoadBitmapA}, {"LoadCursorA", u_LoadCursorA},
        {"LoadIconA", u_LoadIconA}, {"LoadImageA", u_LoadImageA}, {"LoadMenuA", u_LoadMenuA},
        {"LoadStringA", u_LoadStringA}, {"MessageBeep", u_MessageBeep}, {"MessageBoxA", u_MessageBoxA},
        {"MoveWindow", u_MoveWindow}, {"OffsetRect", u_OffsetRect}, {"PeekMessageA", u_PeekMessageA},
        {"PostQuitMessage", u_PostQuitMessage}, {"PostThreadMessageA", u_PostThreadMessageA},
        {"RegisterClassA", u_RegisterClassA}, {"RegisterWindowMessageA", u_RegisterWindowMessageA},
        {"ReleaseDC", u_ReleaseDC}, {"RemovePropA", u_RemovePropA}, {"ScreenToClient", u_ScreenToClient},
        {"SendMessageA", u_SendMessageA}, {"SetActiveWindow", u_SetActiveWindow}, {"SetClassLongA", u_SetClassLongA},
        {"SetCursorPos", u_SetCursorPos}, {"SetFocus", u_SetFocus}, {"SetForegroundWindow", u_SetForegroundWindow},
        {"SetMenu", u_SetMenu}, {"SetMenuItemInfoA", u_SetMenuItemInfoA}, {"SetPropA", u_SetPropA},
        {"SetRect", u_SetRect}, {"SetWindowLongA", u_SetWindowLongA}, {"SetWindowPos", u_SetWindowPos},
        {"ShowCursor", u_ShowCursor}, {"ShowWindow", u_ShowWindow}, {"TranslateAcceleratorA", u_TranslateAcceleratorA},
        {"TranslateMessage", u_TranslateMessage}, {"UpdateWindow", u_UpdateWindow}, {"WaitMessage", u_WaitMessage},
        {"GetActiveWindow", u_GetActiveWindow},
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        char a1[80], a2[80];
        snprintf(a1, sizeof a1, "USER32.dll!%s", t[i].n);
        snprintf(a2, sizeof a2, "USER32.DLL!%s", t[i].n);
        int any = 0;
        if (rt_import_slot(a1)) { rt_bind_import(strdup(a1), t[i].f); any = 1; }
        if (rt_import_slot(a2)) { rt_bind_import(strdup(a2), t[i].f); any = 1; }
        if (!any) fprintf(stderr, "tm2: host: USER32 %s is not imported\n", t[i].n);
    }
}

RT_BIND_AREA(win32_user)
