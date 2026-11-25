/*
 * mt_pause.h: the in game pause menu, ported from TM2.EXE 0x0046b604 (old name
 * gameloop_pause_menu; tools/match pairs it with PS1 humanCarReadInput, which is a wrong pair:
 * this is the PC pause menu with its screen size, display mode and detail pages).
 *
 * Globals (pointers in MtPause):
 *   0x00510a78 u8 pause pressed, 0x00510a79 up, 0x00510a7a down, 0x00510a7b down (second
 *   key), 0x00510a7c confirm
 *   0x00510dc4 i32 menu open, 0x00510dc8 i32 pause key released, 0x00510dcc i32 may close,
 *   0x00510dd0 i32 input consumed (wait for release), 0x00510dac i32 page, 0x00510db0 i32
 *   cursor, 0x00510db4 / 0x00510db8 i32 cursor min / max, 0x00510da8 i32 (page 5 back item),
 *   0x00510a84 i32 detail level chosen, 0x00c84f70 i32, 0x00624ec7 u8 arena
 * Pages: 0 main (1 screen size, 2 display mode, 3 detail with cursor 3, 4 quit), 1 screen size
 * (bigger, smaller, 0x004958d8 based, full, back), 2 display mode (next, previous, first, list:
 * page 5, back), 3 detail (low / medium / high, back), 4 quit (no, yes: event 2), 5 mode list,
 * 6 event 2.
 *
 * Verified against the original under emulation (tools/emu/check_match_pause.sh).
 */
#ifndef MT_PAUSE_H
#define MT_PAUSE_H

#include <stdint.h>

typedef uint32_t (*MtPauseFn)(void* user, uint32_t eax, uint32_t edx, uint32_t ebx, const uint32_t* stack, int nstack);

typedef struct {
    const uint8_t* input;       /* 0x00510a78 .. 0x00510a7c */
    int32_t* open;              /* 0x00510dc4 */
    int32_t* released;          /* 0x00510dc8 */
    int32_t* may_close;         /* 0x00510dcc */
    int32_t* consumed;          /* 0x00510dd0 */
    int32_t* page;              /* 0x00510dac */
    int32_t* cursor;            /* 0x00510db0 */
    const int32_t* cursor_min;  /* 0x00510db4 */
    const int32_t* cursor_max;  /* 0x00510db8 */
    const int32_t* back_item;   /* 0x00510da8 */
    int32_t* detail;            /* 0x00510a84 */
    const int32_t* v_c84f70;    /* 0x00c84f70 */
    const uint8_t* arena;       /* 0x00624ec7 */
    MtPauseFn is_singleplayer;  /* 0x0045c348 () al */
    MtPauseFn view_grow;        /* 0x0049433c (0, +-8) */
    MtPauseFn screen_width;     /* 0x004958d8 () */
    MtPauseFn view_set;         /* 0x0049437c (0, v) */
    MtPauseFn mode_count;       /* 0x004b454c () */
    MtPauseFn mode_get;         /* 0x004b4554 (0x00c84840) */
    MtPauseFn mode_set;         /* 0x004b4558 (0x00c84840, i) */
    MtPauseFn display_ok;       /* 0x004958ac () al */
    MtPauseFn set_step;         /* 0x00470320 (0xc, 0, level) */
    MtPauseFn fx_48e97c;        /* 0x0048e97c (v) */
    MtPauseFn fx_4ad21c;        /* 0x004ad21c (v) */
    MtPauseFn fx_48e918;        /* 0x0048e918 (v) */
    MtPauseFn fx_48e920;        /* 0x0048e920 (stack: float bits) */
    MtPauseFn schedule_event;   /* 0x0046ca68 rtScheduleEvent (2, 0) */
    void* user;
} MtPause;

void mt_pause_menu(const MtPause* p);

/* 0x0046be34 (old name gameloop_timer_update): the pause menu keys and hot keys, every frame:
 * menu inputs 0x00510a78.. from key_state(0x14 / 0 / 1 / 4 / 0x12, 0); scan codes 0x4e, 0x0d grow
 * the view by 8, 0x4a, 0x0c shrink it; 0xc9 / 0xd1 next / previous display mode; 2 / 3 / 4 the
 * detail levels as on the detail page (without moving the cursor); when the menu is open,
 * input_update. Returns 0. */
typedef struct {
    const MtPause* menu;        /* callbacks and globals shared with the menu */
    uint8_t* input;             /* 0x00510a78 .. 0x00510a7c (written) */
    MtPauseFn key_state;        /* 0x004936ac (id, 0) al */
    MtPauseFn key_pressed;      /* 0x0049eb5c (scan code) al */
    MtPauseFn input_update;     /* 0x0049364c () */
} MtPauseKeys;
int32_t mt_pause_keys(const MtPauseKeys* k);

/* 0x0046c5fc: detail level high as on the detail page (cursor untouched), returns 0. */
int32_t mt_pause_detail_high(const MtPause* p);

/* 0x0046cc68 (old name render_scene): the blinking banner: single player: text 0x004e1cb8 at
 * y = (h >> 1) - (w >> 1) + 0xf (h = 0x004958e4(), w = 0x004937e0(0)); else, only when the menu is
 * open and not multiplayer: text 0x004e1cbd at y = h - 0x1e. A 16 bit frame counter (0x00510e6c)
 * toggles the visible flag (i16 0x004e7c64) every 20 frames; visible: text_begin(6, 0x24, 0x96,
 * 0x24), text_draw(6, (screen width >> 1) + 7, y, text), text_end(6). */
typedef struct {
    uint16_t* timer;            /* 0x00510e6c */
    int16_t* visible;           /* 0x004e7c64 */
    MtPauseFn is_singleplayer;  /* 0x0045c348 () al */
    MtPauseFn menu_open;        /* 0x0046be2c () */
    MtPauseFn is_multiplayer;   /* 0x004632b0 () al */
    MtPauseFn text_height;      /* 0x004937e0 (0) */
    MtPauseFn screen_height;    /* 0x004958e4 () */
    MtPauseFn screen_width;     /* 0x004958d8 () */
    MtPauseFn text_begin;       /* 0x00452d28 (6, 0x24, 0x96; stack[0] = ecx 0x24) */
    MtPauseFn text_draw;        /* 0x00451f28 (6, x, y; stack[0] = ecx text) */
    MtPauseFn text_end;         /* 0x00452d48 (6) */
    void* user;
} MtPauseBanner;
void mt_pause_banner(const MtPauseBanner* b);

/* 0x0046c3f4: applies the detail level 0x00510a84 (0 / 1 / 2, else nothing) as the detail page
 * does, cursor untouched. */
void mt_detail_apply(const MtPause* p);

/* Display settings in the registry (HKEY_LOCAL_MACHINE 0x80000002): the view size (8 bytes at
 * 0x00510a80: +0 size step, +4 detail level 0x00510a84) under value name 0x004e7bac, the display
 * mode under 0x004e1b9a (read) / 0x004e1bab (written). The registry helper keeps a 4 byte key
 * handle on the caller's stack; callbacks get host pointers to it (and to the size / value
 * locals right after it) and to the view size globals. */
typedef uint32_t (*MtRegFn)(void* user, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx);
typedef struct {
    const MtPause* menu;        /* detail and display mode callbacks, globals */
    int32_t* view;              /* 0x00510a80 (i32 size step, i32 detail = 0x00510a84) */
    const uint32_t* mode_index; /* 0x00c84840 (the display object's current mode) */
    MtRegFn reg_init;           /* 0x004ac4b0 (key, 0) */
    MtRegFn reg_open;           /* 0x004ac528 (key, 0x80000002, 0): not 0 = failed */
    MtRegFn reg_read_bin;       /* 0x004ac5e4 (key, name, dest, size ptr): not 0 = failed */
    MtRegFn reg_read_dword;     /* 0x004ac5bc (key, name, dest): not 0 = failed */
    MtRegFn reg_write_bin;      /* 0x004ac65c (key, name, src, 8) */
    MtRegFn reg_write_dword;    /* 0x004ac614 (key, name, value) */
    MtRegFn reg_close;          /* 0x004ac590 (key) */
    MtRegFn view_get;           /* 0x00494360 (0, dest, 0) */
} MtSettings;
/* 0x0046c17c (old name aiInputFinalize: the PS1 pair is wrong): reads the settings (defaults:
 * view 0, detail 2; mode count - 1, clamped below the mode count), sets the display mode, the
 * view size and the detail level. Returns 1 when the key does not open, else 0. */
int32_t mt_settings_load(const MtSettings* s);
/* 0x0046c384 (old name render_cleanup_a): writes the view size (read back through view_get) and
 * the current display mode. Returns 1 when the key does not open, else 0. */
int32_t mt_settings_save(const MtSettings* s);

/* 0x0046b354 (PS1 pair taskMgrStopAll is wrong): pause menu setup: back item = display mode
 * count; mode names 0x00510a88 + 0x50 k = wsprintf("mode %ix%i ..." 0x004e1adc, width, height of
 * mode k (16 byte entries at 0x00c848e4)); line height = the font's text height of the first
 * name (0x004b3a90); cursor max 3, menu x 0x14, page / cursor / min / open / released / consumed
 * 0, may close 1, menu y = (int)(height * 1.5 + 15.0), truncated (Watcom's 0x004979a2 rounds
 * toward zero before the fistp; out of int32 range gives 0x80000000). DECOMPILER TRAP: the
 * "result" of 0x004979a2 stored to 0x00510dcc is just the 1 the compiler left in eax. The x87
 * sum is exact at 53 / 64 bit precision for every int32 height; under a 24 bit precision control
 * word (what Direct3D sets by default) heights above 2^23 would round differently. */
typedef struct {
    const MtPause* menu;        /* mode_count; page, cursor, cursor_min/max, open, released, may_close, consumed */
    int32_t* back_item;         /* 0x00510da8 */
    int32_t* menu_x;            /* 0x00510dbc */
    int32_t* menu_y;            /* 0x00510dc0 */
    int32_t* line_h;            /* 0x00510e68 */
    const uint32_t* modes;      /* 0x00c848e4 */
    int32_t* cursor_min;        /* 0x00510db4 (written here) */
    int32_t* cursor_max;        /* 0x00510db8 (written here) */
    uint32_t (*wsprintf_fn)(void* user, uint32_t dest, uint32_t fmt, uint32_t a, uint32_t b); /* import 0x00c90658 */
    uint32_t (*text_height)(void* user, uint32_t font, uint32_t text);                         /* 0x004b3a90 */
} MtPauseInit;
void mt_pause_init(const MtPauseInit* s);

/* Drawing (0x0046bbec, 0x0046b428, 0x0046b4d4). Text goes through the menu font object 0x00510dd4;
 * strings and item tables are handles (the original's addresses): titles 0x004e1b04, 0x004e1b15,
 * 0x004e1b26, 0x004e1b39, 0x004e1b48, 0x004e1b57, 0x004e1b6e (+ 0x004e1b86), item tables
 * {u32 label, u32 value text or 0} at 0x004e7bbc, 0x004e7bdc, 0x004e7c04, 0x004e7c2c,
 * 0x004e7c4c, 0x004e7c5c; mode names 0x00510a88 + 0x50 k, back label *0x004e7c24.
 *   0x00510dbc i32 menu x, 0x00510dc0 i32 menu y, 0x00510e68 i32 line height */
typedef struct {
    int32_t* open;              /* 0x00510dc4 */
    int32_t* page;              /* 0x00510dac */
    int32_t* cursor;            /* 0x00510db0 */
    int32_t* cursor_min;        /* 0x00510db4 */
    int32_t* cursor_max;        /* 0x00510db8 */
    const int32_t* back_item;   /* 0x00510da8 */
    const int32_t* menu_x;      /* 0x00510dbc */
    const int32_t* menu_y;      /* 0x00510dc0 */
    const int32_t* line_h;      /* 0x00510e68 */
    const uint32_t* back_label; /* 0x004e7c24 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    MtPauseFn font_scale;       /* 0x004b3bac (font, sx, sy) */
    MtPauseFn font_color;       /* 0x004b3b84 (font, r, g; ecx b) : uses the stack slot for b */
    MtPauseFn font_draw;        /* 0x004b31d0 (font, text, x; ecx y) */
    MtPauseFn font_width;       /* 0x004b3a44 (font, text) */
    MtPauseFn screen_width;     /* 0x004958d8 () */
    void* user;
} MtPauseDraw;
/* MtPauseFn has three register slots; for font_color and font_draw the fourth register (ecx) is
 * passed as stack[0] (nstack 1). */

/* 0x0046bbec: when open: font scale 1, 1, colour (0xff, 0x40, 0x40), then per page (0..6) the
 * cursor range, title at (0x14, 0xf) and its item list (0x0046b428; page 5: 0x0046b4d4). */
void mt_pause_draw(const MtPauseDraw* d);
/* 0x0046b428: items 0..cursor max of a table, from y = menu y + dy: yellow (0xff, 0xff, 0) at the
 * cursor, else white; label at menu x, value text right aligned at screen width - width - 0x14. */
void mt_pause_draw_items(const MtPauseDraw* d, uint32_t table, int32_t dy);
/* 0x0046b4d4: the display mode names (0 .. cursor max - 1) and the back label. */
void mt_pause_draw_modes(const MtPauseDraw* d);

#endif /* MT_PAUSE_H */
