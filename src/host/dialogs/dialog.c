/*
 * dialog.c: the dialog manager, the system classes' window procedures, painting and the overlay
 * (dialogs.h), port code.
 *
 * Window procedures: each system class gets a fake game address bound to a host adapter, so the
 * game reaches it through SendMessageA / CallWindowProcA like a USER32 procedure. A record per
 * window (DlgWin) is made at WM_NCCREATE and freed at WM_NCDESTROY.
 *
 * Coordinates: the main window's client area is the "screen" (dx_sdl.h); a child's position is
 * its parent's client origin plus its own; a dialog (a popup owned by the main window) is placed
 * at its own position. A dialog's window rectangle in win32_user.c is its client area: the frame
 * and caption are drawn around it.
 *
 * Painting happens in the tick (the main thread's PeekMessageA): a window whose state signature
 * (style, rectangle, focus, its own changes) moved, or that was invalidated, repaints into its
 * picture: WM_DRAWITEM to the parent for BS_OWNERDRAW buttons (as Windows does at WM_PAINT),
 * the control's own look otherwise. The pictures are composed into the overlay in z order
 * (children of the main window in creation order, then each dialog and its children, an open
 * combo box's list last).
 */
#include "host/dialogs/dialogs.h"
#include "host/dx_sdl.h"
#include "host/win32_host.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define LO(x) ((x) & 0xffffu)
#define HI(x) ((x) >> 16)
#define WS_CHILD 0x40000000u
#define WS_VISIBLE 0x10000000u
#define WS_DISABLED 0x08000000u
#define WS_TABSTOP 0x00010000u
#define WS_GROUP 0x00020000u
#define WS_CAPTION 0x00c00000u
#define FRAME 3
#define CAPTION 18

static int dlg_log = -1;
static int logging(void) { if (dlg_log < 0) { const char* l = getenv("TM2_DLG_LOG"); dlg_log = l ? atoi(l) > 1 ? 2 : 1 : 0; } return dlg_log; }

/* ---- imports ---- */

uint32_t dlg_import(const char* name, const uint32_t* v, int n)
{
    static struct { const char* n; uint32_t slot; } cache[32];
    uint32_t slot = 0;
    int i;
    for (i = 0; i < 32 && cache[i].n; i++) if (!strcmp(cache[i].n, name)) { slot = cache[i].slot; break; }
    if (!slot) {
        char full[80];
        snprintf(full, sizeof full, "USER32.dll!%s", name);
        slot = rt_import_slot(full);
        if (!slot) { snprintf(full, sizeof full, "USER32.DLL!%s", name); slot = rt_import_slot(full); }
        if (i < 32) { cache[i].n = name; cache[i].slot = slot; }
    }
    if (!slot) { fprintf(stderr, "tm2 host: dialogs: %s is not imported\n", name); return 0; }
    return rt_call(slot, v, n);
}
static uint32_t imp1(const char* nm, uint32_t a) { uint32_t v[1] = {a}; return dlg_import(nm, v, 1); }
static uint32_t imp2(const char* nm, uint32_t a, uint32_t b) { uint32_t v[2] = {a, b}; return dlg_import(nm, v, 2); }
static uint32_t imp3(const char* nm, uint32_t a, uint32_t b, uint32_t c) { uint32_t v[3] = {a, b, c}; return dlg_import(nm, v, 3); }

uint32_t dlg_send(uint32_t h, uint32_t msg, uint32_t wp, uint32_t lp)
{
    if (!h) return 0;
    uint32_t v[4] = {h, msg, wp, lp};
    return dlg_import("SendMessageA", v, 4);
}

int dlg_info(uint32_t h, DlgInfo* o)
{
    memset(o, 0, sizeof *o);
    if (!h) return 0;
    uint32_t r = rt_frame_push(16);
    int ok = imp2("GetWindowRect", h, r) != 0;
    if (ok) {
        o->hwnd = h;
        o->x = (int32_t)rt_r32(r);
        o->y = (int32_t)rt_r32(r + 4);
        o->w = (int32_t)rt_r32(r + 8) - o->x;
        o->h = (int32_t)rt_r32(r + 12) - o->y;
        o->style = imp2("GetWindowLongA", h, (uint32_t)-16);
        o->exstyle = imp2("GetWindowLongA", h, (uint32_t)-20);
        o->id = imp2("GetWindowLongA", h, (uint32_t)-12);
        o->parent = imp1("GetParent", h);
    }
    rt_frame_pop(16);
    return ok;
}

uint32_t dlg_focus(void) { uint32_t v[1] = {0}; return dlg_import("GetFocus", v, 0); }
void dlg_set_focus(uint32_t h) { imp1("SetFocus", h); }

static uint32_t capture;
void dlg_capture(uint32_t h) { capture = h; }
uint32_t dlg_captured(void) { return capture; }

void dlg_notify(uint32_t ctl, uint32_t code)
{
    DlgInfo i;
    if (dlg_info(ctl, &i)) dlg_send(i.parent, 0x111, LO(i.id) | code << 16, ctl);
}

/* WM_CTLCOLOR* to the parent; *fill = the brush's colour, or def when it answered none */
uint32_t dlg_ctlcolor(uint32_t ctl, uint32_t msg, uint32_t hdc, uint32_t def, uint32_t* fill)
{
    DlgInfo i;
    gdi_dc_reset_colors(hdc, 0, def, 2);
    *fill = def;
    if (!dlg_info(ctl, &i) || !i.parent) return 0;
    uint32_t b = dlg_send(i.parent, msg, hdc, ctl);
    int ok;
    uint32_t c = gdi_brush_color(b, &ok);
    if (ok) *fill = c;
    return b;
}

/* ---- records ---- */

#define NW 256
static DlgWin* wins[NW];
static int nwins;
static uint32_t order_next;
static int overlay_changed = 1;

DlgWin* dlg_win(uint32_t h)
{
    for (int i = 0; i < nwins; i++) if (wins[i]->hwnd == h) return wins[i];
    return NULL;
}
void dlg_dirty(uint32_t h) { DlgWin* w = dlg_win(h); if (w) w->dirty = 1; }
void w32dlg_invalidate(uint32_t h) { dlg_dirty(h); }

static DlgWin* win_new(uint32_t h, int kind)
{
    if (nwins == NW) return NULL;
    DlgWin* w = calloc(1, sizeof *w);
    w->hwnd = h;
    w->kind = kind;
    w->order = order_next++;
    w->sel = -1;
    w->hot = -1;
    w->tmax = 100;
    w->tline = 1;
    w->tpage = 20;
    w->tfreq = 1;
    w->bk = w->fg = 0xffffffffu;
    w->dirty = 1;
    wins[nwins++] = w;
    return w;
}
static void win_free(DlgWin* w)
{
    for (int i = 0; i < nwins; i++)
        if (wins[i] == w) { memmove(wins + i, wins + i + 1, sizeof(DlgWin*) * (size_t)(nwins - i - 1)); nwins--; break; }
    if (capture == w->hwnd) capture = 0;
    ctl_free(w);
    free(w->text);
    free(w->pic.px);
    free(w);
    overlay_changed = 1;
}

/* ---- dialogs ---- */

static DlgWin* modal_top(void)
{
    DlgWin* m = NULL;
    for (int i = 0; i < nwins; i++) if (wins[i]->kind == K_DIALOG && wins[i]->modal && !wins[i]->ended) m = wins[i];
    return m;
}
/* the dialog (non child window) a window belongs to; 0 for the main window's children */
static uint32_t top_of(uint32_t h)
{
    DlgInfo i;
    for (int k = 0; k < 16 && dlg_info(h, &i); k++) {
        if (!(i.style & WS_CHILD)) return dlg_win(h) ? h : 0;
        h = i.parent;
    }
    return 0;
}
static int is_descendant(uint32_t h, uint32_t top)
{
    DlgInfo i;
    for (int k = 0; k < 16 && h; k++) {
        if (h == top) return 1;
        if (!dlg_info(h, &i) || !(i.style & WS_CHILD)) return 0;
        h = i.parent;
    }
    return 0;
}

int dlg_is_default_button(uint32_t h)
{
    DlgInfo i;
    if (!dlg_info(h, &i)) return 0;
    int t = (int)(i.style & 0xf);
    uint32_t f = dlg_focus();
    DlgWin* fw = dlg_win(f);
    DlgInfo fi;
    /* a focused push button is the default one; else the dialog's default id / BS_DEFPUSHBUTTON */
    if (fw && fw->kind == K_BUTTON && dlg_info(f, &fi) && fi.parent == i.parent && ((fi.style & 0xf) <= 1))
        return f == h;
    DlgWin* d = dlg_win(i.parent);
    if (d && d->kind == K_DIALOG && d->defid) return (int)LO(i.id) == d->defid;
    return t == 1;
}

/* the dialog's children in creation order */
static int children(uint32_t parent, DlgWin** out, int max)
{
    int n = 0;
    DlgInfo i;
    for (int k = 0; k < nwins && n < max; k++)
        if (wins[k]->kind != K_DIALOG && dlg_info(wins[k]->hwnd, &i) && i.parent == parent && (i.style & WS_CHILD)) out[n++] = wins[k];
    return n;
}

void dlg_radio_group(uint32_t h)
{
    DlgInfo me, i;
    if (!dlg_info(h, &me)) return;
    DlgWin* c[NW];
    int n = children(me.parent, c, NW), at = -1;
    for (int k = 0; k < n; k++) if (c[k]->hwnd == h) at = k;
    if (at < 0) return;
    int s = at;
    while (s > 0 && dlg_info(c[s]->hwnd, &i) && !(i.style & WS_GROUP)) s--;
    for (int k = s; k < n; k++) {
        if (k > s && dlg_info(c[k]->hwnd, &i) && (i.style & WS_GROUP)) break;
        if (c[k]->hwnd != h && c[k]->kind == K_BUTTON && dlg_info(c[k]->hwnd, &i) && (i.style & 0xf) == 9 && c[k]->check) {
            c[k]->check = 0;
            c[k]->dirty = 1;
        }
    }
}

static int usable(DlgWin* w)
{
    DlgInfo i;
    return dlg_info(w->hwnd, &i) && (i.style & WS_VISIBLE) && !(i.style & WS_DISABLED);
}

/* the next / previous tab stop (GetNextDlgTabItem) */
static uint32_t next_tab(uint32_t dlg, uint32_t from, int back)
{
    DlgWin* c[NW];
    int n = children(dlg, c, NW), at = -1;
    if (!n) return 0;
    for (int k = 0; k < n; k++) if (c[k]->hwnd == from) at = k;
    for (int s = 1; s <= n; s++) {
        int k = at < 0 ? (back ? n - s : s - 1) : ((at + (back ? -s : s)) % n + n) % n;
        DlgInfo i;
        if (usable(c[k]) && dlg_info(c[k]->hwnd, &i) && (i.style & WS_TABSTOP)) return c[k]->hwnd;
    }
    return 0;
}
/* the next / previous item of from's group (GetNextDlgGroupItem) */
static uint32_t next_group(uint32_t dlg, uint32_t from, int back)
{
    DlgWin* c[NW];
    int n = children(dlg, c, NW), at = -1;
    for (int k = 0; k < n; k++) if (c[k]->hwnd == from) at = k;
    if (at < 0) return 0;
    DlgInfo i;
    int s = at, e = at;
    while (s > 0 && dlg_info(c[s]->hwnd, &i) && !(i.style & WS_GROUP)) s--;
    while (e + 1 < n && dlg_info(c[e + 1]->hwnd, &i) && !(i.style & WS_GROUP)) e++;
    int len = e - s + 1;
    for (int k = 1; k < len; k++) {
        int j = s + ((at - s + (back ? -k : k)) % len + len) % len;
        if (usable(c[j]) && c[j]->kind != K_STATIC) return c[j]->hwnd;
    }
    return 0;
}

static void dlg_command(uint32_t dlg, uint32_t id)
{
    uint32_t ctl = imp2("GetDlgItem", dlg, id);
    DlgInfo i;
    if (ctl && dlg_info(ctl, &i) && (i.style & WS_DISABLED)) return;
    dlg_send(dlg, 0x111, LO(id), ctl);   /* BN_CLICKED */
}

/* IsDialogMessage on a host message (hwnd, msg, wp, lp); 1 when it was the dialog's */
static int dialog_message(uint32_t dlg, uint32_t mp, uint32_t hwnd, uint32_t msg, uint32_t wp)
{
    if (!is_descendant(hwnd, dlg)) return 0;
    if (msg == 0x100) {
        uint32_t code = dlg_send(hwnd, 0x87, wp, mp);   /* WM_GETDLGCODE */
        uint32_t f = dlg_focus();
        if (!(code & 4)) {
            if (wp == 0x09 && !(code & 2)) {
                const uint8_t* k = host_keyboard_state(NULL);
                int back = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT];
                uint32_t t = next_tab(dlg, f, back);
                if (t) { dlg_set_focus(t); DlgWin* tw = dlg_win(t); if (tw && tw->kind == K_EDIT) dlg_send(t, 0xb1, 0, 0xffffffffu); }
                return 1;
            }
            if (wp >= 0x25 && wp <= 0x28 && !(code & 1)) {
                uint32_t t = next_group(dlg, f, wp == 0x25 || wp == 0x26);
                if (t) {
                    dlg_set_focus(t);
                    DlgWin* tw = dlg_win(t);
                    DlgInfo ti;
                    if (tw && tw->kind == K_BUTTON && dlg_info(t, &ti) && (ti.style & 0xf) == 9) dlg_send(t, 0xf5, 0, 0);
                }
                return 1;
            }
            if (wp == 0x0d) {
                DlgWin* fw = dlg_win(f);
                DlgInfo fi;
                uint32_t id = 1;
                if (fw && fw->kind == K_BUTTON && dlg_info(f, &fi) && ((fi.style & 0xf) <= 1 || (fi.style & 0xf) == 0xb)) id = LO(fi.id);
                else {
                    uint32_t d = dlg_send(dlg, 0x400, 0, 0);   /* DM_GETDEFID */
                    if (HI(d) == 0x534b) id = LO(d);
                }
                dlg_command(dlg, id);
                return 1;
            }
            if (wp == 0x1b) { dlg_command(dlg, 2); return 1; }
        }
    }
    uint32_t v[1] = {mp};
    dlg_import("TranslateMessage", v, 1);
    dlg_import("DispatchMessageA", v, 1);
    return 1;
}

uint32_t w32dlg_IsDialogMessageA(const uint32_t* a, int n)
{
    uint32_t dlg = W(0), mp = W(1);
    if (!dlg_win(dlg) || !mp) return 0;
    return (uint32_t)dialog_message(dlg, mp, rt_r32(mp), rt_r32(mp + 4), rt_r32(mp + 8));
}

uint32_t w32dlg_EndDialog(const uint32_t* a, int n)
{
    DlgWin* w = dlg_win(W(0));
    if (!w || w->kind != K_DIALOG) return 0;
    w->ended = 1;
    w->result = (int32_t)W(1);
    if (!w->modal) imp2("ShowWindow", w->hwnd, 0);
    return 1;
}

/* the template: DIALOG or DIALOGEX at game address p */
typedef struct { uint32_t p; } Rd;
static uint16_t rd16(Rd* r) { uint16_t v = rt_r16(r->p); r->p += 2; return v; }
static uint32_t rd32(Rd* r) { uint32_t v = rt_r32(r->p); r->p += 4; return v; }
/* a sz_Or_Ord: *ord = the ordinal, or the string copied (ANSI) into s */
static void rd_sz(Rd* r, char* s, size_t cap, uint32_t* ord)
{
    *ord = 0;
    s[0] = 0;
    uint16_t c = rt_r16(r->p);
    if (c == 0) { r->p += 2; return; }
    if (c == 0xffff) { r->p += 2; *ord = rd16(r); return; }
    size_t k = 0;
    while ((c = rd16(r)) != 0) if (k + 1 < cap) s[k++] = c < 0x100 ? (char)c : '?';
    s[k] = 0;
}

static uint32_t class_str(uint32_t ord, const char* name)
{
    static const char* sys[6] = {"Button", "Edit", "Static", "ListBox", "ScrollBar", "ComboBox"};
    static uint32_t cached[6];
    if (ord >= 0x80 && ord <= 0x85) {
        if (!cached[ord - 0x80]) cached[ord - 0x80] = w32_static_str(sys[ord - 0x80]);
        return cached[ord - 0x80];
    }
    return w32_static_str(name);   /* (rare: a named class) */
}

static DlgWin* pending_dialog;
static uint32_t pending_proc, pending_param;

/* creates the dialog of template tpl (game address) owned by owner; 0 on failure */
static uint32_t create_dialog(uint32_t inst, uint32_t tpl, uint32_t owner, uint32_t proc, uint32_t param, int modal)
{
    Rd r = {tpl};
    int ex = rt_r16(tpl) == 1 && rt_r16(tpl + 2) == 0xffff;
    uint32_t style, exstyle;
    if (ex) { r.p += 8; exstyle = rd32(&r); style = rd32(&r); }
    else { style = rd32(&r); exstyle = rd32(&r); }
    int nitems = rd16(&r);
    int x = (int16_t)rd16(&r), y = (int16_t)rd16(&r), cx = (int16_t)rd16(&r), cy = (int16_t)rd16(&r);
    char menu[64], cls[64], title[128], face[64];
    uint32_t ord;
    rd_sz(&r, menu, sizeof menu, &ord);
    rd_sz(&r, cls, sizeof cls, &ord);
    rd_sz(&r, title, sizeof title, &ord);
    int pt = 8;
    if (style & 0x40) {   /* DS_SETFONT */
        pt = rd16(&r);
        if (ex) r.p += 4;
        rd_sz(&r, face, sizeof face, &ord);
    }
    /* dialog base units of MS Sans Serif at 96 dpi: 8 pt -> 6 x 13 */
    int buy = pt * 13 / 8, bux = buy * 6 / 13;
    if (buy < 8) buy = 8;
    if (bux < 4) bux = 4;
#define DX(v) ((v) * bux / 4)
#define DY(v) ((v) * buy / 8)
    int w = DX(cx), h = DY(cy);
    int dw = 640, dh = 480;
    dx_sdl_client_size(&dw, &dh);
    int nc_t = (style & WS_CAPTION) == WS_CAPTION ? FRAME + CAPTION : FRAME;
    int px = DX(x), py = DY(y);
    if ((style & 0x800) || !(style & WS_CHILD)) {   /* DS_CENTER (and plain popups: centred on the owner) */
        if ((style & 0x800) || (x == 0 && y == 0)) { px = (dw - w) / 2; py = (dh - h - nc_t + FRAME) / 2 + nc_t - FRAME; }
    }
    if (logging()) fprintf(stderr, "tm2 host: dialog '%s' %s %dx%d at %d,%d, %d items\n", title, modal ? "modal" : "modeless", w, h, px, py, nitems);
    uint32_t s = rt_frame_push(512);
    w32_put_str(s, title, 256);
    pending_proc = proc;
    pending_param = param;
    pending_dialog = NULL;
    uint32_t cv[12] = {exstyle, w32_static_str("#32770"), s, style & ~WS_VISIBLE, (uint32_t)px, (uint32_t)py, (uint32_t)w, (uint32_t)h,
                       owner, 0, inst, 0};
    uint32_t hd = dlg_import("CreateWindowExA", cv, 12);
    DlgWin* dw_ = dlg_win(hd);
    pending_proc = 0;
    if (!hd || !dw_) { rt_frame_pop(512); return 0; }
    dw_->modal = modal;
    dw_->owner = owner;
    imp3("SetWindowLongA", hd, 4, proc);    /* DWL_DLGPROC */
    uint32_t font = w32gdi_CreateFontA((const uint32_t[]){(uint32_t)-(pt * 96 / 72), 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 14);
    dw_->font = font;
    uint32_t first = 0;
    for (int k = 0; k < nitems; k++) {
        r.p = (r.p + 3) & ~3u;
        uint32_t cst, cex, id;
        if (ex) { r.p += 4; cex = rd32(&r); cst = rd32(&r); }
        else { cst = rd32(&r); cex = rd32(&r); }
        int ix = (int16_t)rd16(&r), iy = (int16_t)rd16(&r), icx = (int16_t)rd16(&r), icy = (int16_t)rd16(&r);
        id = ex ? rd32(&r) : rd16(&r);
        char ccls[64], text[256];
        uint32_t cord, tord;
        rd_sz(&r, ccls, sizeof ccls, &cord);
        rd_sz(&r, text, sizeof text, &tord);
        uint16_t extra = rd16(&r);
        uint32_t data = extra ? r.p : 0;
        r.p += extra;
        w32_put_str(s, text, 256);
        uint32_t iv[12] = {cex, class_str(cord, ccls), tord ? 0 : s, cst | WS_CHILD, (uint32_t)DX(ix), (uint32_t)DY(iy),
                           (uint32_t)DX(icx), (uint32_t)DY(icy), hd, id & 0xffff, inst, data};
        uint32_t hc = dlg_import("CreateWindowExA", iv, 12);
        if (!hc) { fprintf(stderr, "tm2 host: dialog item %u (%s) not created\n", id, cord ? "system" : ccls); continue; }
        dlg_send(hc, 0x30, font, 0);   /* WM_SETFONT */
        if (!first && (cst & WS_TABSTOP) && !(cst & WS_DISABLED)) first = hc;
        if (cord == 0x80 && (cst & 0xf) == 1 && !dw_->defid) dw_->defid = (int)(id & 0xffff);
    }
    rt_frame_pop(512);
#undef DX
#undef DY
    /* WM_INITDIALOG: TRUE puts the focus on the first tab stop */
    uint32_t prev = dlg_focus();
    dw_->prev_focus = prev;
    if (dlg_send(hd, 0x110, first, param) && first && dlg_win(hd)) dlg_set_focus(first);
    if (dlg_win(hd) && (style & WS_VISIBLE)) imp2("ShowWindow", hd, 5);
    return hd;
}

static uint32_t template_of(uint32_t name)
{
    uint32_t de = w32_find_resource(5, name);
    return de ? 0x00400000u + rt_r32(de) : 0;
}

/* CreateDialogParamA(inst, name, owner, proc, param) */
uint32_t w32dlg_CreateDialogParamA(const uint32_t* a, int n)
{
    uint32_t tpl = template_of(W(1));
    if (!tpl) { fprintf(stderr, "tm2 host: CreateDialogParamA: no template %x\n", W(1)); return 0; }
    return create_dialog(W(0), tpl, W(2), W(3), W(4), 0);
}

static void controller_keys(uint32_t dlg);

/* DialogBoxParamA(inst, name, owner, proc, param): the modal loop */
uint32_t w32dlg_DialogBoxParamA(const uint32_t* a, int n)
{
    uint32_t tpl = template_of(W(1)), owner = W(2);
    if (!tpl) { fprintf(stderr, "tm2 host: DialogBoxParamA: no template %x\n", W(1)); return 0xffffffffu; }
    uint32_t hd = create_dialog(W(0), tpl, owner, W(3), W(4), 1);
    if (!hd) return 0xffffffffu;
    DlgWin* d = dlg_win(hd);
    uint32_t prev = d->prev_focus;
    int was_enabled = owner && !imp1("EnableWindow", owner) ? 1 : 0;
    (void)was_enabled;
    imp2("ShowWindow", hd, 5);
    uint32_t mp = rt_frame_push(32);
    int32_t result = -1;
    int quit = 0;
    uint32_t quit_code = 0;
    while ((d = dlg_win(hd)) && !d->ended) {
        uint32_t pv[5] = {mp, 0, 0, 0, 1};
        if (dlg_import("PeekMessageA", pv, 5)) {
            uint32_t h = rt_r32(mp), m = rt_r32(mp + 4);
            if (m == 0x12) { quit = 1; quit_code = rt_r32(mp + 8); break; }   /* WM_QUIT */
            d->idle_sent = 0;
            if (!dialog_message(hd, mp, h, m, rt_r32(mp + 8))) {
                /* input for the disabled owner is dropped */
                if (((m >= 0x100 && m <= 0x108) || (m >= 0x200 && m <= 0x20e)) && !is_descendant(h, hd)) continue;
                uint32_t v[1] = {mp};
                dlg_import("TranslateMessage", v, 1);
                dlg_import("DispatchMessageA", v, 1);
            }
        } else {
            if (!d->idle_sent && owner) {
                d->idle_sent = 1;
                dlg_send(owner, 0x121, 0, hd);   /* WM_ENTERIDLE (MSGF_DIALOGBOX) */
                continue;
            }
            controller_keys(hd);
            uint32_t v[1] = {0};
            dlg_import("WaitMessage", v, 0);
            if ((d = dlg_win(hd))) d->idle_sent = 0;
        }
    }
    rt_frame_pop(32);
    if ((d = dlg_win(hd))) result = d->result;
    if (owner) imp2("EnableWindow", owner, 1);
    if (dlg_win(hd)) imp1("DestroyWindow", hd);
    if (prev) dlg_set_focus(prev);
    else if (owner) dlg_set_focus(owner);
    if (quit) imp1("PostQuitMessage", quit_code);
    if (logging()) fprintf(stderr, "tm2 host: dialog ended: %d\n", result);
    return (uint32_t)result;
}

/* a game controller in a modal dialog: d-pad = arrows, A = Enter, B = Esc */
static void controller_keys(uint32_t dlg)
{
    static SDL_GameController* pads[4];
    static uint8_t prev[4][16];
    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER)) return;
    static const struct { SDL_GameControllerButton b; uint32_t vk; } map[] = {
        {SDL_CONTROLLER_BUTTON_DPAD_UP, 0x26}, {SDL_CONTROLLER_BUTTON_DPAD_DOWN, 0x28}, {SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0x25},
        {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 0x27}, {SDL_CONTROLLER_BUTTON_A, 0x0d}, {SDL_CONTROLLER_BUTTON_B, 0x1b},
    };
    int nj = SDL_NumJoysticks();
    for (int j = 0; j < nj && j < 4; j++) {
        if (!pads[j] && SDL_IsGameController(j)) pads[j] = SDL_GameControllerOpen(j);
        if (!pads[j]) continue;
        for (size_t k = 0; k < sizeof map / sizeof map[0]; k++) {
            uint8_t s = SDL_GameControllerGetButton(pads[j], map[k].b);
            if (s && !prev[j][k]) {
                uint32_t f = dlg_focus();
                if (!f) f = dlg;
                uint32_t mp = rt_frame_push(32);
                rt_w32(mp, f);
                rt_w32(mp + 4, 0x100);
                rt_w32(mp + 8, map[k].vk);
                rt_w32(mp + 12, 1);
                dialog_message(dlg, mp, f, 0x100, map[k].vk);
                rt_w32(mp + 4, 0x101);
                rt_w32(mp + 12, 0xc0000001u);
                if (dlg_win(dlg)) dlg_send(dlg_focus(), 0x101, map[k].vk, 0xc0000001u);
                rt_frame_pop(32);
            }
            prev[j][k] = s;
        }
    }
}

/* ---- window procedures ---- */

static uint32_t def_proc(uint32_t h, uint32_t msg, uint32_t wp, uint32_t lp)
{
    uint32_t v[4] = {h, msg, wp, lp};
    return dlg_import("DefWindowProcA", v, 4);
}

static uint32_t common(DlgWin* w, uint32_t h, uint32_t msg, uint32_t wp, uint32_t lp)
{
    switch (msg) {
    case 0x0c:                                     /* WM_SETTEXT */
        free(w->text);
        w->text = NULL;
        if (lp) { const char* s = w32_str(lp); w->text = malloc(strlen(s) + 1); strcpy(w->text, s); }
        w->dirty = 1;
        return 1;
    case 0x0d: {                                   /* WM_GETTEXT */
        const char* s = w->text ? w->text : "";
        if (!wp || !lp) return 0;
        w32_put_str(lp, s, wp);
        size_t l = strlen(s);
        return (uint32_t)(l < wp ? l : wp - 1);
    }
    case 0x0e: return w->text ? (uint32_t)strlen(w->text) : 0;
    case 0x30: w->font = wp; w->dirty = 1; return 0;               /* WM_SETFONT */
    case 0x31: return w->font;
    case 0x87: return (uint32_t)ctl_dlgcode(w, wp);                 /* WM_GETDLGCODE */
    case 0x07: case 0x08: case 0x0a: w->dirty = 1; return 0;       /* focus, WM_ENABLE */
    case 0x0f: w->dirty = 1; return 0;                              /* WM_PAINT */
    case 0x14: return 1;                                            /* WM_ERASEBKGND */
    case 0x84: return 1;                                            /* WM_NCHITTEST: HTCLIENT */
    case 0x21: return 1;                                            /* WM_MOUSEACTIVATE: MA_ACTIVATE */
    case 0x20: return 0;
    case 0x82: win_free(w); return 0;                               /* WM_NCDESTROY */
    case 0x02: return 0;
    case 0x10: if (w->kind != K_DIALOG) return 0; break;
    default: break;
    }
    return def_proc(h, msg, wp, lp);
}

/* the messages a dialog procedure answers directly instead of through DWL_MSGRESULT */
static int direct_result(uint32_t msg)
{
    return (msg >= 0x132 && msg <= 0x138) || msg == 0x110 || msg == 0x39 || msg == 0x2e || msg == 0x2f || msg == 0x37 || msg == 0x4e ? 0 : 1;
}

static uint32_t dialog_proc(DlgWin* w, uint32_t h, uint32_t msg, uint32_t wp, uint32_t lp)
{
    if (w->dlgproc) {
        imp3("SetWindowLongA", h, 0, 0);   /* DWL_MSGRESULT */
        uint32_t r = w32_call_wndproc(w->dlgproc, h, msg, wp, lp);
        if (!dlg_win(h)) return r;
        if (r) {
            if (msg >= 0x132 && msg <= 0x138) return r;
            if (msg == 0x110) return r;
            return direct_result(msg) ? imp2("GetWindowLongA", h, 0) : r;
        }
    }
    switch (msg) {
    case 0x110: return 1;
    case 0x10: dlg_command(h, 2); return 0;                          /* WM_CLOSE: IDCANCEL */
    case 0x400: return w->defid ? (uint32_t)w->defid | 0x534bu << 16 : 0;   /* DM_GETDEFID */
    case 0x401: w->defid = (int)wp; for (int i = 0; i < nwins; i++) wins[i]->dirty = 1; return 1;
    case 0x87: return 0;
    case 0x132: case 0x133: case 0x134: case 0x135: case 0x136: case 0x137: case 0x138: return 0;
    case 0x07: {                                                     /* WM_SETFOCUS: to the first tab stop */
        uint32_t t = next_tab(h, 0, 0);
        if (t) dlg_set_focus(t);
        return 0;
    }
    case 0x111: return 0;
    default: return common(w, h, msg, wp, lp);
    }
}

static uint32_t proc_kind(int kind, const uint32_t* a, int n)
{
    uint32_t h = W(0), msg = W(1), wp = W(2), lp = W(3);
    DlgWin* w = dlg_win(h);
    if (!w && (msg == 0x81 || msg == 0x01)) {
        w = win_new(h, kind);
        if (!w) return msg == 0x81 ? 0 : 0xffffffffu;
        uint32_t name = rt_r32(lp + 0x24);
        if (name >= 0x10000u && rt_r16(name) != 0xffff && rt_r8(name) != 0xff) {
            const char* s = w32_str(name);
            w->text = malloc(strlen(s) + 1);
            strcpy(w->text, s);
        }
        if (kind == K_DIALOG) { w->dlgproc = pending_proc; w->lparam = pending_param; }
        if (kind == K_COMBO || kind == K_LISTBOX) w->sel = -1;
        if (logging()) {
            DlgInfo i;
            dlg_info(h, &i);
            fprintf(stderr, "tm2 host: control %x kind %d id %x style %x '%s'\n", h, kind, i.id, i.style, w->text ? w->text : "");
        }
        overlay_changed = 1;
        return msg == 0x81 ? 1 : 0;
    }
    if (!w) return def_proc(h, msg, wp, lp);
    if (logging() > 1 && msg != 0x87 && (msg < 0x200 || msg > 0x20e || msg != 0x200))
        fprintf(stderr, "tm2 host: msg %x to %x (kind %d) %x %x\n", msg, h, w->kind, wp, lp);
    if (w->kind == K_DIALOG) return dialog_proc(w, h, msg, wp, lp);
    uint32_t res = 0;
    if (ctl_proc(w, msg, wp, lp, &res)) return res;
    return common(w, h, msg, wp, lp);
}

static uint32_t p_button(const uint32_t* a, int n) { return proc_kind(K_BUTTON, a, n); }
static uint32_t p_static(const uint32_t* a, int n) { return proc_kind(K_STATIC, a, n); }
static uint32_t p_edit(const uint32_t* a, int n) { return proc_kind(K_EDIT, a, n); }
static uint32_t p_listbox(const uint32_t* a, int n) { return proc_kind(K_LISTBOX, a, n); }
static uint32_t p_combo(const uint32_t* a, int n) { return proc_kind(K_COMBO, a, n); }
static uint32_t p_trackbar(const uint32_t* a, int n) { return proc_kind(K_TRACKBAR, a, n); }
static uint32_t p_listview(const uint32_t* a, int n) { return proc_kind(K_LISTVIEW, a, n); }
static uint32_t p_scrollbar(const uint32_t* a, int n) { return proc_kind(K_SCROLLBAR, a, n); }
static uint32_t p_dialog(const uint32_t* a, int n) { return proc_kind(K_DIALOG, a, n); }

static struct { const char* name; RtFn fn; uint32_t addr; } classes[] = {
    {"Button", p_button, 0}, {"Static", p_static, 0}, {"Edit", p_edit, 0}, {"ListBox", p_listbox, 0},
    {"ComboBox", p_combo, 0}, {"msctls_trackbar32", p_trackbar, 0}, {"SysListView32", p_listview, 0},
    {"ScrollBar", p_scrollbar, 0}, {"#32770", p_dialog, 0},
};

uint32_t w32dlg_class_proc(const char* cls)
{
    if (!cls) return 0;
    for (size_t i = 0; i < sizeof classes / sizeof classes[0]; i++)
        if (!strcasecmp(classes[i].name, cls)) return classes[i].addr;
    return 0;
}

/* ---- positions, painting, the overlay ---- */

/* the client origin of h in main window client coordinates */
static int abs_pos(uint32_t h, int* x, int* y)
{
    DlgInfo i;
    *x = *y = 0;
    for (int k = 0; k < 16; k++) {
        if (!dlg_info(h, &i)) return 0;
        if (!(i.style & WS_CHILD)) {
            if (dlg_win(h)) { *x += i.x; *y += i.y; }
            return 1;
        }
        if (!(i.style & WS_VISIBLE)) return 0;
        *x += i.x;
        *y += i.y;
        h = i.parent;
    }
    return 0;
}
static int shown(DlgWin* w, DlgInfo* i, int* x, int* y)
{
    if (!dlg_info(w->hwnd, i) || !(i->style & WS_VISIBLE)) return 0;
    if (!abs_pos(w->hwnd, x, y)) return 0;
    uint32_t t = top_of(w->hwnd);
    if (t && t != w->hwnd) {
        DlgInfo ti;
        if (!dlg_info(t, &ti) || !(ti.style & WS_VISIBLE)) return 0;
    }
    return 1;
}
static int nc_top(DlgInfo* i) { return (i->style & WS_CAPTION) == WS_CAPTION ? FRAME + CAPTION : FRAME; }

static void pic_size(DlgPic* p, int w, int h)
{
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (p->w != w || p->h != h || !p->px) {
        free(p->px);
        p->w = w;
        p->h = h;
        p->px = malloc(sizeof(uint32_t) * (size_t)w * (size_t)h);
    }
    memset(p->px, 0, sizeof(uint32_t) * (size_t)w * (size_t)h);
}

static void paint(DlgWin* w, DlgInfo* i)
{
    uint32_t h = w->hwnd;
    if (w->kind == K_DIALOG) {
        int top = nc_top(i);
        pic_size(&w->pic, i->w + 2 * FRAME, i->h + top + FRAME);
        uint32_t dc = gdi_dc_for(&w->pic, 0, 0);
        int W_ = w->pic.w, H_ = w->pic.h;
        gdi_fill(dc, 0, 0, W_, H_, 0xc0c0c0);
        gdi_edge(dc, 0, 0, W_, H_, 0);
        if (top > FRAME) {
            int act = !w->ended;
            gdi_fill(dc, FRAME, FRAME, W_ - FRAME, FRAME + CAPTION - 1, act ? 0x800000 : 0x808080);
            gdi_dc_select_font(dc, w32gdi_CreateFontA((const uint32_t[]){(uint32_t)-11, 0, 0, 0, 700, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 14));
            gdi_dc_reset_colors(dc, 0xffffff, 0x800000, 1);
            int L = FRAME + 4, T = FRAME, R = W_ - FRAME - 20, B = FRAME + CAPTION - 1;
            gdi_draw_text(dc, w->text ? w->text : "", -1, &L, &T, &R, &B, 0x824);
            uint32_t f = gdi_dc_font(dc);
            uint32_t v[1] = {f};
            w32gdi_DeleteObject(v, 1);
            /* the close box */
            int bx = W_ - FRAME - 18, by = FRAME + 2;
            gdi_fill(dc, bx, by, bx + 16, by + 14, 0xc0c0c0);
            gdi_edge(dc, bx, by, bx + 16, by + 14, 0);
            for (int k = 0; k < 6; k++) {
                gdi_fill(dc, bx + 4 + k, by + 4 + k, bx + 6 + k, by + 5 + k, 0);
                gdi_fill(dc, bx + 10 - k, by + 4 + k, bx + 12 - k, by + 5 + k, 0);
            }
        }
        gdi_dc_free(dc);
        /* the client area: WM_CTLCOLORDLG (DefDlgProc's WM_ERASEBKGND) */
        uint32_t cdc = gdi_dc_for(&w->pic, FRAME, top);
        gdi_dc_reset_colors(cdc, 0, 0xc0c0c0, 2);
        uint32_t b = dlg_send(h, 0x136, cdc, h);
        int ok;
        uint32_t c = gdi_brush_color(b, &ok);
        gdi_fill(cdc, 0, 0, i->w, i->h, ok ? c : 0xc0c0c0);
        gdi_dc_free(cdc);
        return;
    }
    int ch = w->kind == K_COMBO ? ctl_hit_height(w, i->h) : i->h;
    if (w->kind == K_COMBO && w->dropped) ch -= ctl_dropdown_height(w);
    pic_size(&w->pic, i->w, ch);
    uint32_t dc = gdi_dc_for(&w->pic, 0, 0);
    if (w->font) gdi_dc_select_font(dc, w->font);
    int od = (w->kind == K_BUTTON && (i->style & 0xf) == 0xb) || (w->kind == K_STATIC && (i->style & 0x1f) == 0xd);
    if (od) {
        uint32_t dis = rt_frame_push(0x30);
        uint32_t st = 0;
        if (w->pushed) st |= 1;
        if (i->style & WS_DISABLED) st |= 4 | 2;
        if (w->check) st |= 8;
        if (dlg_focus() == h) st |= 0x10;
        rt_w32(dis, w->kind == K_BUTTON ? 4 : 3);   /* ODT_BUTTON / ODT_STATIC */
        rt_w32(dis + 4, i->id);
        rt_w32(dis + 8, 0);
        rt_w32(dis + 0xc, 1);                       /* ODA_DRAWENTIRE */
        rt_w32(dis + 0x10, st);
        rt_w32(dis + 0x14, h);
        rt_w32(dis + 0x18, dc);
        rt_w32(dis + 0x1c, 0);
        rt_w32(dis + 0x20, 0);
        rt_w32(dis + 0x24, (uint32_t)i->w);
        rt_w32(dis + 0x28, (uint32_t)i->h);
        rt_w32(dis + 0x2c, 0);
        dlg_send(i->parent, 0x2b, i->id, dis);      /* WM_DRAWITEM */
        rt_frame_pop(0x30);
    } else ctl_paint(w, dc, i->w, ch);
    gdi_dc_free(dc);
}

static DlgPic overlay;
static uint32_t last_present_ms;
static int ov_any;
static int in_tick;

static void blit(DlgPic* src, int x, int y)
{
    for (int j = 0; j < src->h; j++) {
        int oy = y + j;
        if (oy < 0 || oy >= overlay.h) continue;
        for (int i = 0; i < src->w; i++) {
            int ox = x + i;
            if (ox < 0 || ox >= overlay.w) continue;
            uint32_t c = src->px[j * src->w + i];
            if (c >> 24) overlay.px[oy * overlay.w + ox] = c;
        }
    }
}

static int by_order(const void* a, const void* b)
{
    const DlgWin* x = *(DlgWin* const*)a;
    const DlgWin* y = *(DlgWin* const*)b;
    return x->order < y->order ? -1 : x->order > y->order;
}

/* the drawing order: the main window's children, then each dialog followed by its children */
static int z_order(DlgWin** out)
{
    DlgWin* s[NW];
    int n = nwins, k = 0;
    memcpy(s, wins, sizeof(DlgWin*) * (size_t)n);
    qsort(s, (size_t)n, sizeof s[0], by_order);
    for (int i = 0; i < n; i++) if (s[i]->kind != K_DIALOG && top_of(s[i]->hwnd) == 0) out[k++] = s[i];
    for (int i = 0; i < n; i++) {
        if (s[i]->kind != K_DIALOG) continue;
        out[k++] = s[i];
        for (int j = 0; j < n; j++) if (s[j]->kind != K_DIALOG && top_of(s[j]->hwnd) == s[i]->hwnd) out[k++] = s[j];
    }
    return k;
}

static void compose(void)
{
    int w = 640, h = 480;
    dx_sdl_client_size(&w, &h);
    pic_size(&overlay, w, h);
    DlgWin* z[NW];
    int n = z_order(z);
    ov_any = 0;
    for (int k = 0; k < n; k++) {
        DlgInfo i;
        int x, y;
        if (!shown(z[k], &i, &x, &y) || !z[k]->pic.px) continue;
        if (z[k]->kind == K_DIALOG) blit(&z[k]->pic, x - FRAME, y - nc_top(&i));
        else blit(&z[k]->pic, x, y);
        ov_any = 1;
    }
    /* open combo box lists on top of everything */
    for (int k = 0; k < n; k++) {
        DlgInfo i;
        int x, y;
        if (z[k]->kind != K_COMBO || !z[k]->dropped || !shown(z[k], &i, &x, &y)) continue;
        DlgPic p = {0};
        int lh = ctl_dropdown_height(z[k]);
        pic_size(&p, i.w, lh);
        uint32_t dc = gdi_dc_for(&p, 0, 0);
        if (z[k]->font) gdi_dc_select_font(dc, z[k]->font);
        ctl_paint_dropdown(z[k], dc, i.w, lh);
        gdi_dc_free(dc);
        blit(&p, x, y + ctl_hit_height(z[k], i.h) - lh);
        free(p.px);
    }
}

void w32dlg_tick(void)
{
    static uint32_t last;
    if (in_tick || !nwins) return;
    /* at most every 15 ms: the scan asks USER32 about every window */
    uint32_t now = w32_ms();
    if (now - last < 15 && !overlay_changed) return;
    last = now;
    in_tick = 1;
    DlgWin* z[NW];
    int n = z_order(z);
    uint32_t f = dlg_focus();
    for (int k = 0; k < n; k++) {
        DlgWin* w = z[k];
        DlgInfo i;
        int x, y;
        if (!dlg_win(w->hwnd)) continue;
        int vis = shown(w, &i, &x, &y);
        uint32_t sig = vis ? 2166136261u : 1;
        if (vis) {
            uint32_t v[10] = {i.style, (uint32_t)x, (uint32_t)y, (uint32_t)i.w, (uint32_t)i.h, f == w->hwnd, (uint32_t)w->pushed,
                              (uint32_t)w->dropped, (uint32_t)(w->kind == K_EDIT && f == w->hwnd ? w32_ms() / 500 : 0), 0};
            for (int j = 0; j < 10; j++) sig = (sig ^ v[j]) * 16777619u;
        }
        if (sig != w->sig) overlay_changed = 1;
        if (vis && (sig != w->sig || w->dirty)) {
            w->dirty = 0;
            paint(w, &i);
            overlay_changed = 1;
        }
        w->sig = sig;
    }
    /* when the game is not presenting (a modal loop, a screen waiting), present the last frame
     * again with the overlay */
    if (overlay_changed && last_present_ms && w32_ms() - last_present_ms > 60) dx_sdl_redraw();
    DlgWin* m = modal_top();
    if (m) controller_keys(m->hwnd);
    in_tick = 0;
}

/* replaced by dx_sdl.c's when it has one (dialogs.h) */
__attribute__((weak)) void dx_sdl_redraw(void) {}

void w32dlg_overlay(struct SDL_Renderer* r, int presenting)
{
    static SDL_Texture* tex;
    static int tw, th;
    if (presenting) last_present_ms = w32_ms();
    if (!nwins) return;
    if (overlay_changed) {
        compose();
        overlay_changed = 0;
        if (!tex || tw != overlay.w || th != overlay.h) {
            if (tex) SDL_DestroyTexture(tex);
            tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, overlay.w, overlay.h);
            if (!tex) return;
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
            tw = overlay.w;
            th = overlay.h;
        }
        SDL_UpdateTexture(tex, NULL, overlay.px, overlay.w * 4);
    }
    if (tex && ov_any) SDL_RenderCopy(r, tex, NULL, NULL);
}

/* ---- the mouse ---- */

uint32_t w32dlg_mouse_target(uint32_t h, int32_t* x, int32_t* y, int down)
{
    (void)down;
    if (!nwins) return h;
    int gx = *x, gy = *y;
    if (capture && dlg_win(capture)) {
        int ax, ay;
        abs_pos(capture, &ax, &ay);
        *x = gx - ax;
        *y = gy - ay;
        return capture;
    }
    DlgWin* z[NW];
    int n = z_order(z);
    DlgWin* m = modal_top();
    for (int k = n - 1; k >= 0; k--) {
        DlgWin* w = z[k];
        DlgInfo i;
        int ax, ay;
        if (!shown(w, &i, &ax, &ay)) continue;
        int hh = w->kind == K_DIALOG ? i.h : ctl_hit_height(w, i.h);
        int l = ax, t = ay, r = ax + i.w, b = ay + hh;
        if (w->kind == K_DIALOG) { l -= FRAME; t -= nc_top(&i); r += FRAME; b += FRAME; }
        if (gx < l || gy < t || gx >= r || gy >= b) continue;
        if (w->kind == K_STATIC && !(i.style & 0x100)) continue;                         /* not SS_NOTIFY */
        if (w->kind == K_BUTTON && (i.style & 0xf) == 7) continue;                       /* group boxes */
        if (m && w->hwnd != m->hwnd && !is_descendant(w->hwnd, m->hwnd)) return 0;
        if (i.style & WS_DISABLED) return 0;
        *x = gx - ax;
        *y = gy - ay;
        return w->hwnd;
    }
    return m ? 0 : h;
}

/* ---- painting imports ---- */

/* GetDC(hwnd): a DC on a control's picture, or one that only measures */
uint32_t w32dlg_GetDC(const uint32_t* a, int n)
{
    DlgWin* w = dlg_win(W(0));
    if (w && w->pic.px) return gdi_dc_for(&w->pic, w->kind == K_DIALOG ? FRAME : 0, 0);
    return gdi_dc_for(NULL, 0, 0);
}
uint32_t w32dlg_ReleaseDC(const uint32_t* a, int n)
{
    if (gdi_is_dc(W(1))) gdi_dc_free(W(1));
    else if (w32_handle_type(W(1)) == H_GDI) w32_handle_free(W(1));
    overlay_changed = 1;
    return 1;
}
/* BeginPaint(hwnd, &PAINTSTRUCT {hdc, erase, rect, ...}) */
uint32_t w32dlg_BeginPaint(const uint32_t* a, int n)
{
    DlgInfo i;
    uint32_t dc = w32dlg_GetDC(a, n);
    dlg_info(W(0), &i);
    rt_w32(W(1), dc);
    rt_w32(W(1) + 4, 0);
    rt_w32(W(1) + 8, 0);
    rt_w32(W(1) + 12, 0);
    rt_w32(W(1) + 16, (uint32_t)i.w);
    rt_w32(W(1) + 20, (uint32_t)i.h);
    return dc;
}
uint32_t w32dlg_EndPaint(const uint32_t* a, int n)
{
    uint32_t dc = rt_r32(W(1));
    uint32_t v[2] = {W(0), dc};
    return w32dlg_ReleaseDC(v, 2);
}

static void rt_bind_host_dialogs(void)
{
    for (size_t i = 0; i < sizeof classes / sizeof classes[0]; i++) {
        classes[i].addr = rt_fake_alloc(16);
        char* nm = malloc(64);
        snprintf(nm, 64, "%s window procedure", classes[i].name);
        rt_bind(classes[i].addr, classes[i].fn, nm);
    }
}

RT_BIND_AREA(host_dialogs)
