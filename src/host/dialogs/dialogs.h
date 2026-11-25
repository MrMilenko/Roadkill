/*
 * dialogs.h: the Win32 dialog boxes, the standard controls and the GDI they draw with, on SDL
 * (port code, not from the original). The game builds its setup screens from them: the audio
 * screen (owner drawn buttons, a combo box, track bars on the main window), the input screens
 * (owner drawn buttons; the "Input Configuration" key capture dialog, template 106), the network
 * screen (list box, edit fields) and a debug window (template 105).
 *
 * Windows: the records stay in win32_user.c (HWND, style, rectangle, parent, procedure); this
 * module gives the system classes (Button, Static, Edit, ListBox, ComboBox, msctls_trackbar32,
 * SysListView32, ScrollBar, the dialog class #32770) host window procedures at fake game
 * addresses, so SendMessageA, subclassing (SetWindowLongA GWL_WNDPROC + CallWindowProcA) and the
 * game's own procedures all work as on Windows. A control's state (text, check, items, range)
 * lives here, keyed by HWND.
 *
 * Drawing: every visible control or dialog paints into its own ARGB picture (owner drawn ones
 * through WM_DRAWITEM to the parent, the others with WM_CTLCOLOR* colours) when it changed or
 * was invalidated; the pictures are put together into one overlay in the game's 640 x 480
 * client coordinates and drawn over each presented frame (w32dlg_overlay, called by dx_sdl.c),
 * so they scale with the window. While the game presents nothing (a modal dialog's loop, a
 * screen waiting for input) the tick re-presents the last frame with the overlay.
 *
 * Dialogs: DIALOG / DIALOGEX templates from the EXE's resources (RT_DIALOG 5), dialog units from
 * the template's font (MS Sans Serif 8: 6 x 13 base units). DialogBoxParamA runs its own modal
 * loop (WM_ENTERIDLE to the owner when the queue is empty, IsDialogMessage, owner disabled),
 * CreateDialogParamA makes a modeless one. IsDialogMessageA: Tab / Shift+Tab, arrows in a group,
 * Enter (the default button), Esc (IDCANCEL). A game controller drives a modal dialog (d-pad =
 * arrows, A = Enter, B = Esc).
 *
 * Text: the game's own 8 x 8 font, VGA8X8.FNT, read from the data folder at run time (scaled to
 * the asked font height, spaced proportionally); without it text is drawn as boxes.
 */
#ifndef TM2_HOST_DIALOGS_H
#define TM2_HOST_DIALOGS_H

#include <stdint.h>

#include "rt.h"

struct SDL_Renderer;

/* ---- hooks for win32_user.c ---- */

/* the host window procedure (a fake game address) of a system class name, 0 if not one */
uint32_t w32dlg_class_proc(const char* cls);
/* a mouse message for window h at client (x, y) of the main window: the window that gets it
 * (the control under the cursor, the one holding the capture) with x, y made its client
 * coordinates; 0 = drop it (a disabled window, outside a modal dialog) */
uint32_t w32dlg_mouse_target(uint32_t h, int32_t* x, int32_t* y, int down);
/* the main thread's PeekMessageA: repaint what changed, re-present when the game does not */
void w32dlg_tick(void);
/* InvalidateRect / UpdateWindow on a window */
void w32dlg_invalidate(uint32_t hwnd);
/* the USER32 dialog imports */
uint32_t w32dlg_DialogBoxParamA(const uint32_t* a, int n);
uint32_t w32dlg_CreateDialogParamA(const uint32_t* a, int n);
uint32_t w32dlg_EndDialog(const uint32_t* a, int n);
uint32_t w32dlg_IsDialogMessageA(const uint32_t* a, int n);
/* painting imports: GetDC, ReleaseDC, BeginPaint, EndPaint, FillRect, FrameRect, DrawTextA */
uint32_t w32dlg_GetDC(const uint32_t* a, int n);
uint32_t w32dlg_ReleaseDC(const uint32_t* a, int n);
uint32_t w32dlg_BeginPaint(const uint32_t* a, int n);
uint32_t w32dlg_EndPaint(const uint32_t* a, int n);
uint32_t w32dlg_FillRect(const uint32_t* a, int n);
uint32_t w32dlg_FrameRect(const uint32_t* a, int n);
uint32_t w32dlg_DrawTextA(const uint32_t* a, int n);
uint32_t w32dlg_LoadBitmapA(const uint32_t* a, int n);

/* ---- hooks for win32_gdi.c: the GDI32 imports on host DCs and objects ---- */
uint32_t w32gdi_BitBlt(const uint32_t* a, int n);
uint32_t w32gdi_CreateCompatibleDC(const uint32_t* a, int n);
uint32_t w32gdi_CreateFontA(const uint32_t* a, int n);
uint32_t w32gdi_CreateSolidBrush(const uint32_t* a, int n);
uint32_t w32gdi_DeleteDC(const uint32_t* a, int n);
uint32_t w32gdi_DeleteObject(const uint32_t* a, int n);
uint32_t w32gdi_ExtTextOutA(const uint32_t* a, int n);
uint32_t w32gdi_GetObjectA(const uint32_t* a, int n);
uint32_t w32gdi_GetStockObject(const uint32_t* a, int n);
uint32_t w32gdi_GetTextExtentPoint32A(const uint32_t* a, int n);
uint32_t w32gdi_GetTextMetricsA(const uint32_t* a, int n);
uint32_t w32gdi_SelectObject(const uint32_t* a, int n);
uint32_t w32gdi_SetBkColor(const uint32_t* a, int n);
uint32_t w32gdi_SetBkMode(const uint32_t* a, int n);
uint32_t w32gdi_SetTextColor(const uint32_t* a, int n);
uint32_t w32gdi_StretchDIBits(const uint32_t* a, int n);
uint32_t w32gdi_TextOutA(const uint32_t* a, int n);

/* ---- the display (dx_sdl.c): draws the overlay over the frame just copied to the renderer
 * (presenting: 1 from a present, 0 from a screenshot's redraw). The tick re-presents the last
 * frame through dx_sdl_redraw() (clear, copy the last frame, overlay, present), which dx_sdl.c
 * defines; dialog.c has a weak empty one so the build links without it ---- */
void w32dlg_overlay(struct SDL_Renderer* r, int presenting);
void dx_sdl_redraw(void);

/* ======== inside the module (src/host/dialogs) ======== */

/* an ARGB picture (0xAARRGGBB; alpha 0 = see through) */
typedef struct { uint32_t* px; int w, h; } DlgPic;

/* GDI (gdi.c) */
uint32_t gdi_dc_for(DlgPic* pic, int ox, int oy);   /* a DC drawing into pic, origin offset */
void gdi_dc_free(uint32_t hdc);
int gdi_is_dc(uint32_t hdc);
uint32_t gdi_brush_color(uint32_t brush, int* ok);   /* a brush handle or (COLOR_x + 1): its 0xBBGGRR */
uint32_t gdi_dc_text_color(uint32_t hdc);
uint32_t gdi_dc_bk_color(uint32_t hdc);
int gdi_dc_bk_mode(uint32_t hdc);
void gdi_dc_reset_colors(uint32_t hdc, uint32_t text, uint32_t bk, int mode);
uint32_t gdi_dc_font(uint32_t hdc);
void gdi_dc_select_font(uint32_t hdc, uint32_t font);
uint32_t gdi_default_font(void);   /* the dialog font (MS Sans Serif 8) */
/* drawing in DC coordinates; colors 0xBBGGRR (Win32 COLORREF) */
void gdi_fill(uint32_t hdc, int l, int t, int r, int b, uint32_t color);
void gdi_frame(uint32_t hdc, int l, int t, int r, int b, uint32_t color);
void gdi_line_h(uint32_t hdc, int x0, int x1, int y, uint32_t color);
void gdi_line_v(uint32_t hdc, int x, int y0, int y1, uint32_t color);
/* text with the DC's font and colors; DrawText flags (DT_*); returns the height drawn; with
 * DT_CALCRECT (0x400) sets *r / *b and draws nothing */
int gdi_draw_text(uint32_t hdc, const char* s, int len, int* l, int* t, int* r, int* b, uint32_t fmt);
int gdi_text_width(uint32_t font, const char* s, int len);
int gdi_font_height(uint32_t font);   /* a line's height */
void gdi_edge(uint32_t hdc, int l, int t, int r, int b, int sunken);   /* 3D border, 2 pixels */

/* windows (dialog.c) */
typedef struct { uint32_t hwnd, parent, id, style, exstyle; int x, y, w, h; } DlgInfo;
int dlg_info(uint32_t h, DlgInfo* o);
uint32_t dlg_send(uint32_t h, uint32_t msg, uint32_t wp, uint32_t lp);
uint32_t dlg_import(const char* name, const uint32_t* v, int n);   /* call a USER32 import */
uint32_t dlg_focus(void);
void dlg_set_focus(uint32_t h);
void dlg_capture(uint32_t h);   /* SetCapture / ReleaseCapture (0) */
uint32_t dlg_captured(void);
void dlg_notify(uint32_t ctl, uint32_t code);   /* WM_COMMAND (id, code) to the parent */
uint32_t dlg_ctlcolor(uint32_t ctl, uint32_t msg, uint32_t hdc, uint32_t defbrush_color, uint32_t* fill);
void dlg_radio_group(uint32_t h);   /* clears the other auto radio buttons of h's group */
int dlg_is_default_button(uint32_t h);

/* controls (controls.c) */
enum { K_NONE, K_BUTTON, K_STATIC, K_EDIT, K_LISTBOX, K_COMBO, K_TRACKBAR, K_LISTVIEW, K_SCROLLBAR, K_DIALOG };
typedef struct { char* s; uint32_t data; char* sub[4]; uint32_t state, image; } DlgItem;
typedef struct DlgWin {
    uint32_t hwnd;
    int kind;
    uint32_t order;                 /* creation order (z order among siblings) */
    char* text;
    uint32_t font;
    int check, pushed, tracking, hot;
    DlgItem* items;
    int nitems, cap, sel, top, dropped;
    int tmin, tmax, tpos, tline, tpage, tfreq, tsel0, tsel1;
    int caret, limit, anchor;
    uint32_t bk, fg;                /* list view colors (CLR_NONE 0xffffffff: default) */
    DlgPic pic;
    uint32_t sig;
    int dirty;
    /* dialogs */
    uint32_t dlgproc, owner, lparam, prev_focus;
    int modal, ended, defid;
    int32_t result;
    uint32_t idle_sent;
} DlgWin;
DlgWin* dlg_win(uint32_t h);
void dlg_dirty(uint32_t h);
/* a control's window procedure; returns 1 and *res when it handled msg, else 0 (DefWindowProc) */
int ctl_proc(DlgWin* w, uint32_t msg, uint32_t wp, uint32_t lp, uint32_t* res);
void ctl_paint(DlgWin* w, uint32_t hdc, int cw, int ch);   /* draws the control in its client area */
int ctl_dlgcode(DlgWin* w, uint32_t vk);
void ctl_free(DlgWin* w);
int ctl_hit_height(DlgWin* w, int h);   /* the height a click counts in (a closed combo box) */
void ctl_paint_dropdown(DlgWin* w, uint32_t hdc, int cw, int listh);   /* an open combo box's list */
int ctl_dropdown_height(DlgWin* w);

#endif /* TM2_HOST_DIALOGS_H */
