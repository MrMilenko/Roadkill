/*
 * pf_mouse.c: TM2.EXE's mouse module (see pf_mouse.h, docs/PLATFORM.md "Input: mouse").
 */
#include "pf_mouse.h"

#include <string.h>

#define SM_MOUSEPRESENT 0x13
#define SM_CMOUSEBUTTONS 0x2b
#define HKEY_CURRENT_USER 0x80000001u

/* the registry part shared by 0x004b2820 and 0x004b295c: HKEY_CURRENT_USER
   "Control Panel\Mouse" "SwapMouseButtons" (buffer 0x50 bytes); "1" swaps. The close is called
   only when the open succeeded. Returns 1 when the value is "1". */
static int read_swap(PfMouseSys* g) {
    const PfMouseBackend* be = g->be;
    char buf[0x50];
    int32_t size = 0x50;
    int swap = 0;
    memset(buf, 0, sizeof buf);
    be->reg_init(be->user, "Control Panel\\Mouse");
    if (be->reg_open(be->user, HKEY_CURRENT_USER) != 0) return 0;
    if (be->reg_read(be->user, "SwapMouseButtons", buf, &size) == 0) {
        buf[sizeof buf - 1] = 0;
        swap = strcmp(buf, "1") == 0;   /* lstrcmpA(buf, "1") == 0 */
    }
    be->reg_close(be->user);
    return swap;
}

/* 0x004b2820 mouse_init(eax state): defaults, present / button count from GetSystemMetrics,
   middle key 4 every call; the first call only: keys 1 / 2, swapped from the registry, and the
   "unread" flag cleared (even when the registry open failed). Returns the state. */
PfMouse* pf_mouse_init(PfMouseSys* g, PfMouse* m) {
    const PfMouseBackend* be = g->be;
    m->present = 0; m->buttons = 0; m->x = -1; m->y = -1;
    m->bits = 0; m->left = 0; m->right = 0; m->middle = 0;
    m->ref_w = 640; m->ref_h = 480; m->range = 0x7f; m->unused2c = 0; m->dead = 0x19;
    g->vk_middle = 4;
    int32_t r = be->get_system_metrics(be->user, SM_MOUSEPRESENT);
    if (r != 0) {
        r = be->get_system_metrics(be->user, SM_CMOUSEBUTTONS);
        m->present = 1;
    } else {
        m->present = r;
    }
    m->buttons = r;
    if (g->registry_unread != 0) {
        g->vk_primary = 1;
        g->vk_secondary = 2;
        if (read_swap(g)) { g->vk_primary = 2; g->vk_secondary = 1; }
        g->registry_unread = 0;
    }
    return m;
}

/* 0x004b295c: the registry read alone; "1" -> primary 2 / secondary 1, else the keys stay */
void pf_mouse_reread_swap(PfMouseSys* g) {
    if (read_swap(g)) { g->vk_secondary = 1; g->vk_primary = 2; }
}

static int key_down(PfMouseSys* g, int32_t vk) {
    return (g->be->get_async_key_state(g->be->user, vk) & 0x8001) != 0;   /* down now or pressed since */
}

/* 0x004b29e0 mouse_poll(eax state): nothing without a mouse. Buttons from GetAsyncKeyState
   (& 0x8001) of the three keys (primary sets +0x10 to 1, the others or in 2 / 4), the cursor
   (GetCursorPos, ScreenToClient(main window); its failure -> -1 / -1; GetCursorPos's result is
   not checked), and the reference size from the screen size. */
void pf_mouse_poll(PfMouseSys* g, PfMouse* m) {
    const PfMouseBackend* be = g->be;
    if (m->present == 0) return;
    m->bits = 0; m->left = 0; m->right = 0; m->middle = 0;
    be->get_hwnd(be->user);
    if (key_down(g, g->vk_primary)) { m->left = 1; m->bits = 1; }
    if (key_down(g, g->vk_secondary)) { m->right = 1; m->bits = (m->bits & ~0xff) | ((m->bits | 2) & 0xff); }
    if (key_down(g, g->vk_middle)) { m->middle = 1; m->bits = (m->bits & ~0xff) | ((m->bits | 4) & 0xff); }
    PfPoint pt = {0, 0};
    be->get_cursor_pos(be->user, &pt);
    if (be->screen_to_client(be->user, &pt)) { m->x = pt.x; m->y = pt.y; }
    else { m->y = -1; m->x = -1; }
    m->ref_w = be->screen_width(be->user);
    m->ref_h = be->screen_height(be->user);
}

/* 0x004b2ad4 (eax state, edx POINT*): 0 without a mouse; else GetCursorPos(pt), returns
   ScreenToClient(main window, pt) */
int32_t pf_mouse_cursor_client(PfMouseSys* g, const PfMouse* m, PfPoint* pt) {
    const PfMouseBackend* be = g->be;
    if (m->present == 0) return 0;
    be->get_hwnd(be->user);
    be->get_cursor_pos(be->user, pt);
    return be->screen_to_client(be->user, pt);
}

static int32_t half(int32_t v) { return v / 2; }   /* sar / sub / sar: rounds toward zero */

/* 0x004b2b00 (eax state, edx int* x, ebx int* y): x = cursor.x - ref_w / 2, y = ref_h / 2 -
   cursor.y (y up), from a fresh GetCursorPos / ScreenToClient (the point zeroed first). Returns
   ScreenToClient's BOOL; 0 without a mouse or when it fails (outputs untouched). */
int32_t pf_mouse_centered_pos(PfMouseSys* g, const PfMouse* m, int32_t* x, int32_t* y) {
    const PfMouseBackend* be = g->be;
    if (m->present == 0) return 0;
    be->get_hwnd(be->user);
    PfPoint pt = {0, 0};
    be->get_cursor_pos(be->user, &pt);
    int32_t r = be->screen_to_client(be->user, &pt);
    if (r == 0) return 0;
    *x = (int32_t)((uint32_t)pt.x - (uint32_t)half(m->ref_w));
    int32_t t = (int32_t)((uint32_t)pt.y - (uint32_t)half(m->ref_h));
    *y = (int32_t)(0u - (uint32_t)t);
    return r;
}

/* the clamp shared by the four axes: min(v, range + dead) - dead, not below 0 */
static int32_t axis_clamp(const PfMouse* m, int32_t v, int32_t lim) {
    if (v > lim) v = lim;
    v = (int32_t)((uint32_t)v - (uint32_t)m->dead);
    return v < 0 ? 0 : v;
}

/* 0x004b2b84 (x) / 0x004b2c44 (y): the cursor past the centre toward + as 0..range:
   d = pos - size / 2; 0 if d < 0 or the far half (size - size / 2) is 0; else
   d * (range + dead) / far half (32 bit imul, idiv) */
static int32_t axis_pos(const PfMouse* m, int32_t pos, int32_t size) {
    if (m->present == 0) return 0;
    int32_t h = half(size);
    int32_t span = (int32_t)((uint32_t)size - (uint32_t)h);
    int32_t d = (int32_t)((uint32_t)pos - (uint32_t)h);
    int32_t lim = (int32_t)((uint32_t)m->dead + (uint32_t)m->range);
    int32_t v;
    if (d < 0) v = 0;
    else if (span == 0) v = 0;
    else v = (int32_t)((uint32_t)d * (uint32_t)lim) / span;
    return axis_clamp(m, v, lim);
}

/* 0x004b2bdc (x) / 0x004b2c9c (y): toward -: d = pos - size / 2; 0 if d >= 0 or |size / 2| == 0;
   else |d| * (range + dead) / |size / 2| */
static int32_t axis_neg(const PfMouse* m, int32_t pos, int32_t size) {
    if (m->present == 0) return 0;
    int32_t h = half(size);
    int32_t d = (int32_t)((uint32_t)pos - (uint32_t)h);
    int32_t a = h < 0 ? (int32_t)(0u - (uint32_t)h) : h;   /* neg, cdq / xor / sub */
    int32_t lim = (int32_t)((uint32_t)m->dead + (uint32_t)m->range);
    int32_t v;
    if (d >= 0) v = 0;
    else if (a == 0) v = 0;
    else {
        int32_t ad = (int32_t)(0u - (uint32_t)d);
        v = (int32_t)((uint32_t)ad * (uint32_t)lim) / a;
    }
    return axis_clamp(m, v, lim);
}

int32_t pf_mouse_axis_right(const PfMouse* m) { return axis_pos(m, m->x, m->ref_w); }
int32_t pf_mouse_axis_left(const PfMouse* m) { return axis_neg(m, m->x, m->ref_w); }
int32_t pf_mouse_axis_down(const PfMouse* m) { return axis_pos(m, m->y, m->ref_h); }
int32_t pf_mouse_axis_up(const PfMouse* m) { return axis_neg(m, m->y, m->ref_h); }

/* the move shared by the four setters: ClientToScreen(main window, pt), SetCursorPos(pt);
   returns ClientToScreen's BOOL (SetCursorPos's result is dropped) */
static int32_t move_to(PfMouseSys* g, int32_t x, int32_t y) {
    const PfMouseBackend* be = g->be;
    be->get_hwnd(be->user);
    PfPoint pt = {x, y};
    int32_t r = be->client_to_screen(be->user, &pt);
    be->set_cursor_pos(be->user, pt.x, pt.y);
    return r;
}

/* 0x004b2d00 (edx POINT*): moves the cursor there (client coordinates) when 0 <= x < screen
   width and 0 <= y < screen height; else 0. eax is not used. */
int32_t pf_mouse_set_cursor_pt(PfMouseSys* g, const PfPoint* p) {
    const PfMouseBackend* be = g->be;
    int32_t w = be->screen_width(be->user), h = be->screen_height(be->user);
    if (p->x >= w || p->x < 0 || p->y >= h || p->y < 0) return 0;
    return move_to(g, p->x, p->y);
}

/* 0x004b2d60 (edx x, ebx y): as 0x004b2d00 with the values */
int32_t pf_mouse_set_cursor(PfMouseSys* g, int32_t x, int32_t y) {
    const PfMouseBackend* be = g->be;
    int32_t w = be->screen_width(be->user), h = be->screen_height(be->user);
    if (x >= w || x < 0 || y >= h || y < 0) return 0;
    return move_to(g, x, y);
}

/* 0x004b2db8 (eax state, edx x): new x, the current y (GetCursorPos / ScreenToClient, result
   not checked). Without a mouse the current point is not read and the original uses stack
   garbage for y (ORIGINAL BUG); the port uses -1 there (always out of range: no move). */
int32_t pf_mouse_set_cursor_x(PfMouseSys* g, const PfMouse* m, int32_t x) {
    const PfMouseBackend* be = g->be;
    int32_t w = be->screen_width(be->user), h = be->screen_height(be->user);
    PfPoint cur = {-1, -1};
    if (m->present != 0) {
        be->get_hwnd(be->user);
        be->get_cursor_pos(be->user, &cur);
        be->screen_to_client(be->user, &cur);
    }
    if (x >= w || x < 0 || h <= cur.y || cur.y < 0) return 0;
    return move_to(g, x, cur.y);
}

/* 0x004b2e48 (eax state, edx y): new y, the current x (as 0x004b2db8) */
int32_t pf_mouse_set_cursor_y(PfMouseSys* g, const PfMouse* m, int32_t y) {
    const PfMouseBackend* be = g->be;
    int32_t w = be->screen_width(be->user), h = be->screen_height(be->user);
    PfPoint cur = {-1, -1};
    if (m->present != 0) {
        be->get_hwnd(be->user);
        be->get_cursor_pos(be->user, &cur);
        be->screen_to_client(be->user, &cur);
    }
    if (cur.x >= w || cur.x < 0 || y >= h || y < 0) return 0;
    return move_to(g, cur.x, y);
}

/* 0x004b2ee0 / 0x004b2f00: (GetAsyncKeyState(vk) & 0x8001) != 0 for the primary / secondary key */
int32_t pf_mouse_primary_down(PfMouseSys* g) { return key_down(g, g->vk_primary); }
int32_t pf_mouse_secondary_down(PfMouseSys* g) { return key_down(g, g->vk_secondary); }
