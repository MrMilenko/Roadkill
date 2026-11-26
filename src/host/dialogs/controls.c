/*
 * controls.c: the standard controls' behaviour and looks (dialogs.h), port code.
 *
 * Button (push, default push, check box, 3 state, radio, group box, owner draw), Static (text,
 * rectangles, frames, etched lines), Edit (single line), ListBox, ComboBox (drop down list),
 * msctls_trackbar32, SysListView32 (list / report, text only) and ScrollBar (looks only).
 * Messages and notifications follow Win32: BM_*, STM_*, EM_*, LB_*, CB_*, TBM_*, LVM_* with
 * BN_CLICKED, EN_CHANGE, LBN_SELCHANGE / LBN_DBLCLK, CBN_SELCHANGE / DROPDOWN / CLOSEUP,
 * WM_HSCROLL / WM_VSCROLL (TB_*), WM_NOTIFY (NM_CLICK, LVN_ITEMCHANGED). The looks are the
 * Windows 95 ones, simplified; colours come from WM_CTLCOLOR* answered by the parent.
 */
#include "host/dialogs/dialogs.h"
#include "host/win32_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define LO(x) ((x) & 0xffffu)
#define HI(x) ((x) >> 16)
#define FACE 0xc0c0c0u
#define WS_DISABLED 0x08000000u

static uint32_t style_of(DlgWin* w) { DlgInfo i; return dlg_info(w->hwnd, &i) ? i.style : 0; }
static uint32_t exstyle_of(DlgWin* w) { DlgInfo i; return dlg_info(w->hwnd, &i) ? i.exstyle : 0; }
static char* dupn(const char* s) { size_t n = strlen(s); char* d = malloc(n + 1); memcpy(d, s, n + 1); return d; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int line_h(DlgWin* w) { return gdi_font_height(w->font ? w->font : gdi_default_font()); }

/* ---- items (list box, combo box, list view) ---- */

static void items_free_one(DlgItem* it)
{
    free(it->s);
    for (int k = 0; k < 4; k++) free(it->sub[k]);
}
static int items_insert(DlgWin* w, int at, const char* s, int sort)
{
    if (w->nitems == w->cap) {
        w->cap = w->cap ? w->cap * 2 : 16;
        w->items = realloc(w->items, sizeof(DlgItem) * (size_t)w->cap);
    }
    if (sort) for (at = 0; at < w->nitems && strcasecmp(w->items[at].s, s) <= 0; at++) {}
    if (at < 0 || at > w->nitems) at = w->nitems;
    memmove(w->items + at + 1, w->items + at, sizeof(DlgItem) * (size_t)(w->nitems - at));
    memset(&w->items[at], 0, sizeof(DlgItem));
    w->items[at].s = dupn(s);
    w->nitems++;
    if (w->sel >= at && w->sel >= 0) w->sel++;
    w->dirty = 1;
    return at;
}
static int items_delete(DlgWin* w, int at)
{
    if (at < 0 || at >= w->nitems) return -1;
    items_free_one(&w->items[at]);
    memmove(w->items + at, w->items + at + 1, sizeof(DlgItem) * (size_t)(w->nitems - at - 1));
    w->nitems--;
    if (w->sel == at) w->sel = -1;
    else if (w->sel > at) w->sel--;
    w->dirty = 1;
    return w->nitems;
}
static void items_clear(DlgWin* w)
{
    for (int i = 0; i < w->nitems; i++) items_free_one(&w->items[i]);
    w->nitems = 0;
    w->sel = -1;
    w->top = 0;
    w->dirty = 1;
}
static int items_find(DlgWin* w, int start, const char* s, int exact)
{
    size_t l = strlen(s);
    for (int k = 1; k <= w->nitems; k++) {
        int i = (start + k) % w->nitems;
        if (start < 0) i = k - 1;
        if (exact ? !strcasecmp(w->items[i].s, s) : !strncasecmp(w->items[i].s, s, l)) return i;
    }
    return -1;
}

void ctl_free(DlgWin* w)
{
    items_clear(w);
    free(w->items);
    w->items = NULL;
}

/* ---- notifications ---- */

static void scroll_notify(DlgWin* w, uint32_t code)
{
    DlgInfo i;
    if (!dlg_info(w->hwnd, &i)) return;
    uint32_t msg = (i.style & 2) ? 0x115u : 0x114u;   /* TBS_VERT: WM_VSCROLL */
    uint32_t wp = code;
    if (code == 4 || code == 5) wp |= (uint32_t)(w->tpos & 0xffff) << 16;
    dlg_send(i.parent, msg, wp, w->hwnd);
}

/* ---- buttons ---- */

static int btype(DlgWin* w) { return (int)(style_of(w) & 0xf); }
static int is_push(int t) { return t == 0 || t == 1 || t == 0xb || t == 8; }

static void button_click(DlgWin* w)
{
    int t = btype(w);
    if (t == 3) w->check = !w->check;
    else if (t == 6) w->check = (w->check + 1) % 3;
    else if (t == 9 && !w->check) {
        /* an auto radio button: the others of its group are cleared */
        dlg_radio_group(w->hwnd);
        w->check = 1;
    }
    w->dirty = 1;
    dlg_notify(w->hwnd, 0);   /* BN_CLICKED */
}

static int button_proc(DlgWin* w, uint32_t msg, uint32_t wp, uint32_t lp, uint32_t* res)
{
    int t = btype(w);
    switch (msg) {
    case 0xf0: *res = (uint32_t)w->check; return 1;                         /* BM_GETCHECK */
    case 0xf1: w->check = (int)wp; w->dirty = 1; *res = 0; return 1;        /* BM_SETCHECK */
    case 0xf2: *res = (uint32_t)w->check | (w->pushed ? 4u : 0) | (dlg_focus() == w->hwnd ? 8u : 0); return 1;
    case 0xf3: w->pushed = wp != 0; w->dirty = 1; *res = 0; return 1;      /* BM_SETSTATE */
    case 0xf4: {                                                            /* BM_SETSTYLE */
        uint32_t v[3] = {w->hwnd, (uint32_t)-16, (style_of(w) & ~0xffffu) | LO(wp)};
        dlg_import("SetWindowLongA", v, 3);
        w->dirty = 1;
        *res = 0;
        return 1;
    }
    case 0xf5: button_click(w); *res = 0; return 1;                         /* BM_CLICK */
    case 0x201: case 0x203:                                                 /* WM_LBUTTONDOWN / DBLCLK */
        if (t == 7) break;
        dlg_set_focus(w->hwnd);
        dlg_capture(w->hwnd);
        w->tracking = w->pushed = 1;
        w->dirty = 1;
        *res = 0;
        return 1;
    case 0x200:                                                             /* WM_MOUSEMOVE */
        if (w->tracking) {
            DlgInfo i;
            int x = (int16_t)LO(lp), y = (int16_t)HI(lp);
            int in = dlg_info(w->hwnd, &i) && x >= 0 && y >= 0 && x < i.w && y < i.h;
            if (in != w->pushed) { w->pushed = in; w->dirty = 1; }
        }
        *res = 0;
        return 1;
    case 0x202:                                                             /* WM_LBUTTONUP */
        if (w->tracking) {
            int was = w->pushed;
            w->tracking = w->pushed = 0;
            dlg_capture(0);
            w->dirty = 1;
            if (was) button_click(w);
        }
        *res = 0;
        return 1;
    case 0x100:                                                             /* WM_KEYDOWN */
        if (wp == 0x20 && t != 7) { w->pushed = 1; w->dirty = 1; }
        break;
    case 0x101:
        if (wp == 0x20 && w->pushed) { w->pushed = 0; w->dirty = 1; button_click(w); }
        break;
    case 0x08:                                                              /* WM_KILLFOCUS */
        if (w->pushed && !w->tracking) w->pushed = 0;
        break;
    default: break;
    }
    return 0;
}

static void draw_check(uint32_t hdc, int x, int y, int st, int radio, int disabled)
{
    if (radio) {
        /* a 12 x 12 circle */
        static const char* ring[12] = {"    1111    ", "  11222211  ", " 1222222221 ", " 12      23 ", "12        34",
                                      "12        34", "12        34", "12        34", " 1        43 ", " 34      43 ",
                                      "  33444433  ", "    3333    "};
        for (int j = 0; j < 12; j++)
            for (int i = 0; i < 12; i++) {
                char c = ring[j][i];
                uint32_t col = c == '1' ? 0x808080 : c == '2' ? 0x000000 : c == '3' ? 0xffffff : c == '4' ? FACE : 0xffffffu;
                if (c == ' ') {
                    int dx = 2 * i - 11, dy = 2 * j - 11;
                    if (dx * dx + dy * dy > 100) continue;
                    col = disabled ? FACE : 0xffffff;
                }
                gdi_fill(hdc, x + i, y + j, x + i + 1, y + j + 1, col);
            }
        if (st) gdi_fill(hdc, x + 4, y + 4, x + 8, y + 8, disabled ? 0x808080 : 0);
        return;
    }
    gdi_fill(hdc, x, y, x + 13, y + 13, disabled ? FACE : 0xffffff);
    gdi_edge(hdc, x, y, x + 13, y + 13, 1);
    if (st) {
        uint32_t c = st == 2 || disabled ? 0x808080 : 0;
        for (int k = 0; k < 7; k++) {
            int cx = x + 3 + k, cy = k < 2 ? y + 5 + k : y + 7 - (k - 2) + 1;
            if (k >= 2) cy = y + 7 - (k - 2);
            gdi_fill(hdc, cx, cy, cx + 1, cy + 3, c);
        }
    }
}

static void text_in(DlgWin* w, uint32_t hdc, int l, int t, int r, int b, uint32_t fmt, int disabled)
{
    if (!w->text || !*w->text) return;
    gdi_dc_select_font(hdc, w->font ? w->font : gdi_default_font());
    int L = l, T = t, R = r, B = b;
    if (disabled) {
        uint32_t c = gdi_dc_text_color(hdc), bk = gdi_dc_bk_color(hdc);
        int L1 = L + 1, T1 = T + 1, R1 = R + 1, B1 = B + 1;
        gdi_dc_reset_colors(hdc, 0xffffff, bk, 1);
        gdi_draw_text(hdc, w->text, -1, &L1, &T1, &R1, &B1, fmt);
        gdi_dc_reset_colors(hdc, 0x808080, bk, 1);
        gdi_draw_text(hdc, w->text, -1, &L, &T, &R, &B, fmt);
        gdi_dc_reset_colors(hdc, c, bk, 1);
        return;
    }
    gdi_draw_text(hdc, w->text, -1, &L, &T, &R, &B, fmt);
}

static void focus_rect(uint32_t hdc, int l, int t, int r, int b)
{
    for (int x = l; x < r; x += 2) { gdi_fill(hdc, x, t, x + 1, t + 1, 0); gdi_fill(hdc, x, b - 1, x + 1, b, 0); }
    for (int y = t; y < b; y += 2) { gdi_fill(hdc, l, y, l + 1, y + 1, 0); gdi_fill(hdc, r - 1, y, r, y + 1, 0); }
}


static void button_paint(DlgWin* w, uint32_t hdc, int cw, int ch)
{
    uint32_t st = style_of(w);
    int t = (int)(st & 0xf), dis = (st & WS_DISABLED) != 0, foc = dlg_focus() == w->hwnd;
    uint32_t fill;
    if (t == 0 || t == 1 || t == 8) {
        dlg_ctlcolor(w->hwnd, 0x135, hdc, FACE, &fill);   /* WM_CTLCOLORBTN */
        gdi_dc_reset_colors(hdc, 0, FACE, 1);
        int def = dlg_is_default_button(w->hwnd);
        int l = 0, tp = 0, r = cw, b = ch;
        gdi_fill(hdc, 0, 0, cw, ch, FACE);
        if (def) { gdi_frame(hdc, 0, 0, cw, ch, 0); l = tp = 1; r--; b--; }
        if (w->pushed) {
            gdi_frame(hdc, l, tp, r, b, 0x808080);
        } else gdi_edge(hdc, l, tp, r, b, 0);
        int o = w->pushed ? 1 : 0;
        text_in(w, hdc, l + 2 + o, tp + 2 + o, r - 2 + o, b - 2 + o, 0x25, dis);
        if (foc) focus_rect(hdc, l + 3, tp + 3, r - 3, b - 3);
        return;
    }
    dlg_ctlcolor(w->hwnd, 0x138, hdc, FACE, &fill);   /* WM_CTLCOLORSTATIC */
    if (t == 7) {   /* group box: etched frame below the text's middle */
        int lh = line_h(w);
        gdi_fill(hdc, 0, 0, cw, ch, fill);
        int y = lh / 2;
        gdi_frame(hdc, 0, y, cw - 1, ch - 1, 0x808080);
        gdi_frame(hdc, 1, y + 1, cw, ch, 0xffffff);
        if (w->text && *w->text) {
            int tw = gdi_text_width(w->font ? w->font : gdi_default_font(), w->text, (int)strlen(w->text));
            gdi_fill(hdc, 7, 0, 7 + tw + 4, lh, fill);
            text_in(w, hdc, 9, 0, cw - 2, lh, 0x20, dis);
        }
        return;
    }
    gdi_fill(hdc, 0, 0, cw, ch, fill);
    int radio = t == 4 || t == 9;
    int box = radio ? 12 : 13, by = (ch - box) / 2;
    int left = (st & 0x20) != 0;   /* BS_LEFTTEXT */
    int bx = left ? cw - box : 0;
    draw_check(hdc, bx, by, w->check, radio, dis);
    int tl = left ? 0 : box + 4, tr = left ? cw - box - 4 : cw;
    text_in(w, hdc, tl, 0, tr, ch, 0x24, dis);
    if (foc && w->text && *w->text) {
        int tw = gdi_text_width(w->font ? w->font : gdi_default_font(), w->text, (int)strlen(w->text));
        int lh = line_h(w);
        focus_rect(hdc, tl - 1, (ch - lh) / 2, tl + tw + 2, (ch + lh) / 2);
    }
}

/* ---- statics ---- */

static void static_paint(DlgWin* w, uint32_t hdc, int cw, int ch)
{
    uint32_t st = style_of(w), ex = exstyle_of(w);
    uint32_t fill;
    dlg_ctlcolor(w->hwnd, 0x138, hdc, FACE, &fill);
    int t = (int)(st & 0x1f);
    int l = 0, tp = 0, r = cw, b = ch;
    if (t >= 4 && t <= 9) {
        static const uint32_t c[3] = {0x000000, 0x808080, 0xffffff};
        if (t <= 6) gdi_fill(hdc, 0, 0, cw, ch, c[t - 4]);
        else gdi_frame(hdc, 0, 0, cw, ch, c[t - 7]);
        return;
    }
    if (t == 0x10) { gdi_line_h(hdc, 0, cw, 0, 0x808080); gdi_line_h(hdc, 0, cw, 1, 0xffffff); return; }
    if (t == 0x11) { gdi_line_v(hdc, 0, 0, ch, 0x808080); gdi_line_v(hdc, 1, 0, ch, 0xffffff); return; }
    if (t == 0x12) { gdi_frame(hdc, 0, 0, cw - 1, ch - 1, 0x808080); gdi_frame(hdc, 1, 1, cw, ch, 0xffffff); return; }
    if (t == 3 || t == 0xe || t == 0xd) return;   /* icons, bitmaps, owner draw: nothing */
    gdi_fill(hdc, 0, 0, cw, ch, fill);
    if (st & 0x00800000u) { gdi_frame(hdc, 0, 0, cw, ch, 0); l++; tp++; r--; b--; }   /* WS_BORDER */
    if ((st & 0x1000) || (ex & 0x20000)) {                                           /* SS_SUNKEN, static edge */
        gdi_line_h(hdc, l, r, tp, 0x808080);
        gdi_line_v(hdc, l, tp, b, 0x808080);
        gdi_line_h(hdc, l, r, b - 1, 0xffffff);
        gdi_line_v(hdc, r - 1, tp, b, 0xffffff);
        l++; tp++; r--; b--;
    }
    if (ex & 0x200) { gdi_edge(hdc, l, tp, r, b, 1); l += 2; tp += 2; r -= 2; b -= 2; }   /* client edge */
    uint32_t fmt = t == 1 ? 0x11 : t == 2 ? 0x12 : (t == 0xb || t == 0xc) ? 0 : 0x10;
    if (st & 0x80) fmt |= 0x800;
    if (st & 0x200) fmt = (fmt & ~0x10u) | 0x24;   /* SS_CENTERIMAGE */
    text_in(w, hdc, l, tp, r, b, fmt, (st & WS_DISABLED) != 0);
}

/* ---- edit ---- */

static void edit_set(DlgWin* w, char* s)
{
    free(w->text);
    w->text = s;
    w->dirty = 1;
}
static void edit_insert(DlgWin* w, const char* ins)
{
    const char* t = w->text ? w->text : "";
    int len = (int)strlen(t), a = clampi(w->anchor, 0, len), c = clampi(w->caret, 0, len);
    int lo = a < c ? a : c, hi = a < c ? c : a;
    int il = (int)strlen(ins);
    int lim = w->limit > 0 ? w->limit : 30000;
    if (len - (hi - lo) + il > lim) il = lim - (len - (hi - lo));
    if (il < 0) il = 0;
    char* n = malloc((size_t)(len - (hi - lo) + il + 1));
    memcpy(n, t, (size_t)lo);
    memcpy(n + lo, ins, (size_t)il);
    memcpy(n + lo + il, t + hi, (size_t)(len - hi + 1));
    edit_set(w, n);
    w->caret = w->anchor = lo + il;
}

static int edit_proc(DlgWin* w, uint32_t msg, uint32_t wp, uint32_t lp, uint32_t* res)
{
    int len = w->text ? (int)strlen(w->text) : 0;
    uint32_t st = style_of(w);
    switch (msg) {
    case 0xb0:                                                     /* EM_GETSEL */
        if (wp) rt_w32(wp, (uint32_t)(w->anchor < w->caret ? w->anchor : w->caret));
        if (lp) rt_w32(lp, (uint32_t)(w->anchor < w->caret ? w->caret : w->anchor));
        *res = (uint32_t)(w->anchor < w->caret ? w->anchor : w->caret) | (uint32_t)(w->anchor < w->caret ? w->caret : w->anchor) << 16;
        return 1;
    case 0xb1:                                                     /* EM_SETSEL */
        w->anchor = (int32_t)wp < 0 ? len : clampi((int)wp, 0, len);
        w->caret = (int32_t)lp < 0 ? len : clampi((int)lp, 0, len);
        if ((int32_t)wp < 0) w->anchor = w->caret;
        w->dirty = 1;
        *res = 1;
        return 1;
    case 0xc5: w->limit = wp ? (int)wp : 0x7ffffffe; *res = 0; return 1;   /* EM_LIMITTEXT */
    case 0xd5: *res = (uint32_t)(w->limit > 0 ? w->limit : 30000); return 1; /* EM_GETLIMITTEXT */
    case 0xc2:                                                     /* EM_REPLACESEL */
        edit_insert(w, lp ? w32_str(lp) : "");
        *res = 0;
        return 1;
    case 0xba: *res = 1; return 1;                                 /* EM_GETLINECOUNT */
    case 0xcf: {                                                   /* EM_SETREADONLY */
        uint32_t v[3] = {w->hwnd, (uint32_t)-16, wp ? st | 0x800 : st & ~0x800u};
        dlg_import("SetWindowLongA", v, 3);
        *res = 1;
        return 1;
    }
    case 0x0c:                                                     /* WM_SETTEXT */
        edit_set(w, dupn(lp ? w32_str(lp) : ""));
        w->caret = w->anchor = 0;
        *res = 1;
        return 1;
    case 0x07:
        w->anchor = 0;
        w->caret = len;
        dlg_notify(w->hwnd, 0x100);                                /* EN_SETFOCUS */
        break;
    case 0x08: w->anchor = w->caret; dlg_notify(w->hwnd, 0x200); break;
    case 0x201: case 0x203: {
        dlg_set_focus(w->hwnd);
        int x = (int16_t)LO(lp) - 2, pos = 0;
        while (pos < len && gdi_text_width(w->font ? w->font : gdi_default_font(), w->text, pos + 1) <= x) pos++;
        w->caret = w->anchor = pos;
        w->dirty = 1;
        *res = 0;
        return 1;
    }
    case 0x102: {                                                  /* WM_CHAR */
        if (st & 0x800) { *res = 0; return 1; }                    /* ES_READONLY */
        char c = (char)wp;
        if (c == 8) {
            if (w->anchor == w->caret && w->caret > 0) w->anchor = w->caret - 1;
            if (w->anchor != w->caret) edit_insert(w, "");
        } else if ((unsigned char)c >= 32) {
            if ((st & 0x2000) && (c < '0' || c > '9')) { *res = 0; return 1; }   /* ES_NUMBER */
            if (st & 8) c = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
            if (st & 0x10) c = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
            char s[2] = {c, 0};
            edit_insert(w, s);
        } else { *res = 0; return 1; }
        dlg_notify(w->hwnd, 0x400);                                /* EN_UPDATE */
        dlg_notify(w->hwnd, 0x300);                                /* EN_CHANGE */
        *res = 0;
        return 1;
    }
    case 0x100:                                                    /* WM_KEYDOWN */
        switch (wp) {
        case 0x25: if (w->caret > 0) w->caret--; break;
        case 0x27: if (w->caret < len) w->caret++; break;
        case 0x24: w->caret = 0; break;
        case 0x23: w->caret = len; break;
        case 0x2e:
            if (!(st & 0x800)) {
                if (w->anchor == w->caret && w->caret < len) w->anchor = w->caret + 1;
                if (w->anchor != w->caret) { edit_insert(w, ""); dlg_notify(w->hwnd, 0x300); }
            }
            *res = 0;
            return 1;
        default: return 0;
        }
        w->anchor = w->caret;
        w->dirty = 1;
        *res = 0;
        return 1;
    default: break;
    }
    return 0;
}

static void edit_paint(DlgWin* w, uint32_t hdc, int cw, int ch)
{
    uint32_t st = style_of(w), ex = exstyle_of(w);
    int ro = (st & 0x800) || (st & WS_DISABLED);
    uint32_t fill;
    dlg_ctlcolor(w->hwnd, ro ? 0x138 : 0x133, hdc, ro ? FACE : 0xffffff, &fill);
    gdi_fill(hdc, 0, 0, cw, ch, fill);
    int l = 0, t = 0, r = cw, b = ch;
    if (ex & 0x200) { gdi_edge(hdc, 0, 0, cw, ch, 1); l = t = 2; r -= 2; b -= 2; }
    else if (st & 0x00800000u) { gdi_frame(hdc, 0, 0, cw, ch, 0); l = t = 1; r--; b--; }
    const char* s = w->text ? w->text : "";
    int len = (int)strlen(s);
    char* shown = dupn(s);
    if (st & 0x20) memset(shown, '*', (size_t)len);   /* ES_PASSWORD */
    uint32_t font = w->font ? w->font : gdi_default_font();
    int cx = gdi_text_width(font, shown, clampi(w->caret, 0, len));
    int off = cx > r - l - 4 ? cx - (r - l - 4) : 0;
    int lh = line_h(w);
    int ty = (st & 4) ? t + 1 : t + (b - t - lh) / 2;   /* ES_MULTILINE at the top */
    gdi_dc_select_font(hdc, font);
    gdi_dc_reset_colors(hdc, (st & WS_DISABLED) ? 0x808080 : gdi_dc_text_color(hdc), fill, 1);
    int L = l + 2 - off, T = ty, R = r + 4096, B = ty + lh;
    int clip_l = l, clip_r = r;
    (void)clip_l; (void)clip_r;
    gdi_draw_text(hdc, shown, len, &L, &T, &R, &B, 0x920);
    /* the selection, inverted colours */
    int a = clampi(w->anchor, 0, len), c = clampi(w->caret, 0, len);
    if (a != c && dlg_focus() == w->hwnd) {
        int lo = a < c ? a : c, hi = a < c ? c : a;
        int x0 = l + 2 - off + gdi_text_width(font, shown, lo), x1 = l + 2 - off + gdi_text_width(font, shown, hi);
        gdi_fill(hdc, x0, ty, x1, ty + lh, 0x800000);
        gdi_dc_reset_colors(hdc, 0xffffff, 0x800000, 1);
        int L2 = x0, T2 = ty, R2 = x1 + 4096, B2 = ty + lh;
        gdi_draw_text(hdc, shown + lo, hi - lo, &L2, &T2, &R2, &B2, 0x920);
    }
    if (dlg_focus() == w->hwnd && (w32_ms() / 500) % 2 == 0) gdi_fill(hdc, l + 2 - off + cx, ty, l + 3 - off + cx, ty + lh, 0);
    free(shown);
}

/* ---- list box and the combo box's list ---- */

static void list_select(DlgWin* w, int i, int notify_code)
{
    if (w->nitems == 0) return;
    i = clampi(i, 0, w->nitems - 1);
    if (i == w->sel) return;
    w->sel = i;
    w->dirty = 1;
    if (notify_code >= 0) dlg_notify(w->hwnd, (uint32_t)notify_code);
}
static int list_rows(DlgWin* w, int h) { int lh = line_h(w); return h / lh > 0 ? h / lh : 1; }
static void list_show(DlgWin* w, int rows)
{
    if (w->sel < 0) return;
    if (w->sel < w->top) w->top = w->sel;
    if (w->sel >= w->top + rows) w->top = w->sel - rows + 1;
    if (w->top < 0) w->top = 0;
}

static void list_draw(DlgWin* w, uint32_t hdc, int l, int t, int r, int b, uint32_t bg, int hot, int focus)
{
    int lh = line_h(w);
    uint32_t text = gdi_dc_text_color(hdc);
    gdi_dc_select_font(hdc, w->font ? w->font : gdi_default_font());
    for (int i = w->top, y = t; i < w->nitems && y < b; i++, y += lh) {
        int on = hot >= 0 ? i == hot : i == w->sel;
        uint32_t ibg = on ? 0x800000 : bg;
        gdi_fill(hdc, l, y, r, y + lh < b ? y + lh : b, ibg);
        gdi_dc_reset_colors(hdc, on ? 0xffffff : text, ibg, 1);
        int L = l + 2, T = y, R = r - 1, B = y + lh < b ? y + lh : b;
        gdi_draw_text(hdc, w->items[i].s, -1, &L, &T, &R, &B, 0x824);
        if (on && focus) focus_rect(hdc, l, y, r, y + lh);
    }
    gdi_dc_reset_colors(hdc, text, bg, 1);
}

static void vscroll_draw(uint32_t hdc, int x, int t, int b, int top, int rows, int n)
{
    gdi_fill(hdc, x, t, x + 16, b, 0xe0e0e0);
    gdi_fill(hdc, x, t, x + 16, t + 16, FACE);
    gdi_edge(hdc, x, t, x + 16, t + 16, 0);
    gdi_fill(hdc, x, b - 16, x + 16, b, FACE);
    gdi_edge(hdc, x, b - 16, x + 16, b, 0);
    for (int k = 0; k < 4; k++) {
        gdi_line_h(hdc, x + 7 - k, x + 8 + k, t + 6 + k, 0);
        gdi_line_h(hdc, x + 7 - k, x + 8 + k, b - 7 - k, 0);
    }
    int track = b - t - 32;
    if (n > rows && track > 8) {
        int th = track * rows / n;
        if (th < 8) th = 8;
        int ty = t + 16 + (track - th) * top / (n - rows);
        gdi_fill(hdc, x, ty, x + 16, ty + th, FACE);
        gdi_edge(hdc, x, ty, x + 16, ty + th, 0);
    }
}

static int listbox_proc(DlgWin* w, uint32_t msg, uint32_t wp, uint32_t lp, uint32_t* res)
{
    uint32_t st = style_of(w);
    int notify = (st & 1) ? 1 : -1;
    DlgInfo i;
    int rows = dlg_info(w->hwnd, &i) ? list_rows(w, i.h - 2) : 1;
    switch (msg) {
    case 0x180: *res = (uint32_t)items_insert(w, w->nitems, lp ? (st & 0x40) || !(st & 0x30) ? w32_str(lp) : "" : "", (st & 2) != 0); return 1;
    case 0x181: *res = (uint32_t)items_insert(w, (int32_t)wp < 0 ? w->nitems : (int)wp, lp ? w32_str(lp) : "", 0); return 1;
    case 0x182: *res = (uint32_t)items_delete(w, (int)wp); return 1;
    case 0x184: items_clear(w); *res = 0; return 1;
    case 0x185: if ((int32_t)lp >= -1 && wp) w->sel = (int32_t)lp; w->dirty = 1; *res = 0; return 1;   /* LB_SETSEL */
    case 0x186:                                                    /* LB_SETCURSEL */
        if ((int32_t)wp < 0 || (int)wp >= w->nitems) { w->sel = -1; w->dirty = 1; *res = 0xffffffffu; return 1; }
        w->sel = (int)wp; list_show(w, rows); w->dirty = 1; *res = wp; return 1;
    case 0x187: *res = (int)wp == w->sel; return 1;                /* LB_GETSEL */
    case 0x188: *res = (uint32_t)w->sel; return 1;                 /* LB_GETCURSEL */
    case 0x189:                                                    /* LB_GETTEXT */
        if ((int)wp >= w->nitems) { *res = 0xffffffffu; return 1; }
        w32_put_str(lp, w->items[wp].s, 0);
        *res = (uint32_t)strlen(w->items[wp].s);
        return 1;
    case 0x18a: *res = (int)wp < w->nitems ? (uint32_t)strlen(w->items[wp].s) : 0xffffffffu; return 1;
    case 0x18b: *res = (uint32_t)w->nitems; return 1;              /* LB_GETCOUNT */
    case 0x18c: {                                                  /* LB_SELECTSTRING */
        int k = items_find(w, (int32_t)wp, lp ? w32_str(lp) : "", 0);
        if (k >= 0) { w->sel = k; list_show(w, rows); w->dirty = 1; }
        *res = (uint32_t)k;
        return 1;
    }
    case 0x18e: *res = (uint32_t)w->top; return 1;
    case 0x18f: *res = (uint32_t)items_find(w, (int32_t)wp, lp ? w32_str(lp) : "", 0); return 1;
    case 0x1a2: *res = (uint32_t)items_find(w, (int32_t)wp, lp ? w32_str(lp) : "", 1); return 1;
    case 0x197: w->top = clampi((int)wp, 0, w->nitems > 0 ? w->nitems - 1 : 0); w->dirty = 1; *res = 0; return 1;
    case 0x199: *res = (int)wp < w->nitems ? w->items[wp].data : 0xffffffffu; return 1;
    case 0x19a: if ((int)wp < w->nitems) w->items[wp].data = lp; *res = 0; return 1;
    case 0x1a1: *res = (uint32_t)line_h(w); return 1;              /* LB_GETITEMHEIGHT */
    case 0x198: {                                                  /* LB_GETITEMRECT */
        int lh = line_h(w), y = ((int)wp - w->top) * lh + 1;
        rt_w32(lp, 1); rt_w32(lp + 4, (uint32_t)y); rt_w32(lp + 8, (uint32_t)(i.w - 1)); rt_w32(lp + 12, (uint32_t)(y + lh));
        *res = 1;
        return 1;
    }
    case 0x100: {
        int s = w->sel;
        switch (wp) {
        case 0x26: case 0x25: s = s < 0 ? 0 : s - 1; break;
        case 0x28: case 0x27: s = s + 1; break;
        case 0x21: s -= rows; break;
        case 0x22: s = s < 0 ? rows : s + rows; break;
        case 0x24: s = 0; break;
        case 0x23: s = w->nitems - 1; break;
        default: return 0;
        }
        list_select(w, s, notify);
        list_show(w, rows);
        *res = 0;
        return 1;
    }
    case 0x201: case 0x203: {
        dlg_set_focus(w->hwnd);
        int x = (int16_t)LO(lp), y = (int16_t)HI(lp) - 1;
        if ((st & 0x00200000u) && x >= i.w - 17 && w->nitems > rows) {   /* the scroll bar */
            if (y < 16) w->top--;
            else if (y > i.h - 18) w->top++;
            else w->top += y < i.h / 2 ? -rows : rows;
            w->top = clampi(w->top, 0, w->nitems - rows);
            w->dirty = 1;
        } else {
            int k = w->top + y / line_h(w);
            if (k < w->nitems) {
                list_select(w, k, notify);
                if (msg == 0x203 && notify > 0) dlg_notify(w->hwnd, 2);   /* LBN_DBLCLK */
            }
        }
        *res = 0;
        return 1;
    }
    default: return 0;
    }
}

static void listbox_paint(DlgWin* w, uint32_t hdc, int cw, int ch)
{
    uint32_t st = style_of(w), ex = exstyle_of(w);
    uint32_t fill;
    dlg_ctlcolor(w->hwnd, 0x134, hdc, 0xffffff, &fill);   /* WM_CTLCOLORLISTBOX */
    gdi_fill(hdc, 0, 0, cw, ch, fill);
    int l = 0, t = 0, r = cw, b = ch;
    if (ex & 0x200) { gdi_edge(hdc, 0, 0, cw, ch, 1); l = t = 2; r -= 2; b -= 2; }
    else if (st & 0x00800000u) { gdi_frame(hdc, 0, 0, cw, ch, 0); l = t = 1; r--; b--; }
    int rows = list_rows(w, b - t);
    if ((st & 0x00200000u) && (w->nitems > rows || (st & 0x1000))) {
        vscroll_draw(hdc, r - 16, t, b, w->top, rows, w->nitems);
        r -= 16;
    }
    list_draw(w, hdc, l, t, r, b, fill, -1, dlg_focus() == w->hwnd);
}

/* ---- combo box ---- */

static int combo_closed_h(DlgWin* w) { return line_h(w) + 8; }
int ctl_hit_height(DlgWin* w, int h)
{
    if (w->kind == K_COMBO && (style_of(w) & 3) != 1) return w->dropped ? combo_closed_h(w) + ctl_dropdown_height(w) : combo_closed_h(w);
    return h;
}
int ctl_dropdown_height(DlgWin* w)
{
    DlgInfo i;
    if (!dlg_info(w->hwnd, &i)) return 0;
    int lh = line_h(w), avail = i.h - combo_closed_h(w);
    int n = w->nitems > 0 ? w->nitems : 1;
    if (avail < lh) avail = lh * 8;
    int rows = avail / lh;
    if (rows > n) rows = n;
    return rows * lh + 2;
}

static void combo_close(DlgWin* w, int ok)
{
    if (!w->dropped) return;
    w->dropped = 0;
    dlg_capture(0);
    w->dirty = 1;
    if (ok && w->hot >= 0 && w->hot != w->sel) {
        w->sel = w->hot;
        dlg_notify(w->hwnd, 9);    /* CBN_SELENDOK */
        dlg_notify(w->hwnd, 1);    /* CBN_SELCHANGE */
    } else dlg_notify(w->hwnd, ok ? 9 : 10);
    dlg_notify(w->hwnd, 8);        /* CBN_CLOSEUP */
}
static void combo_open(DlgWin* w)
{
    if (w->dropped) return;
    dlg_notify(w->hwnd, 7);        /* CBN_DROPDOWN */
    w->dropped = 1;
    w->hot = w->sel;
    w->top = 0;
    int rows = (ctl_dropdown_height(w) - 2) / line_h(w);
    if (w->sel >= rows) w->top = w->sel - rows + 1;
    dlg_capture(w->hwnd);
    w->dirty = 1;
}

static int combo_proc(DlgWin* w, uint32_t msg, uint32_t wp, uint32_t lp, uint32_t* res)
{
    uint32_t st = style_of(w);
    switch (msg) {
    case 0x143: *res = (uint32_t)items_insert(w, w->nitems, lp ? w32_str(lp) : "", (st & 0x100) != 0); return 1;
    case 0x14a: *res = (uint32_t)items_insert(w, (int32_t)wp < 0 ? w->nitems : (int)wp, lp ? w32_str(lp) : "", 0); return 1;
    case 0x144: *res = (uint32_t)items_delete(w, (int)wp); return 1;
    case 0x14b: items_clear(w); *res = 0; return 1;
    case 0x146: *res = (uint32_t)w->nitems; return 1;
    case 0x147: *res = (uint32_t)w->sel; return 1;
    case 0x14e:                                                    /* CB_SETCURSEL */
        if ((int32_t)wp < 0 || (int)wp >= w->nitems) { w->sel = -1; w->dirty = 1; *res = 0xffffffffu; return 1; }
        w->sel = (int)wp; w->dirty = 1; *res = wp; return 1;
    case 0x148:                                                    /* CB_GETLBTEXT */
        if ((int)wp >= w->nitems) { *res = 0xffffffffu; return 1; }
        w32_put_str(lp, w->items[wp].s, 0);
        *res = (uint32_t)strlen(w->items[wp].s);
        return 1;
    case 0x149: *res = (int)wp < w->nitems ? (uint32_t)strlen(w->items[wp].s) : 0xffffffffu; return 1;
    case 0x14c: *res = (uint32_t)items_find(w, (int32_t)wp, lp ? w32_str(lp) : "", 0); return 1;
    case 0x158: *res = (uint32_t)items_find(w, (int32_t)wp, lp ? w32_str(lp) : "", 1); return 1;
    case 0x14d: {                                                  /* CB_SELECTSTRING */
        int k = items_find(w, (int32_t)wp, lp ? w32_str(lp) : "", 0);
        if (k >= 0) { w->sel = k; w->dirty = 1; }
        *res = (uint32_t)k;
        return 1;
    }
    case 0x14f: if (wp) combo_open(w); else combo_close(w, 0); *res = 1; return 1;   /* CB_SHOWDROPDOWN */
    case 0x150: *res = (int)wp < w->nitems ? w->items[wp].data : 0xffffffffu; return 1;
    case 0x151: if ((int)wp < w->nitems) w->items[wp].data = lp; *res = 0; return 1;
    case 0x154: *res = (uint32_t)line_h(w); return 1;              /* CB_GETITEMHEIGHT */
    case 0x157: *res = (uint32_t)w->dropped; return 1;             /* CB_GETDROPPEDSTATE */
    case 0x0e: case 0x0d: {                                        /* WM_GETTEXTLENGTH / WM_GETTEXT */
        const char* s = w->sel >= 0 && w->sel < w->nitems ? w->items[w->sel].s : "";
        if (msg == 0x0d && lp && wp) w32_put_str(lp, s, wp);
        *res = (uint32_t)strlen(s);
        return 1;
    }
    case 0x08: combo_close(w, 0); break;
    case 0x100:
        if (w->dropped) {
            int rows = (ctl_dropdown_height(w) - 2) / line_h(w);
            switch (wp) {
            case 0x26: case 0x25: if (w->hot > 0) w->hot--; break;
            case 0x28: case 0x27: if (w->hot < w->nitems - 1) w->hot++; break;
            case 0x0d: combo_close(w, 1); break;
            case 0x1b: combo_close(w, 0); break;
            case 0x73: combo_close(w, 1); break;
            default: return 0;
            }
            if (w->hot >= 0 && w->hot < w->top) w->top = w->hot;
            if (w->hot >= w->top + rows) w->top = w->hot - rows + 1;
            w->dirty = 1;
            *res = 0;
            return 1;
        }
        switch (wp) {
        case 0x26: case 0x25: list_select(w, w->sel < 0 ? 0 : w->sel - 1, 1); break;
        case 0x28: case 0x27: list_select(w, w->sel + 1, 1); break;
        case 0x24: list_select(w, 0, 1); break;
        case 0x23: list_select(w, w->nitems - 1, 1); break;
        case 0x73: combo_open(w); break;
        default: return 0;
        }
        *res = 0;
        return 1;
    case 0x104:   /* WM_SYSKEYDOWN: Alt+Down / Up */
        if (wp == 0x28 || wp == 0x26) { if (w->dropped) combo_close(w, 1); else combo_open(w); *res = 0; return 1; }
        return 0;
    case 0x201: case 0x203: {
        int y = (int16_t)HI(lp), x = (int16_t)LO(lp), ch = combo_closed_h(w);
        DlgInfo i;
        dlg_info(w->hwnd, &i);
        if (w->dropped) {
            int lh = line_h(w), lt = ch + 1;
            if (x >= 0 && x < i.w && y >= lt && y < ch + ctl_dropdown_height(w)) {
                int k = w->top + (y - lt) / lh;
                if (k < w->nitems) w->hot = k;
                combo_close(w, 1);
            } else combo_close(w, y >= 0 && y < ch && x >= 0 && x < i.w);
        } else {
            dlg_set_focus(w->hwnd);
            combo_open(w);
        }
        *res = 0;
        return 1;
    }
    case 0x200:
        if (w->dropped) {
            int y = (int16_t)HI(lp) - combo_closed_h(w) - 1;
            int k = y >= 0 ? w->top + y / line_h(w) : -1;
            if (k >= 0 && k < w->nitems && k != w->hot) { w->hot = k; w->dirty = 1; }
        }
        *res = 0;
        return 1;
    case 0x202: *res = 0; return 1;
    default: return 0;
    }
    return 0;
}

static void combo_paint(DlgWin* w, uint32_t hdc, int cw, int ch)
{
    (void)ch;
    uint32_t st = style_of(w);
    int h = combo_closed_h(w), dis = (st & WS_DISABLED) != 0;
    uint32_t fill;
    dlg_ctlcolor(w->hwnd, 0x134, hdc, 0xffffff, &fill);
    gdi_fill(hdc, 0, 0, cw, h, dis ? FACE : fill);
    gdi_edge(hdc, 0, 0, cw, h, 1);
    /* the arrow button */
    int bx = cw - 2 - 16;
    gdi_fill(hdc, bx, 2, cw - 2, h - 2, FACE);
    if (w->dropped) gdi_frame(hdc, bx, 2, cw - 2, h - 2, 0x808080);
    else gdi_edge(hdc, bx, 2, cw - 2, h - 2, 0);
    int ax = bx + 4 + (w->dropped ? 1 : 0), ay = h / 2 - 1 + (w->dropped ? 1 : 0);
    for (int k = 0; k < 4; k++) gdi_line_h(hdc, ax + k, ax + 7 - k, ay + k, dis ? 0x808080 : 0);
    const char* s = w->sel >= 0 && w->sel < w->nitems ? w->items[w->sel].s : "";
    int foc = dlg_focus() == w->hwnd && !w->dropped;
    uint32_t text = gdi_dc_text_color(hdc);
    if (foc) gdi_fill(hdc, 3, 3, bx - 1, h - 3, 0x800000);
    gdi_dc_select_font(hdc, w->font ? w->font : gdi_default_font());
    gdi_dc_reset_colors(hdc, dis ? 0x808080 : foc ? 0xffffff : text, foc ? 0x800000 : fill, 1);
    int L = 5, T = 2, R = bx - 2, B = h - 2;
    gdi_draw_text(hdc, s, -1, &L, &T, &R, &B, 0x824);
    if (foc) focus_rect(hdc, 3, 3, bx - 1, h - 3);
    gdi_dc_reset_colors(hdc, text, fill, 1);
}

void ctl_paint_dropdown(DlgWin* w, uint32_t hdc, int cw, int listh)
{
    uint32_t fill;
    dlg_ctlcolor(w->hwnd, 0x134, hdc, 0xffffff, &fill);
    gdi_fill(hdc, 0, 0, cw, listh, fill);
    gdi_frame(hdc, 0, 0, cw, listh, 0);
    list_draw(w, hdc, 1, 1, cw - 1, listh - 1, fill, w->hot, 0);
}

/* ---- track bar ---- */

static void tb_geom(DlgWin* w, int cw, int ch, int* cl, int* cr, int* cy, int* thx)
{
    uint32_t st = style_of(w);
    *cl = 8 + 5;
    *cr = cw - 8 - 5;
    *cy = (st & 8) ? ch / 2 : (st & 4) ? ch / 2 + 4 : ch / 2 - 4;
    int span = w->tmax > w->tmin ? w->tmax - w->tmin : 1;
    *thx = *cl + (int)((int64_t)(*cr - *cl) * (w->tpos - w->tmin) / span);
}
static void tb_set(DlgWin* w, int pos, uint32_t code)
{
    pos = clampi(pos, w->tmin, w->tmax);
    if (pos != w->tpos) { w->tpos = pos; w->dirty = 1; }
    scroll_notify(w, code);
}

static int trackbar_proc(DlgWin* w, uint32_t msg, uint32_t wp, uint32_t lp, uint32_t* res)
{
    DlgInfo i;
    dlg_info(w->hwnd, &i);
    switch (msg) {
    case 0x400: *res = (uint32_t)w->tpos; return 1;                      /* TBM_GETPOS */
    case 0x401: *res = (uint32_t)w->tmin; return 1;
    case 0x402: *res = (uint32_t)w->tmax; return 1;
    case 0x405: w->tpos = clampi((int32_t)lp, w->tmin, w->tmax); w->dirty = 1; *res = 0; return 1;
    case 0x406: w->tmin = (int16_t)LO(lp); w->tmax = (int16_t)HI(lp);   /* TBM_SETRANGE */
        if (w->tmax < w->tmin) w->tmax = w->tmin;
        w->tmin = (int)LO(lp); w->tmax = (int)HI(lp);
        w->tpos = clampi(w->tpos, w->tmin, w->tmax); w->dirty = 1; *res = 0; return 1;
    case 0x407: w->tmin = (int32_t)lp; w->tpos = clampi(w->tpos, w->tmin, w->tmax); w->dirty = 1; *res = 0; return 1;
    case 0x408: w->tmax = (int32_t)lp; w->tpos = clampi(w->tpos, w->tmin, w->tmax); w->dirty = 1; *res = 0; return 1;
    case 0x409: *res = 0; return 1;                                      /* TBM_CLEARTICS */
    case 0x40b: w->tsel0 = (int32_t)lp; *res = 0; return 1;
    case 0x40c: w->tsel1 = (int32_t)lp; *res = 0; return 1;
    case 0x40a: w->tsel0 = (int)LO(lp); w->tsel1 = (int)HI(lp); *res = 0; return 1;
    case 0x410: *res = w->tfreq > 0 ? (uint32_t)((w->tmax - w->tmin) / w->tfreq + 1) : 2; return 1;
    case 0x414: w->tfreq = (int)wp; w->dirty = 1; *res = 0; return 1;   /* TBM_SETTICFREQ */
    case 0x415: *res = (uint32_t)w->tpage; w->tpage = (int32_t)lp; return 1;
    case 0x416: *res = (uint32_t)w->tpage; return 1;
    case 0x417: *res = (uint32_t)w->tline; w->tline = (int32_t)lp; return 1;
    case 0x418: *res = (uint32_t)w->tline; return 1;
    case 0x419: case 0x41a: {                                            /* thumb / channel rect */
        int cl, cr, cy, tx;
        tb_geom(w, i.w, i.h, &cl, &cr, &cy, &tx);
        if (msg == 0x419) { rt_w32(lp, (uint32_t)(tx - 5)); rt_w32(lp + 4, (uint32_t)(cy - 10)); rt_w32(lp + 8, (uint32_t)(tx + 6)); rt_w32(lp + 12, (uint32_t)(cy + 10)); }
        else { rt_w32(lp, (uint32_t)(cl - 5)); rt_w32(lp + 4, (uint32_t)(cy - 2)); rt_w32(lp + 8, (uint32_t)(cr + 5)); rt_w32(lp + 12, (uint32_t)(cy + 2)); }
        *res = 0;
        return 1;
    }
    case 0x100: {                                                        /* WM_KEYDOWN */
        int line = w->tline > 0 ? w->tline : 1, page = w->tpage > 0 ? w->tpage : (w->tmax - w->tmin) / 5;
        switch (wp) {
        case 0x25: case 0x26: tb_set(w, w->tpos - line, 0); break;       /* TB_LINEUP */
        case 0x27: case 0x28: tb_set(w, w->tpos + line, 1); break;       /* TB_LINEDOWN */
        case 0x21: tb_set(w, w->tpos - page, 2); break;
        case 0x22: tb_set(w, w->tpos + page, 3); break;
        case 0x24: tb_set(w, w->tmin, 6); break;
        case 0x23: tb_set(w, w->tmax, 7); break;
        default: return 0;
        }
        *res = 0;
        return 1;
    }
    case 0x101:
        if ((wp >= 0x21 && wp <= 0x28)) { scroll_notify(w, 8); *res = 0; return 1; }   /* TB_ENDTRACK */
        return 0;
    case 0x201: case 0x203: {
        dlg_set_focus(w->hwnd);
        int cl, cr, cy, tx, x = (int16_t)LO(lp);
        tb_geom(w, i.w, i.h, &cl, &cr, &cy, &tx);
        dlg_capture(w->hwnd);
        if (x >= tx - 6 && x <= tx + 6) w->tracking = 1;
        else {
            int page = w->tpage > 0 ? w->tpage : (w->tmax - w->tmin) / 5;
            if (x < tx) tb_set(w, w->tpos - page, 2); else tb_set(w, w->tpos + page, 3);
            w->tracking = 2;
        }
        *res = 0;
        return 1;
    }
    case 0x200:
        if (w->tracking == 1) {
            int cl, cr, cy, tx, x = (int16_t)LO(lp);
            tb_geom(w, i.w, i.h, &cl, &cr, &cy, &tx);
            int span = cr - cl > 0 ? cr - cl : 1;
            int pos = w->tmin + (int)((int64_t)(clampi(x, cl, cr) - cl) * (w->tmax - w->tmin) * 2 / span + 1) / 2;
            if (pos != w->tpos) tb_set(w, pos, 5);                       /* TB_THUMBTRACK */
        }
        *res = 0;
        return 1;
    case 0x202:
        if (w->tracking) {
            if (w->tracking == 1) scroll_notify(w, 4);                   /* TB_THUMBPOSITION */
            scroll_notify(w, 8);
            w->tracking = 0;
            dlg_capture(0);
        }
        *res = 0;
        return 1;
    default: return 0;
    }
}

static void trackbar_paint(DlgWin* w, uint32_t hdc, int cw, int ch)
{
    uint32_t st = style_of(w);
    uint32_t fill;
    dlg_ctlcolor(w->hwnd, 0x138, hdc, FACE, &fill);
    gdi_fill(hdc, 0, 0, cw, ch, fill);
    int cl, cr, cy, tx;
    tb_geom(w, cw, ch, &cl, &cr, &cy, &tx);
    /* the channel */
    gdi_fill(hdc, cl - 5, cy - 2, cr + 6, cy + 2, 0xffffff);
    gdi_edge(hdc, cl - 5, cy - 2, cr + 6, cy + 2, 1);
    /* ticks */
    if (!(st & 0x10)) {
        int span = w->tmax > w->tmin ? w->tmax - w->tmin : 1;
        int f = (st & 1) && w->tfreq > 0 ? w->tfreq : span;
        uint32_t tc = (fill & 0xff) + (fill >> 8 & 0xff) + (fill >> 16 & 0xff) < 0x180 ? 0xc0c0c0 : 0x000000;
        for (int v = w->tmin; ; v += f) {
            if (v > w->tmax) v = w->tmax;
            int x = cl + (int)((int64_t)(cr - cl) * (v - w->tmin) / span);
            int big = v == w->tmin || v == w->tmax;
            if ((st & 4) || (st & 8)) gdi_line_v(hdc, x, cy - 14 - big, cy - 11, tc);
            if (!(st & 4) || (st & 8)) gdi_line_v(hdc, x, cy + 11, cy + 15 + big, tc);
            if (v >= w->tmax) break;
        }
    }
    /* the thumb */
    if (!(st & 0x80)) {
        int t = cy - 10, b = cy + 10;
        gdi_fill(hdc, tx - 5, t, tx + 6, b, w->tracking == 1 ? 0xe0e0e0 : FACE);
        gdi_edge(hdc, tx - 5, t, tx + 6, b, 0);
    }
    if (dlg_focus() == w->hwnd) focus_rect(hdc, 0, 0, cw, ch);
}

/* ---- list view (list / report, text) ---- */

static void lv_notify(DlgWin* w, int32_t code, int item, uint32_t newst, uint32_t oldst)
{
    DlgInfo i;
    if (!dlg_info(w->hwnd, &i)) return;
    uint32_t p = rt_frame_push(48);
    rt_w32(p, w->hwnd);
    rt_w32(p + 4, i.id);
    rt_w32(p + 8, (uint32_t)code);
    rt_w32(p + 12, (uint32_t)item);
    rt_w32(p + 20, newst);
    rt_w32(p + 24, oldst);
    rt_w32(p + 28, 8);   /* LVIF_STATE */
    if (item >= 0 && item < w->nitems) rt_w32(p + 40, w->items[item].data);
    dlg_send(i.parent, 0x4e, i.id, p);   /* WM_NOTIFY */
    rt_frame_pop(48);
}
static void lv_select(DlgWin* w, int k)
{
    if (k < 0 || k >= w->nitems || k == w->sel) return;
    int old = w->sel;
    if (old >= 0 && old < w->nitems) { w->items[old].state &= ~3u; lv_notify(w, -101, old, 0, 3); }
    w->sel = k;
    w->items[k].state |= 3;
    lv_notify(w, -101, k, 3, 0);   /* LVN_ITEMCHANGED */
    w->dirty = 1;
}
static void lv_set_from(DlgWin* w, int k, uint32_t it)
{
    uint32_t mask = rt_r32(it);
    int sub = (int32_t)rt_r32(it + 8);
    if ((mask & 1) && rt_r32(it + 20) && rt_r32(it + 20) != 0xffffffffu) {
        const char* s = w32_str(rt_r32(it + 20));
        if (sub == 0) { free(w->items[k].s); w->items[k].s = dupn(s); }
        else if (sub >= 1 && sub <= 4) { free(w->items[k].sub[sub - 1]); w->items[k].sub[sub - 1] = dupn(s); }
    }
    if (sub == 0) {
        if (mask & 2) w->items[k].image = rt_r32(it + 28);
        if (mask & 4) w->items[k].data = rt_r32(it + 32);
        if (mask & 8) {
            uint32_t sm = rt_r32(it + 16);
            w->items[k].state = (w->items[k].state & ~sm) | (rt_r32(it + 12) & sm);
            if (w->items[k].state & 2) w->sel = k;
        }
    }
    w->dirty = 1;
}

static int cols_w[5] = {120, 80, 80, 80, 80};
static int listview_proc(DlgWin* w, uint32_t msg, uint32_t wp, uint32_t lp, uint32_t* res)
{
    DlgInfo i;
    dlg_info(w->hwnd, &i);
    int rows = list_rows(w, i.h - 4);
    switch (msg) {
    case 0x1000: *res = w->bk; return 1;
    case 0x1001: w->bk = lp; w->bk_set = 1; w->dirty = 1; *res = 1; return 1;   /* LVM_SETBKCOLOR */
    case 0x1002: case 0x1003: *res = 0; return 1;                        /* image lists */
    case 0x1004: *res = (uint32_t)w->nitems; return 1;                   /* LVM_GETITEMCOUNT */
    case 0x1005: {                                                       /* LVM_GETITEMA */
        int k = (int32_t)rt_r32(lp + 4);
        if (k < 0 || k >= w->nitems) { *res = 0; return 1; }
        uint32_t mask = rt_r32(lp);
        int sub = (int32_t)rt_r32(lp + 8);
        if ((mask & 1) && rt_r32(lp + 20)) {
            const char* s = sub == 0 ? w->items[k].s : sub <= 4 && w->items[k].sub[sub - 1] ? w->items[k].sub[sub - 1] : "";
            w32_put_str(rt_r32(lp + 20), s, rt_r32(lp + 24));
        }
        if (mask & 4) rt_w32(lp + 32, w->items[k].data);
        if (mask & 8) rt_w32(lp + 12, w->items[k].state & rt_r32(lp + 16));
        if (mask & 2) rt_w32(lp + 28, w->items[k].image);
        *res = 1;
        return 1;
    }
    case 0x1006: {                                                       /* LVM_SETITEMA */
        int k = (int32_t)rt_r32(lp + 4);
        if (k < 0 || k >= w->nitems) { *res = 0; return 1; }
        lv_set_from(w, k, lp);
        *res = 1;
        return 1;
    }
    case 0x1007: {                                                       /* LVM_INSERTITEMA */
        int k = items_insert(w, (int32_t)rt_r32(lp + 4), "", 0);
        rt_w32(lp + 8, 0);
        lv_set_from(w, k, lp);
        *res = (uint32_t)k;
        return 1;
    }
    case 0x1008: *res = items_delete(w, (int)wp) >= 0; return 1;
    case 0x1009: items_clear(w); *res = 1; return 1;
    case 0x100c: {                                                       /* LVM_GETNEXTITEM */
        for (int k = (int32_t)wp + 1; k < w->nitems; k++)
            if (!(LO(lp) & 3) || (w->items[k].state & LO(lp) & 3)) { *res = (uint32_t)k; return 1; }
        *res = 0xffffffffu;
        return 1;
    }
    case 0x1013: if ((int)wp < w->top) w->top = (int)wp; else if ((int)wp >= w->top + rows) w->top = (int)wp - rows + 1; w->dirty = 1; *res = 1; return 1;
    case 0x101b: {                                                       /* LVM_INSERTCOLUMNA */
        if (wp < 5 && (rt_r32(lp) & 2)) cols_w[wp] = (int32_t)rt_r32(lp + 8);
        *res = wp;
        return 1;
    }
    case 0x101e: if (wp < 5) cols_w[wp] = (int)LO(lp); *res = 1; return 1;   /* LVM_SETCOLUMNWIDTH */
    case 0x1024: w->fg_set = 1; w->fg = lp; w->dirty = 1; *res = 1; return 1;
    case 0x1026: *res = 1; return 1;
    case 0x102b: {                                                       /* LVM_SETITEMSTATE */
        uint32_t sm = rt_r32(lp + 16), sv = rt_r32(lp + 12);
        for (int k = (int32_t)wp < 0 ? 0 : (int)wp; k < w->nitems && (k == (int)wp || (int32_t)wp < 0); k++) {
            w->items[k].state = (w->items[k].state & ~sm) | (sv & sm);
            if (sv & sm & 2) w->sel = k;
        }
        w->dirty = 1;
        *res = 1;
        return 1;
    }
    case 0x102c: *res = (int)wp < w->nitems ? w->items[wp].state & lp : 0; return 1;
    case 0x102d: {                                                       /* LVM_GETITEMTEXTA */
        rt_w32(lp + 4, wp);
        rt_w32(lp, 1);
        uint32_t r2;
        uint32_t v = lp;
        (void)v;
        listview_proc(w, 0x1005, 0, lp, &r2);
        *res = rt_r32(lp + 20) ? (uint32_t)strlen(w32_str(rt_r32(lp + 20))) : 0;
        return 1;
    }
    case 0x102e: {                                                       /* LVM_SETITEMTEXTA */
        if ((int)wp >= w->nitems) { *res = 0; return 1; }
        rt_w32(lp, 1);
        lv_set_from(w, (int)wp, lp);
        *res = 1;
        return 1;
    }
    case 0x100: {
        int s = w->sel;
        if (wp == 0x26) s--; else if (wp == 0x28) s++; else if (wp == 0x24) s = 0; else if (wp == 0x23) s = w->nitems - 1;
        else return 0;
        lv_select(w, clampi(s, 0, w->nitems - 1));
        if (w->sel < w->top) w->top = w->sel;
        if (w->sel >= w->top + rows) w->top = w->sel - rows + 1;
        *res = 0;
        return 1;
    }
    case 0x201: case 0x203: {
        dlg_set_focus(w->hwnd);
        int k = w->top + ((int16_t)HI(lp) - 2) / line_h(w);
        if (k < w->nitems) lv_select(w, k);
        lv_notify(w, msg == 0x203 ? -3 : -2, k < w->nitems ? k : -1, 0, 0);   /* NM_DBLCLK / NM_CLICK */
        *res = 0;
        return 1;
    }
    default: return 0;
    }
}

static void listview_paint(DlgWin* w, uint32_t hdc, int cw, int ch)
{
    /* a color the game set, black included (the lobby's player list: white on black) */
    uint32_t bg = w->bk_set && w->bk != 0xffffffffu ? w->bk & 0xffffff : 0xffffff;
    gdi_fill(hdc, 0, 0, cw, ch, bg);
    gdi_edge(hdc, 0, 0, cw, ch, 1);
    int lh = line_h(w);
    uint32_t fg = w->fg_set && w->fg != 0xffffffffu ? w->fg & 0xffffff : 0;
    gdi_dc_select_font(hdc, w->font ? w->font : gdi_default_font());
    for (int k = w->top, y = 2; k < w->nitems && y + lh <= ch - 2; k++, y += lh) {
        int on = k == w->sel;
        uint32_t ibg = on ? 0x800000 : bg;
        gdi_fill(hdc, 2, y, cw - 2, y + lh, ibg);
        gdi_dc_reset_colors(hdc, on ? 0xffffff : fg, ibg, 1);
        int x = 4;
        for (int c = 0; c < 5 && x < cw; c++) {
            const char* s = c == 0 ? w->items[k].s : w->items[k].sub[c - 1];
            if (s) { int L = x, T = y, R = x + cols_w[c] - 4, B = y + lh; gdi_draw_text(hdc, s, -1, &L, &T, &R, &B, 0x824); }
            x += cols_w[c];
        }
    }
}

/* ---- the common entry points ---- */

int ctl_proc(DlgWin* w, uint32_t msg, uint32_t wp, uint32_t lp, uint32_t* res)
{
    switch (w->kind) {
    case K_BUTTON: return button_proc(w, msg, wp, lp, res);
    case K_EDIT: return edit_proc(w, msg, wp, lp, res);
    case K_LISTBOX: return listbox_proc(w, msg, wp, lp, res);
    case K_COMBO: return combo_proc(w, msg, wp, lp, res);
    case K_TRACKBAR: return trackbar_proc(w, msg, wp, lp, res);
    case K_LISTVIEW: return listview_proc(w, msg, wp, lp, res);
    default: return 0;
    }
}

void ctl_paint(DlgWin* w, uint32_t hdc, int cw, int ch)
{
    switch (w->kind) {
    case K_BUTTON: button_paint(w, hdc, cw, ch); break;
    case K_STATIC: static_paint(w, hdc, cw, ch); break;
    case K_EDIT: edit_paint(w, hdc, cw, ch); break;
    case K_LISTBOX: listbox_paint(w, hdc, cw, ch); break;
    case K_COMBO: combo_paint(w, hdc, cw, ch); break;
    case K_TRACKBAR: trackbar_paint(w, hdc, cw, ch); break;
    case K_LISTVIEW: listview_paint(w, hdc, cw, ch); break;
    case K_SCROLLBAR: gdi_fill(hdc, 0, 0, cw, ch, 0xe0e0e0); break;
    default: break;
    }
}

/* WM_GETDLGCODE */
int ctl_dlgcode(DlgWin* w, uint32_t vk)
{
    (void)vk;
    switch (w->kind) {
    case K_BUTTON: {
        int t = btype(w);
        if (t == 7) return 0x100;                         /* DLGC_STATIC */
        if (t == 4 || t == 9) return 0x2000 | 0x40;       /* DLGC_BUTTON | DLGC_RADIOBUTTON */
        if (is_push(t)) return 0x2000 | (dlg_is_default_button(w->hwnd) ? 0x10 : 0x20);
        return 0x2000;
    }
    case K_STATIC: return 0x100;
    case K_EDIT: return 0x80 | 0x8 | 0x1 | ((style_of(w) & 4) ? 0x4 : 0);
    case K_COMBO: return 0x80 | 0x1 | (w->dropped ? 0x4 : 0);
    case K_LISTBOX: case K_LISTVIEW: return 0x80 | 0x1;
    case K_TRACKBAR: case K_SCROLLBAR: return 0x1;
    default: return 0;
    }
}
