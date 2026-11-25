/*
 * gdi.c: GDI on host pictures for the dialogs and controls (dialogs.h), port code.
 *
 * Objects are host records behind H_GDI handles: DCs (a window's picture, a memory DC's selected
 * bitmap, or nothing: a DC that only measures, e.g. GetDC of the game's own window), solid
 * brushes, fonts, bitmaps (32 bit host pixels, from LoadBitmapA's DIB resources), stock objects.
 * Handles the old stubs made (palettes) have no record and stay inert.
 *
 * Drawing: FillRect / FrameRect with brushes or (COLOR_x + 1), StretchDIBits of 1 / 4 / 8 / 16 /
 * 24 / 32 bit DIBs (bottom up or top down; SRCCOPY), BitBlt from a memory DC, text (TextOutA,
 * ExtTextOutA, DrawTextA with alignment, word breaks, '&' prefixes, DT_CALCRECT) in the DC's
 * colours and background mode. Text uses VGA8X8.FNT (the game's 8 x 8 font, read from the data
 * folder at run time) scaled to the font's height and spaced proportionally; bold draws twice.
 * GetTextMetricsA / GetTextExtentPoint32A answer the same metrics the drawing uses.
 *
 * Not done: pens and lines (none imported), clipping regions, ROPs other than copies, palettes
 * (8 bit DIBs carry their own colours), text on DirectDraw surface DCs (drawn nowhere, as before).
 */
#include "host/dialogs/dialogs.h"
#include "host/win32_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum { G_DC = 1, G_BRUSH, G_PEN, G_FONT, G_BITMAP };

typedef struct {
    int kind;
    int stock;
    /* brush / pen */
    uint32_t color;
    int hollow;
    /* font */
    int height, weight, italic, underline;
    char face[32];
    /* bitmap */
    DlgPic bmp;
    /* dc */
    DlgPic* target;
    int ox, oy, mem;
    uint32_t font, brush, pen, bitmap;
    uint32_t text, bk;
    int mode;
} GObj;

static GObj* obj(uint32_t h) { return w32_handle_get(h, H_GDI); }
static uint32_t make(GObj* o) { return w32_handle_new(H_GDI, o); }
static GObj* alloc_obj(int kind) { GObj* o = calloc(1, sizeof *o); o->kind = kind; return o; }

static uint32_t argb(uint32_t cr) { return 0xff000000u | (cr & 0xff) << 16 | (cr & 0xff00) | (cr >> 16 & 0xff); }

/* ---- the font ---- */

static uint8_t font8[256][8];
static uint8_t glyph_l[256], glyph_w[256];
static int font_state;   /* 0 not loaded, 1 loaded, -1 missing */

static void font_load(void)
{
    if (font_state) return;
    font_state = -1;
    char path[1024];
    w32_host_path("VGA8X8.FNT", path, sizeof path);
    FILE* f = fopen(path, "rb");
    if (f) {
        if (fread(font8, 1, sizeof font8, f) == sizeof font8) font_state = 1;
        fclose(f);
    }
    if (font_state < 0) fprintf(stderr, "tm2 host: dialogs: %s not found, text drawn as boxes\n", path);
    for (int c = 0; c < 256; c++) {
        int lo = 8, hi = -1;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                if (font_state > 0 ? font8[c][y] & (1 << x) : (c > 32 && x > 0 && x < 6))
                    { if (x < lo) lo = x; if (x > hi) hi = x; }
        if (hi < 0) { glyph_l[c] = 0; glyph_w[c] = 3; }   /* blank: a space's width */
        else { glyph_l[c] = (uint8_t)lo; glyph_w[c] = (uint8_t)(hi - lo + 1); }
    }
}
static int glyph_bit(int c, int x, int y)
{
    if (font_state > 0) return (font8[c][y] & (1 << x)) != 0;   /* bit 0 is the left column */
    return c > 32 && x > 0 && x < 6 && y > 0 && y < 7 && (x == 1 || x == 5 || y == 1 || y == 6);
}

/* metrics of a font object (NULL: the dialog font) */
typedef struct { int scale, line, bold, underline; } FM;
static FM metrics(uint32_t font)
{
    GObj* f = obj(font);
    FM m = {1, 13, 0, 0};
    if (f && f->kind == G_FONT) {
        int h = f->height < 0 ? -f->height : f->height;
        if (h == 0) h = 13;
        m.scale = h >= 22 ? (h + 6) / 12 : 1;
        m.line = h > 8 * m.scale + 2 ? h : 8 * m.scale + 2 * m.scale + 1;
        m.bold = f->weight >= 600;
        m.underline = f->underline;
    }
    return m;
}
static int adv(const FM* m, unsigned char c) { return (glyph_w[c] + 1 + m->bold) * m->scale; }

int gdi_text_width(uint32_t font, const char* s, int len)
{
    font_load();
    FM m = metrics(font);
    int w = 0;
    for (int i = 0; i < len; i++) w += adv(&m, (unsigned char)s[i]);
    return w;
}
int gdi_font_height(uint32_t font) { return metrics(font).line; }

/* ---- DCs ---- */

static GObj* dc(uint32_t h) { GObj* o = obj(h); return o && o->kind == G_DC ? o : NULL; }
int gdi_is_dc(uint32_t h) { return dc(h) != NULL; }

static uint32_t stock[20];
static uint32_t stock_obj(uint32_t i);

uint32_t gdi_default_font(void) { return stock_obj(17); }

static GObj* new_dc(void)
{
    GObj* d = alloc_obj(G_DC);
    d->text = 0;
    d->bk = 0xffffff;
    d->mode = 2;   /* OPAQUE */
    d->font = stock_obj(13);
    d->brush = stock_obj(0);
    d->pen = stock_obj(7);
    return d;
}
uint32_t gdi_dc_for(DlgPic* pic, int ox, int oy)
{
    GObj* d = new_dc();
    d->target = pic;
    d->ox = ox;
    d->oy = oy;
    d->font = gdi_default_font();
    return make(d);
}
void gdi_dc_free(uint32_t h)
{
    GObj* d = dc(h);
    if (!d) return;
    free(d);
    w32_handle_free(h);
}
uint32_t gdi_dc_text_color(uint32_t h) { GObj* d = dc(h); return d ? d->text : 0; }
uint32_t gdi_dc_bk_color(uint32_t h) { GObj* d = dc(h); return d ? d->bk : 0xffffff; }
int gdi_dc_bk_mode(uint32_t h) { GObj* d = dc(h); return d ? d->mode : 2; }
void gdi_dc_reset_colors(uint32_t h, uint32_t text, uint32_t bk, int mode)
{
    GObj* d = dc(h);
    if (!d) return;
    d->text = text;
    d->bk = bk;
    d->mode = mode;
}
uint32_t gdi_dc_font(uint32_t h) { GObj* d = dc(h); return d ? d->font : 0; }
void gdi_dc_select_font(uint32_t h, uint32_t font)
{
    GObj* d = dc(h);
    GObj* f = obj(font);
    if (d && f && f->kind == G_FONT) d->font = font;
}

/* the picture a DC draws into (NULL: nothing) */
static DlgPic* surf(GObj* d)
{
    if (!d) return NULL;
    if (d->mem) { GObj* b = obj(d->bitmap); return b && b->kind == G_BITMAP && b->bmp.px ? &b->bmp : NULL; }
    return d->target && d->target->px ? d->target : NULL;
}

static void put(DlgPic* p, int x, int y, uint32_t c)
{
    if (x >= 0 && y >= 0 && x < p->w && y < p->h) p->px[y * p->w + x] = c;
}
static void fill_abs(DlgPic* p, int l, int t, int r, int b, uint32_t c)
{
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r > p->w) r = p->w;
    if (b > p->h) b = p->h;
    for (int y = t; y < b; y++)
        for (int x = l; x < r; x++) p->px[y * p->w + x] = c;
}

void gdi_fill(uint32_t h, int l, int t, int r, int b, uint32_t color)
{
    GObj* d = dc(h);
    DlgPic* p = surf(d);
    if (p) fill_abs(p, l + d->ox, t + d->oy, r + d->ox, b + d->oy, argb(color));
}
void gdi_frame(uint32_t h, int l, int t, int r, int b, uint32_t color)
{
    if (r <= l || b <= t) return;
    gdi_fill(h, l, t, r, t + 1, color);
    gdi_fill(h, l, b - 1, r, b, color);
    gdi_fill(h, l, t, l + 1, b, color);
    gdi_fill(h, r - 1, t, r, b, color);
}
void gdi_line_h(uint32_t h, int x0, int x1, int y, uint32_t color) { gdi_fill(h, x0, y, x1, y + 1, color); }
void gdi_line_v(uint32_t h, int x, int y0, int y1, uint32_t color) { gdi_fill(h, x, y0, x + 1, y1, color); }
void gdi_edge(uint32_t h, int l, int t, int r, int b, int sunken)
{
    /* Windows 95 3D edges: raised = white / light gray outside, dark gray / black inside */
    uint32_t lt1 = sunken ? 0x808080 : 0xc0c0c0, lt2 = sunken ? 0x000000 : 0xffffff;
    uint32_t rb1 = sunken ? 0xffffff : 0x000000, rb2 = sunken ? 0xc0c0c0 : 0x808080;
    gdi_line_h(h, l, r - 1, t, lt1);
    gdi_line_v(h, l, t, b - 1, lt1);
    gdi_line_h(h, l, r, b - 1, rb1);
    gdi_line_v(h, r - 1, t, b, rb1);
    gdi_line_h(h, l + 1, r - 2, t + 1, lt2);
    gdi_line_v(h, l + 1, t + 1, b - 2, lt2);
    gdi_line_h(h, l + 1, r - 1, b - 2, rb2);
    gdi_line_v(h, r - 2, t + 1, b - 1, rb2);
}

/* one line of text at (x, y) (DC coordinates, y the top of the line), clipped to clip (absolute) */
static void text_line(GObj* d, DlgPic* p, int x, int y, const char* s, int len, int prefix, const int* clip)
{
    FM m = metrics(d->font);
    uint32_t c = argb(d->text);
    int gy = y + d->oy + (m.line - 8 * m.scale) / 2;
    x += d->ox;
    int ul = -1;
    for (int i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (prefix && ch == '&') {
            if (i + 1 < len && s[i + 1] == '&') i++;
            else { ul = i + 1; continue; }
            ch = '&';
        }
        int a = adv(&m, ch);
        for (int gyi = 0; gyi < 8; gyi++)
            for (int gxi = 0; gxi < 8; gxi++) {
                if (!glyph_bit(ch, gxi, gyi)) continue;
                for (int sy = 0; sy < m.scale; sy++)
                    for (int sx = 0; sx < m.scale * (1 + m.bold); sx++) {
                        int px = x + (gxi - glyph_l[ch]) * m.scale + sx, py = gy + gyi * m.scale + sy;
                        if (px < clip[0] || py < clip[1] || px >= clip[2] || py >= clip[3]) continue;
                        put(p, px, py, c);
                    }
            }
        if (i == ul || m.underline) {
            int uy = gy + 8 * m.scale;
            for (int k = 0; k < a; k++)
                if (x + k >= clip[0] && x + k < clip[2] && uy >= clip[1] && uy < clip[3]) put(p, x + k, uy, c);
        }
        x += a;
    }
}

/* the width of a line with '&' prefixes taken out */
static int line_width(uint32_t font, const char* s, int len, int prefix)
{
    int w = 0;
    FM m = metrics(font);
    for (int i = 0; i < len; i++) {
        if (prefix && s[i] == '&') {
            if (i + 1 < len && s[i + 1] == '&') i++;
            else continue;
        }
        w += adv(&m, (unsigned char)s[i]);
    }
    return w;
}

int gdi_draw_text(uint32_t h, const char* s, int len, int* l, int* t, int* r, int* b, uint32_t fmt)
{
    font_load();
    GObj* d = dc(h);
    GObj tmp;
    if (!d) { memset(&tmp, 0, sizeof tmp); tmp.font = gdi_default_font(); d = &tmp; }
    if (len < 0) len = (int)strlen(s);
    FM m = metrics(d->font);
    int prefix = !(fmt & 0x800), single = (fmt & 0x20) != 0, wrap = (fmt & 0x10) && !single;
    int width = *r - *l;
    /* split into lines */
    struct { int a, n; } lines[64];
    int nl = 0, i = 0;
    while (i <= len && nl < 64) {
        int st = i, end = i;
        if (single) { end = len; i = len + 1; }
        else {
            while (end < len && s[end] != '\n' && s[end] != '\r') end++;
            if (wrap) {
                int lastsp = -1;
                for (int k = st; k < end; k++) {
                    if (s[k] == ' ') lastsp = k;
                    if (line_width(d->font, s + st, k - st + 1, prefix) > width && k > st) {
                        end = lastsp > st ? lastsp : k;
                        break;
                    }
                }
            }
            i = end;
            if (i < len && s[i] == '\r') i++;
            if (i < len && (s[i] == '\n' || s[i] == ' ')) i++;
            if (i == st) i++;
        }
        lines[nl].a = st;
        lines[nl].n = end - st;
        nl++;
        if (i >= len) break;
    }
    int total = nl * m.line;
    if (fmt & 0x400) {   /* DT_CALCRECT */
        int mw = 0;
        for (int k = 0; k < nl; k++) { int w = line_width(d->font, s + lines[k].a, lines[k].n, prefix); if (w > mw) mw = w; }
        if (!wrap || mw < width) *r = *l + mw;
        *b = *t + total;
        return total;
    }
    DlgPic* p = surf(d);
    if (!p) return total;
    int y = *t;
    if (single && (fmt & 4)) y = *t + (*b - *t - m.line) / 2;
    else if (single && (fmt & 8)) y = *b - m.line;
    int clip[4] = {0, 0, p->w, p->h};
    if (!(fmt & 0x100)) {
        clip[0] = *l + d->ox; clip[1] = *t + d->oy; clip[2] = *r + d->ox; clip[3] = *b + d->oy;
    }
    for (int k = 0; k < nl; k++, y += m.line) {
        int w = line_width(d->font, s + lines[k].a, lines[k].n, prefix);
        int x = *l;
        if (fmt & 1) x = *l + (width - w) / 2;
        else if (fmt & 2) x = *r - w;
        if (d->mode == 2)
            fill_abs(p, x + d->ox > clip[0] ? x + d->ox : clip[0], y + d->oy > clip[1] ? y + d->oy : clip[1],
                     x + w + d->ox < clip[2] ? x + w + d->ox : clip[2], y + m.line + d->oy < clip[3] ? y + m.line + d->oy : clip[3],
                     argb(d->bk));
        text_line(d, p, x, y, s + lines[k].a, lines[k].n, prefix, clip);
    }
    return total;
}

/* ---- objects ---- */

static uint32_t stock_obj(uint32_t i)
{
    if (i >= 20) return 0;
    if (stock[i]) return stock[i];
    GObj* o;
    static const uint32_t gray[5] = {0xffffff, 0xc0c0c0, 0x808080, 0x404040, 0x000000};
    if (i <= 5) { o = alloc_obj(G_BRUSH); o->color = i < 5 ? gray[i] : 0; o->hollow = i == 5; }
    else if (i <= 8) { o = alloc_obj(G_PEN); o->color = i == 6 ? 0xffffff : 0; o->hollow = i == 8; }
    else if (i >= 10 && i != 15) { o = alloc_obj(G_FONT); o->height = i == 17 ? -11 : 16; o->weight = i == 13 ? 700 : 400; snprintf(o->face, sizeof o->face, "%s", i == 17 ? "MS Sans Serif" : "System"); }
    else { o = alloc_obj(G_BRUSH); }   /* DEFAULT_PALETTE and the rest: inert */
    o->stock = 1;
    return stock[i] = make(o);
}

uint32_t gdi_brush_color(uint32_t brush, int* ok)
{
    *ok = 1;
    if (brush && brush <= 31) {   /* (COLOR_x + 1) */
        uint32_t v[1] = {brush - 1};
        return dlg_import("GetSysColor", v, 1);
    }
    GObj* b = obj(brush);
    if (b && b->kind == G_BRUSH && !b->hollow) return b->color;
    *ok = 0;
    return 0;
}

uint32_t w32gdi_GetStockObject(const uint32_t* a, int n) { return stock_obj(W(0)); }
uint32_t w32gdi_CreateSolidBrush(const uint32_t* a, int n)
{
    GObj* o = alloc_obj(G_BRUSH);
    o->color = W(0) & 0xffffff;
    return make(o);
}
/* CreateFontA(h, w, esc, orient, weight, italic, underline, strike, charset, out, clip, quality,
 * pitch, face) */
uint32_t w32gdi_CreateFontA(const uint32_t* a, int n)
{
    GObj* o = alloc_obj(G_FONT);
    o->height = (int32_t)W(0);
    o->weight = (int)W(4);
    o->italic = W(5) != 0;
    o->underline = W(6) != 0;
    if (W(13)) snprintf(o->face, sizeof o->face, "%s", w32_str(W(13)));
    if (getenv("TM2_DLG_LOG")) fprintf(stderr, "tm2 host: CreateFontA %d weight %d '%s'\n", o->height, o->weight, o->face);
    return make(o);
}
uint32_t w32gdi_CreateCompatibleDC(const uint32_t* a, int n)
{
    (void)a; (void)n;
    GObj* d = new_dc();
    d->mem = 1;
    return make(d);
}
uint32_t w32gdi_DeleteDC(const uint32_t* a, int n)
{
    GObj* d = dc(W(0));
    if (d) { free(d); w32_handle_free(W(0)); return 1; }
    if (w32_handle_type(W(0)) == H_GDI) w32_handle_free(W(0));
    return 1;
}
uint32_t w32gdi_DeleteObject(const uint32_t* a, int n)
{
    GObj* o = obj(W(0));
    if (o) {
        if (o->stock || o->kind == G_DC) return 1;
        free(o->bmp.px);
        free(o);
    }
    if (w32_handle_type(W(0)) == H_GDI) w32_handle_free(W(0));
    return 1;
}
/* SelectObject(hdc, obj): the previous object of that kind */
uint32_t w32gdi_SelectObject(const uint32_t* a, int n)
{
    GObj* d = dc(W(0));
    GObj* o = obj(W(1));
    if (!d || !o) return stock_obj(0);
    uint32_t* slot = o->kind == G_FONT ? &d->font : o->kind == G_BRUSH ? &d->brush : o->kind == G_PEN ? &d->pen
                   : o->kind == G_BITMAP ? &d->bitmap : NULL;
    if (!slot) return 0;
    uint32_t old = *slot;
    *slot = W(1);
    return old ? old : stock_obj(0);
}
uint32_t w32gdi_SetBkColor(const uint32_t* a, int n) { GObj* d = dc(W(0)); if (!d) return 0xffffff; uint32_t o = d->bk; d->bk = W(1) & 0xffffff; return o; }
uint32_t w32gdi_SetTextColor(const uint32_t* a, int n) { GObj* d = dc(W(0)); if (!d) return 0; uint32_t o = d->text; d->text = W(1) & 0xffffff; return o; }
uint32_t w32gdi_SetBkMode(const uint32_t* a, int n) { GObj* d = dc(W(0)); if (!d) return 2; uint32_t o = (uint32_t)d->mode; d->mode = (int)W(1); return o; }

/* GetObjectA(obj, size, buf): BITMAP {type, w, h, bytes per line, planes, bpp, bits} or LOGFONTA */
uint32_t w32gdi_GetObjectA(const uint32_t* a, int n)
{
    GObj* o = obj(W(0));
    uint32_t sz = W(1), p = W(2);
    if (p && sz < 0x1000) memset(rt_ptr(p), 0, sz);
    if (!o) return sz;
    if (o->kind == G_BITMAP) {
        if (!p) return 24;
        if (sz >= 24) {
            rt_w32(p + 4, (uint32_t)o->bmp.w);
            rt_w32(p + 8, (uint32_t)o->bmp.h);
            rt_w32(p + 12, (uint32_t)o->bmp.w * 4);
            rt_w16(p + 16, 1);
            rt_w16(p + 18, 32);
        }
        return 24;
    }
    if (o->kind == G_FONT) {
        if (!p) return 60;
        if (sz >= 60) {
            rt_w32(p, (uint32_t)o->height);
            rt_w32(p + 16, (uint32_t)o->weight);
            rt_w8(p + 20, (uint8_t)o->italic);
            rt_w8(p + 21, (uint8_t)o->underline);
            w32_put_str(p + 28, o->face, 32);
        }
        return 60;
    }
    return sz;
}

/* GetTextExtentPoint32A(hdc, text, count, &size) */
uint32_t w32gdi_GetTextExtentPoint32A(const uint32_t* a, int n)
{
    font_load();
    GObj* d = dc(W(0));
    uint32_t f = d ? d->font : gdi_default_font();
    rt_w32(W(3), (uint32_t)gdi_text_width(f, W(1) ? w32_str(W(1)) : "", (int)W(2)));
    rt_w32(W(3) + 4, (uint32_t)gdi_font_height(f));
    return 1;
}
/* GetTextMetricsA(hdc, &TEXTMETRICA) */
uint32_t w32gdi_GetTextMetricsA(const uint32_t* a, int n)
{
    font_load();
    GObj* d = dc(W(0));
    FM m = metrics(d ? d->font : gdi_default_font());
    uint32_t t = W(1);
    memset(rt_ptr(t), 0, 56);
    int asc = (m.line + 8 * m.scale) / 2;
    rt_w32(t + 0, (uint32_t)m.line);           /* tmHeight */
    rt_w32(t + 4, (uint32_t)asc);              /* tmAscent */
    rt_w32(t + 8, (uint32_t)(m.line - asc));   /* tmDescent */
    rt_w32(t + 20, (uint32_t)(6 * m.scale));   /* tmAveCharWidth */
    rt_w32(t + 24, (uint32_t)(9 * m.scale));   /* tmMaxCharWidth */
    rt_w32(t + 28, m.bold ? 700 : 400);        /* tmWeight */
    rt_w8(t + 45, 0x20);                       /* tmFirstChar .. tmBreakChar */
    rt_w8(t + 46, 0xff);
    rt_w8(t + 47, 0x20);
    rt_w8(t + 48, 0x20);
    rt_w8(t + 52, 0x22);                       /* variable pitch, swiss */
    return 1;
}

/* TextOutA(hdc, x, y, text, count) */
uint32_t w32gdi_TextOutA(const uint32_t* a, int n)
{
    int l = (int32_t)W(1), t = (int32_t)W(2), r = l + 4096, b = t + 4096;
    if (W(3)) gdi_draw_text(W(0), w32_str(W(3)), (int)W(4), &l, &t, &r, &b, 0x920);   /* single, no clip, no prefix */
    return 1;
}
/* ExtTextOutA(hdc, x, y, options, rect, text, count, dx) */
uint32_t w32gdi_ExtTextOutA(const uint32_t* a, int n)
{
    uint32_t rc = W(4);
    if (rc && (W(3) & 2)) {   /* ETO_OPAQUE */
        gdi_fill(W(0), (int32_t)rt_r32(rc), (int32_t)rt_r32(rc + 4), (int32_t)rt_r32(rc + 8), (int32_t)rt_r32(rc + 12),
                 gdi_dc_bk_color(W(0)));
    }
    int l = (int32_t)W(1), t = (int32_t)W(2), r = l + 4096, b = t + 4096;
    uint32_t fmt = 0x920;
    if (rc && (W(3) & 4)) { r = (int32_t)rt_r32(rc + 8); b = (int32_t)rt_r32(rc + 12); fmt = 0x820; }
    if (W(5)) gdi_draw_text(W(0), w32_str(W(5)), (int)W(6), &l, &t, &r, &b, fmt);
    return 1;
}

/* a DIB (BITMAPINFO at bi, bits) pixel at column x, memory row y (as stored) as ARGB */
static uint32_t dib_px(uint32_t bi, uint32_t bits, int x, int y)
{
    int w = (int32_t)rt_r32(bi + 4);
    int bpp = rt_r16(bi + 14);
    uint32_t hs = rt_r32(bi);
    uint32_t stride = (uint32_t)(((w * bpp + 31) / 32) * 4);
    uint32_t row = bits + (uint32_t)y * stride;
    uint32_t pal = bi + hs;
    uint32_t idx;
    switch (bpp) {
    case 1: idx = rt_r8(row + (uint32_t)x / 8) >> (7 - x % 8) & 1; break;
    case 4: idx = rt_r8(row + (uint32_t)x / 2) >> (x % 2 ? 0 : 4) & 15; break;
    case 8: idx = rt_r8(row + (uint32_t)x); break;
    case 16: {
        uint32_t v = rt_r16(row + 2u * (uint32_t)x);
        uint32_t r5 = v >> 10 & 31, g5 = v >> 5 & 31, b5 = v & 31;
        return 0xff000000u | (r5 * 255 / 31) << 16 | (g5 * 255 / 31) << 8 | b5 * 255 / 31;
    }
    case 24: {
        uint32_t q = row + 3u * (uint32_t)x;
        return 0xff000000u | (uint32_t)rt_r8(q + 2) << 16 | (uint32_t)rt_r8(q + 1) << 8 | rt_r8(q);
    }
    case 32: return 0xff000000u | (rt_r32(row + 4u * (uint32_t)x) & 0xffffff);
    default: return 0xff000000u;
    }
    uint32_t q = rt_r32(pal + 4 * idx);   /* RGBQUAD: blue, green, red */
    return 0xff000000u | (q & 0xffffff);
}

/* StretchDIBits(hdc, xd, yd, wd, hd, xs, ys, ws, hs, bits, info, usage, rop) */
uint32_t w32gdi_StretchDIBits(const uint32_t* a, int n)
{
    GObj* d = dc(W(0));
    DlgPic* p = surf(d);
    int32_t hs = (int32_t)W(8);
    uint32_t bi = W(10), bits = W(9);
    if (!p || !bi || !bits) return (uint32_t)hs;
    int xd = (int32_t)W(1), yd = (int32_t)W(2), wd = (int32_t)W(3), hd = (int32_t)W(4);
    int xs = (int32_t)W(5), ys = (int32_t)W(6), ws = (int32_t)W(7);
    int bw = (int32_t)rt_r32(bi + 4), bh = (int32_t)rt_r32(bi + 8);
    int topdown = bh < 0;
    if (topdown) bh = -bh;
    if (wd < 0) wd = -wd;
    if (hd < 0) hd = -hd;
    if (ws < 0) ws = -ws;
    if (hs < 0) hs = -hs;
    if (!wd || !hd || !ws || !hs) return (uint32_t)hs;
    for (int dy = 0; dy < hd; dy++) {
        int y = yd + dy + d->oy;
        if (y < 0 || y >= p->h) continue;
        /* bottom up: ys counts from the bottom row; memory row 0 is the bottom */
        int sy = topdown ? ys + dy * hs / hd : ys + (hd - 1 - dy) * hs / hd;
        if (sy < 0 || sy >= bh) continue;
        for (int dx = 0; dx < wd; dx++) {
            int x = xd + dx + d->ox;
            if (x < 0 || x >= p->w) continue;
            int sx = xs + dx * ws / wd;
            if (sx < 0 || sx >= bw) continue;
            p->px[y * p->w + x] = dib_px(bi, bits, sx, sy);
        }
    }
    return (uint32_t)hs;
}

/* BitBlt(dst, x, y, w, h, src, xs, ys, rop) */
uint32_t w32gdi_BitBlt(const uint32_t* a, int n)
{
    GObj* d = dc(W(0));
    DlgPic* p = surf(d);
    if (!p) return 1;
    int x = (int32_t)W(1), y = (int32_t)W(2), w = (int32_t)W(3), h = (int32_t)W(4);
    uint32_t rop = W(8);
    if (rop == 0x00000042u || rop == 0x00ff0062u) {   /* BLACKNESS, WHITENESS */
        gdi_fill(W(0), x, y, x + w, y + h, rop == 0x42 ? 0 : 0xffffff);
        return 1;
    }
    GObj* s = dc(W(5));
    DlgPic* sp = surf(s);
    if (!sp) return 1;
    int xs = (int32_t)W(6) + s->ox, ys = (int32_t)W(7) + s->oy;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int sx = xs + i, sy = ys + j, dx = x + i + d->ox, dy = y + j + d->oy;
            if (sx < 0 || sy < 0 || sx >= sp->w || sy >= sp->h || dx < 0 || dy < 0 || dx >= p->w || dy >= p->h) continue;
            uint32_t c = sp->px[sy * sp->w + sx];
            if (rop == 0x008800c6u) c &= p->px[dy * p->w + dx] | 0xff000000u;        /* SRCAND */
            else if (rop == 0x00ee0086u) c |= p->px[dy * p->w + dx];                 /* SRCPAINT */
            else if (rop == 0x00660046u) c = (c ^ p->px[dy * p->w + dx]) | 0xff000000u;   /* SRCINVERT */
            p->px[dy * p->w + dx] = c | 0xff000000u;
        }
    return 1;
}

/* LoadBitmapA(hinst, name): a DIB resource (RT_BITMAP 2) as a host bitmap */
uint32_t w32dlg_LoadBitmapA(const uint32_t* a, int n)
{
    uint32_t de = w32_find_resource(2, W(1));
    if (!de) return 0;
    uint32_t bi = 0x00400000u + rt_r32(de);
    int bw = (int32_t)rt_r32(bi + 4), bh = (int32_t)rt_r32(bi + 8);
    int topdown = bh < 0;
    if (topdown) bh = -bh;
    if (bw <= 0 || bh <= 0 || bw > 4096 || bh > 4096) return 0;
    int bpp = rt_r16(bi + 14);
    uint32_t ncol = rt_r32(bi + 32);
    if (!ncol && bpp <= 8) ncol = 1u << bpp;
    uint32_t bits = bi + rt_r32(bi) + 4 * ncol;
    GObj* o = alloc_obj(G_BITMAP);
    o->bmp.w = bw;
    o->bmp.h = bh;
    o->bmp.px = malloc(sizeof(uint32_t) * (size_t)bw * (size_t)bh);
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) o->bmp.px[y * bw + x] = dib_px(bi, bits, x, topdown ? y : bh - 1 - y);
    return make(o);
}

/* ---- USER32 painting on DCs ---- */

/* FillRect(hdc, rect, brush) */
uint32_t w32dlg_FillRect(const uint32_t* a, int n)
{
    int ok;
    uint32_t c = gdi_brush_color(W(2), &ok);
    uint32_t r = W(1);
    if (ok && r) gdi_fill(W(0), (int32_t)rt_r32(r), (int32_t)rt_r32(r + 4), (int32_t)rt_r32(r + 8), (int32_t)rt_r32(r + 12), c);
    return 1;
}
/* FrameRect(hdc, rect, brush): a one pixel frame */
uint32_t w32dlg_FrameRect(const uint32_t* a, int n)
{
    int ok;
    uint32_t c = gdi_brush_color(W(2), &ok);
    uint32_t r = W(1);
    if (ok && r) gdi_frame(W(0), (int32_t)rt_r32(r), (int32_t)rt_r32(r + 4), (int32_t)rt_r32(r + 8), (int32_t)rt_r32(r + 12), c);
    return 1;
}
/* DrawTextA(hdc, text, count, rect, format): the text's height */
uint32_t w32dlg_DrawTextA(const uint32_t* a, int n)
{
    uint32_t rc = W(3);
    if (!rc || !W(1)) return 0;
    int l = (int32_t)rt_r32(rc), t = (int32_t)rt_r32(rc + 4), r = (int32_t)rt_r32(rc + 8), b = (int32_t)rt_r32(rc + 12);
    int h = gdi_draw_text(W(0), w32_str(W(1)), (int32_t)W(2), &l, &t, &r, &b, W(4));
    if (W(4) & 0x400) { rt_w32(rc + 8, (uint32_t)r); rt_w32(rc + 12, (uint32_t)b); }
    return (uint32_t)h;
}
