/*
 * win32_gdi.c: the GDI32 and COMCTL32 imports, port code. The game draws through DirectDraw;
 * GDI only serves its windows' chrome, the text it measures and dialog bitmaps.
 *
 * STUBS: objects (DCs, fonts, brushes, palettes, image lists) are fake handles that draw
 * nothing: BitBlt, ExtTextOutA, TextOutA, StretchDIBits answer success without drawing. Text
 * metrics are those of an 8 x 16 fixed font. GetDeviceCaps reports a 640 x 480 screen of
 * TM2_BPP (default 16) bits per pixel, no palette.
 */
#include "win32_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t obj(void) { return w32_handle_new(H_GDI, NULL); }

static uint32_t g_new(const uint32_t* a, int n) { (void)a; (void)n; return obj(); }
static uint32_t g_delete(const uint32_t* a, int n) { if (w32_handle_type(W(0)) == H_GDI) w32_handle_free(W(0)); return 1; }
static uint32_t g_true(const uint32_t* a, int n) { (void)a; (void)n; return 1; }
static uint32_t g_zero(const uint32_t* a, int n) { (void)a; (void)n; return 0; }

static uint32_t stock[32];
static uint32_t g_GetStockObject(const uint32_t* a, int n)
{
    uint32_t i = W(0) & 31;
    if (!stock[i]) stock[i] = obj();
    return stock[i];
}
/* SelectObject(hdc, obj): the previous object of that kind; a fake that was never selected */
static uint32_t g_SelectObject(const uint32_t* a, int n) { (void)a; (void)n; return g_GetStockObject((const uint32_t[]){0}, 1); }
static uint32_t g_SelectPalette(const uint32_t* a, int n) { (void)a; (void)n; return g_GetStockObject((const uint32_t[]){15}, 1); }
static uint32_t bk_color = 0xffffff, text_color, bk_mode = 2;
static uint32_t g_SetBkColor(const uint32_t* a, int n) { uint32_t o = bk_color; bk_color = W(1); return o; }
static uint32_t g_SetTextColor(const uint32_t* a, int n) { uint32_t o = text_color; text_color = W(1); return o; }
static uint32_t g_SetBkMode(const uint32_t* a, int n) { uint32_t o = bk_mode; bk_mode = W(1); return o; }

static uint32_t g_GetDeviceCaps(const uint32_t* a, int n)
{
    const char* b = getenv("TM2_BPP");
    switch (W(1)) {
    case 8: return 640;                        /* HORZRES */
    case 10: return 480;                       /* VERTRES */
    case 12: return b && *b ? (uint32_t)atoi(b) : 16;   /* BITSPIXEL */
    case 14: return 1;                         /* PLANES */
    case 24: return 0xffffffffu;               /* NUMCOLORS: more than 256 */
    case 88: case 90: return 96;               /* LOGPIXELSX / Y */
    case 104: return 0;                        /* SIZEPALETTE */
    default: return 0;
    }
}

/* GetTextExtentPoint32A(hdc, text, count, &size) */
static uint32_t g_GetTextExtentPoint32A(const uint32_t* a, int n)
{
    rt_w32(W(3), 8u * W(2));
    rt_w32(W(3) + 4, 16);
    return 1;
}
/* GetTextMetricsA(hdc, &TEXTMETRICA) */
static uint32_t g_GetTextMetricsA(const uint32_t* a, int n)
{
    uint32_t t = W(1);
    memset(rt_ptr(t), 0, 56);
    rt_w32(t + 0, 16);    /* tmHeight */
    rt_w32(t + 4, 13);    /* tmAscent */
    rt_w32(t + 8, 3);     /* tmDescent */
    rt_w32(t + 20, 8);    /* tmAveCharWidth */
    rt_w32(t + 24, 8);    /* tmMaxCharWidth */
    rt_w32(t + 28, 400);  /* tmWeight */
    rt_w8(t + 45, 0x20);  /* tmFirstChar .. */
    rt_w8(t + 46, 0xff);
    rt_w8(t + 47, 0x20);
    rt_w8(t + 48, 0x20);
    rt_w8(t + 52, 0x31);  /* tmPitchAndFamily: fixed, TMPF_TRUETYPE no */
    return 1;
}
static uint32_t g_GetTextFaceA(const uint32_t* a, int n)
{
    if (W(2)) w32_put_str(W(2), "Fixedsys", W(1));
    return 9;
}
/* GetObjectA(obj, size, buf): zeros (a 0 x 0 bitmap / an empty LOGFONT) */
static uint32_t g_GetObjectA(const uint32_t* a, int n)
{
    if (W(2) && W(1) < 0x1000) memset(rt_ptr(W(2)), 0, W(1));
    return W(1);
}
/* StretchDIBits(hdc, x, y, w, h, sx, sy, sw, sh, bits, info, usage, rop): the scan lines */
static uint32_t g_StretchDIBits(const uint32_t* a, int n) { return W(8); }

/* COMCTL32 */
static uint32_t c_ImageList_Create(const uint32_t* a, int n) { (void)a; (void)n; return obj(); }
static uint32_t c_ImageList_Add(const uint32_t* a, int n) { (void)a; (void)n; return 0; }
static uint32_t c_ImageList_Destroy(const uint32_t* a, int n) { return g_delete(a, n); }

void rt_bind_win32_gdi(void)
{
    rt_bind_import("GDI32.dll!BitBlt", g_true);
    rt_bind_import("GDI32.dll!CreateCompatibleDC", g_new);
    rt_bind_import("GDI32.dll!CreateFontA", g_new);
    rt_bind_import("GDI32.dll!CreatePalette", g_new);
    rt_bind_import("GDI32.dll!CreateSolidBrush", g_new);
    rt_bind_import("GDI32.dll!DeleteDC", g_delete);
    rt_bind_import("GDI32.dll!DeleteObject", g_delete);
    rt_bind_import("GDI32.dll!ExtTextOutA", g_true);
    rt_bind_import("GDI32.dll!GetDeviceCaps", g_GetDeviceCaps);
    rt_bind_import("GDI32.dll!GetObjectA", g_GetObjectA);
    rt_bind_import("GDI32.dll!GetStockObject", g_GetStockObject);
    rt_bind_import("GDI32.dll!GetTextExtentPoint32A", g_GetTextExtentPoint32A);
    rt_bind_import("GDI32.dll!GetTextFaceA", g_GetTextFaceA);
    rt_bind_import("GDI32.dll!GetTextMetricsA", g_GetTextMetricsA);
    rt_bind_import("GDI32.dll!RealizePalette", g_zero);
    rt_bind_import("GDI32.dll!SelectObject", g_SelectObject);
    rt_bind_import("GDI32.dll!SelectPalette", g_SelectPalette);
    rt_bind_import("GDI32.dll!SetBkColor", g_SetBkColor);
    rt_bind_import("GDI32.dll!SetBkMode", g_SetBkMode);
    rt_bind_import("GDI32.dll!SetTextColor", g_SetTextColor);
    rt_bind_import("GDI32.dll!StretchDIBits", g_StretchDIBits);
    rt_bind_import("GDI32.dll!TextOutA", g_true);
    /* InitCommonControls, imported by ordinal: rt.c lists only named imports, so bind the slot
     * (tools/rt/imports.tsv) unless the runtime learns ordinals ("COMCTL32.dll!#17") */
    if (rt_import_slot("COMCTL32.dll!#17")) rt_bind_import("COMCTL32.dll!#17", g_zero);
    else rt_bind(0x00c907f4u, g_zero, "COMCTL32.dll!#17");
    rt_bind_import("COMCTL32.dll!ImageList_Add", c_ImageList_Add);
    rt_bind_import("COMCTL32.dll!ImageList_Create", c_ImageList_Create);
    rt_bind_import("COMCTL32.dll!ImageList_Destroy", c_ImageList_Destroy);
}

RT_BIND_AREA(win32_gdi)
