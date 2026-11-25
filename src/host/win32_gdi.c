/*
 * win32_gdi.c: the GDI32 and COMCTL32 imports, port code. The game draws through DirectDraw;
 * GDI only serves its windows' chrome, the text it measures and dialog bitmaps.
 *
 * DCs, fonts, brushes, bitmaps and the drawing calls are src/host/dialogs/gdi.c (the controls'
 * pictures). STUBS: palettes and image lists are fake handles. GetDeviceCaps reports a 640 x 480
 * screen of TM2_BPP (default 16) bits per pixel, no palette.
 */
#include "win32_host.h"
#include "host/settings/settings.h"
#include "host/dialogs/dialogs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t obj(void) { return w32_handle_new(H_GDI, NULL); }

static uint32_t g_new(const uint32_t* a, int n) { (void)a; (void)n; return obj(); }
static uint32_t g_delete(const uint32_t* a, int n) { if (w32_handle_type(W(0)) == H_GDI) w32_handle_free(W(0)); return 1; }
static uint32_t g_zero(const uint32_t* a, int n) { (void)a; (void)n; return 0; }

static uint32_t stock[32];
static uint32_t g_GetStockObject(const uint32_t* a, int n)
{
    uint32_t i = W(0) & 31;
    if (!stock[i]) stock[i] = obj();
    return stock[i];
}
static uint32_t g_SelectPalette(const uint32_t* a, int n) { (void)a; (void)n; return g_GetStockObject((const uint32_t[]){15}, 1); }

static uint32_t g_GetDeviceCaps(const uint32_t* a, int n)
{

    switch (W(1)) {
    case 8: return 640;                        /* HORZRES */
    case 10: return 480;                       /* VERTRES */
    case 12: return (uint32_t)settings_int("debug.bpp", 16);   /* BITSPIXEL */
    case 14: return 1;                         /* PLANES */
    case 24: return 0xffffffffu;               /* NUMCOLORS: more than 256 */
    case 88: case 90: return 96;               /* LOGPIXELSX / Y */
    case 104: return 0;                        /* SIZEPALETTE */
    default: return 0;
    }
}

static uint32_t g_GetTextFaceA(const uint32_t* a, int n)
{
    if (W(2)) w32_put_str(W(2), "Fixedsys", W(1));
    return 9;
}

/* COMCTL32 */
static uint32_t c_ImageList_Create(const uint32_t* a, int n) { (void)a; (void)n; return obj(); }
static uint32_t c_ImageList_Add(const uint32_t* a, int n) { (void)a; (void)n; return 0; }
static uint32_t c_ImageList_Destroy(const uint32_t* a, int n) { return g_delete(a, n); }

void rt_bind_win32_gdi(void)
{
    rt_bind_import("GDI32.dll!BitBlt", w32gdi_BitBlt);
    rt_bind_import("GDI32.dll!CreateCompatibleDC", w32gdi_CreateCompatibleDC);
    rt_bind_import("GDI32.dll!CreateFontA", w32gdi_CreateFontA);
    rt_bind_import("GDI32.dll!CreatePalette", g_new);
    rt_bind_import("GDI32.dll!CreateSolidBrush", w32gdi_CreateSolidBrush);
    rt_bind_import("GDI32.dll!DeleteDC", w32gdi_DeleteDC);
    rt_bind_import("GDI32.dll!DeleteObject", w32gdi_DeleteObject);
    rt_bind_import("GDI32.dll!ExtTextOutA", w32gdi_ExtTextOutA);
    rt_bind_import("GDI32.dll!GetDeviceCaps", g_GetDeviceCaps);
    rt_bind_import("GDI32.dll!GetObjectA", w32gdi_GetObjectA);
    rt_bind_import("GDI32.dll!GetStockObject", w32gdi_GetStockObject);
    rt_bind_import("GDI32.dll!GetTextExtentPoint32A", w32gdi_GetTextExtentPoint32A);
    rt_bind_import("GDI32.dll!GetTextFaceA", g_GetTextFaceA);
    rt_bind_import("GDI32.dll!GetTextMetricsA", w32gdi_GetTextMetricsA);
    rt_bind_import("GDI32.dll!RealizePalette", g_zero);
    rt_bind_import("GDI32.dll!SelectObject", w32gdi_SelectObject);
    rt_bind_import("GDI32.dll!SelectPalette", g_SelectPalette);
    rt_bind_import("GDI32.dll!SetBkColor", w32gdi_SetBkColor);
    rt_bind_import("GDI32.dll!SetBkMode", w32gdi_SetBkMode);
    rt_bind_import("GDI32.dll!SetTextColor", w32gdi_SetTextColor);
    rt_bind_import("GDI32.dll!StretchDIBits", w32gdi_StretchDIBits);
    rt_bind_import("GDI32.dll!TextOutA", w32gdi_TextOutA);
    /* InitCommonControls, imported by ordinal: rt.c lists only named imports, so bind the slot
     * (tools/rt/imports.tsv) unless the runtime learns ordinals ("COMCTL32.dll!#17") */
    if (rt_import_slot("COMCTL32.dll!#17")) rt_bind_import("COMCTL32.dll!#17", g_zero);
    else rt_bind(0x00c907f4u, g_zero, "COMCTL32.dll!#17");
    rt_bind_import("COMCTL32.dll!ImageList_Add", c_ImageList_Add);
    rt_bind_import("COMCTL32.dll!ImageList_Create", c_ImageList_Create);
    rt_bind_import("COMCTL32.dll!ImageList_Destroy", c_ImageList_Destroy);
}

RT_BIND_AREA(win32_gdi)
