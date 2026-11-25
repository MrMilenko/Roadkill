/*
 * mt_inputcfg.h: the input configuration / car select shell screen's main loop and widget
 * teardown, ported from TM2.EXE 0x0045f2dc, 0x004602b0, 0x00460370.
 *
 * Widgets are objects (handles) whose +0x34 points at a method table; method 0 with 2 destroys.
 *   0x0050e108, 0x0050e10c, 0x0050e414, 0x0050e41c, 0x0050e420, 0x0050e418 screen widgets,
 *   0x0050e120 + 0x10 k and 0x0050e220 + 0x10 k (k = 0..15) the two car lists,
 *   0x0050e428 .. 0x0050e440 the car panel widgets (7); widget lists 0x0050e4f8 / 0x0050e444.
 *   0x0050e100 i32 exit flag, 0x0050e104 i32 result, 0x0050e6cc u8[8] player slots
 *
 * Verified against the original under emulation (tools/emu/check_match_inputcfg.sh).
 */
#ifndef MT_INPUTCFG_H
#define MT_INPUTCFG_H

#include <stdint.h>

#include "mt_shell.h"

typedef struct {
    const uint32_t* widgets;    /* 0x0050e100 .. 0x0050e444 as u32 (index = (addr - 0x0050e100) / 4) */
    int32_t* exit_flag;         /* 0x0050e100 */
    int32_t* result;            /* 0x0050e104 */
    uint8_t* slots;             /* 0x0050e6cc */
    const MtShell* shell;       /* 0x0045f080 / 0x0045f1e8 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    void (*widget_call)(void* user, uint32_t fn, uint32_t widget, int32_t arg); /* [[w+0x34]](w, arg) */
    void (*list_free)(void* user, uint32_t list);                   /* 0x004a2fdc */
    void (*set_rect)(void* user, const int32_t rect[4]);            /* SetRect (import 0x00c90574) */
    void (*invalidate)(void* user, uint32_t hwnd, uint32_t rect, int32_t erase); /* InvalidateRect (0x00c904f8) */
    void (*screen_begin)(void* user);                               /* 0x004a8e6c */
    void (*create_widgets)(void* user);                             /* 0x0045f3b4 inputcfg_create_widgets */
    uint32_t (*num_players)(void* user);                            /* 0x004a2470 (al) */
    void (*input_poll)(void* user);                                 /* 0x00493748 */
    uint32_t (*key_state)(void* user, int32_t id, int32_t b);       /* 0x004936ac (al) */
    uint32_t (*device_mode)(void* user, int32_t player);            /* 0x004932a0 (al, signed) */
    void (*widget_enable)(void* user, uint32_t widget, int32_t on);  /* 0x004a6dd8 */
    /* 0x00461924.. */
    const int32_t* busy;        /* 0x0050e5b8 */
    const uint8_t* keytab;      /* 0x004e7a48 (one key id byte every 8 bytes, 16) */
    const uint32_t* names_left; /* 0x004e755c (text handle per key code) */
    const uint32_t* names_right; /* 0x004e797c */
    uint32_t (*widget_get)(void* user, uint32_t fn, uint32_t widget); /* [[w+0x34]+0xc8](w) */
    void (*busy_click)(void* user, uint32_t widget);                 /* 0x004a6f80 */
    void (*set_device)(void* user, int32_t player, int32_t on);      /* 0x004932c4 */
    void (*key_set_a)(void* user, uint32_t key, uint32_t code);      /* 0x004a1f6c */
    void (*key_set_b)(void* user, uint32_t key, uint32_t code);      /* 0x004a274c */
    void (*keys_save)(void* user);                                   /* 0x004a9c38 */
    void (*keys_default_a)(void* user);                              /* 0x004a2018 */
    uint32_t (*key_get_a)(void* user, uint32_t key);                 /* 0x004a1f8c (al) */
    void (*keys_default_b)(void* user);                              /* 0x004a27d4 */
    uint32_t (*key_get_b)(void* user, uint32_t key);                 /* 0x004a2758 (al) */
    void (*text_set)(void* user, uint32_t widget, uint32_t text);    /* 0x004a6f08 (WM_SETTEXT) */
    void (*render_mode)(void* user, int32_t a, int32_t b);           /* 0x0046086c */
    uint32_t (*main_window)(void* user);                             /* 0x004a8f34 */
    void (*message_box)(void* user, uint32_t hwnd, uint32_t text, uint32_t caption, uint32_t type); /* MessageBoxA (0x00c90524) */
    void (*input_update)(void* user);                                /* 0x0049364c */
    /* key rebinding dialogs */
    int32_t* dlg;               /* 0x0050e5b8 .. 0x0050e5c9: +0 busy, +4 waiting, +8 use second
                                   code, +0xc i32, +0x10 u8 code, +0x11 u8 second code */
    void (*widget_color)(void* user, uint32_t fn, uint32_t widget, int32_t rgb); /* [[w+0x34]+0xb0](w, rgb) */
    void (*widget_redraw)(void* user, uint32_t widget, int32_t a, int32_t b);   /* 0x004a6e2c (w, 0, 0) */
    void (*dlg_setup)(void* user, int32_t player, int32_t index);              /* 0x0045fc3c inputcfg_setup */
    void (*dlg_step_left)(void* user, int32_t index);                          /* 0x004605b0 */
    void (*dlg_step_right)(void* user, int32_t index);                         /* 0x00460b04 */
    void (*list_redraw)(void* user, uint32_t list, int32_t a, int32_t b, int32_t c2); /* 0x004a2ffc (list, 0, 0, 0) */
    void (*refresh_right)(void* user, int32_t a, int32_t b);                    /* 0x00460ea0 (0, 0) */
    /* duplicate key marking (status text widget *0x0050e43c; strings as host pointers) */
    void (*status_clear)(void* user, uint32_t widget);                          /* 0x004a5630 */
    void (*status_set)(void* user, uint32_t widget, const char* text);          /* 0x004a5418 */
    void (*status_get)(void* user, uint32_t widget, int32_t a, char* out);      /* 0x004a559c (w, 0, buffer) */
    int32_t (*str_compare)(void* user, const char* a, const char* b);           /* 0x004a91b0 */
    void* user;
} MtInputCfg;

/* 0x004602b0: destroys the screen widgets and both car lists (non 0 ones), frees list 0x0050e4f8. */
void mt_inputcfg_destroy_widgets(const MtInputCfg* c);
/* 0x00460370: destroys the car panel widgets, frees list 0x0050e444, SetRect(local, 0xa0, 0x4d,
 * 0x1df, 0x1ab) (the rect is not used afterwards) and InvalidateRect(0, 0, 1). */
void mt_inputcfg_destroy_car_panel(const MtInputCfg* c);
/* 0x0045f2dc inputcfg_main: screen begin, widgets, state table, player slots (0 and 1..4 on,
 * 5..7 by the number of input devices), unlocks, then polls until key 0x14 or 0x15; destroys the
 * widgets and returns 0x0050e104. */
int32_t mt_inputcfg_main(const MtInputCfg* c);
/* 0x00460438 / 0x004604f4 (old names shell_setup_1 / _2): the left / right player's input device
 * mode m = (int8) device_mode(1 / 4): its device widget (0x0050e108 / 0x0050e10c) selected
 * ([[w+0x34]+0xd0](w, m == 1)); m 2: widget disabled, its car list (0x0050e120 / 0x0050e220)
 * disabled; m 0: widget enabled, list disabled; m 1: both enabled; else nothing more. */
void mt_inputcfg_device_left(const MtInputCfg* c);
void mt_inputcfg_device_right(const MtInputCfg* c);
/* 0x00461924 / 0x004619e4 / 0x00461a68 / 0x00461ae0: button handlers. All start with: while a
 * message is up (0x0050e5b8), only busy_click(0x0050e440) (when that is not 0).
 * 0x00461924 accept: both device widgets' states (method +0xc8 == 1) to set_device(1 / 4), the 16
 * key codes of both players (bytes 0x0050e11d / 0x0050e21d + 0x10 k) to key_set_a / _b(key id
 * 0x004e7a48[8 k]), keys_save, exit flag 0x0050e100 = 1.
 * 0x004619e4 / 0x00461a68 defaults for the left / right player: keys_default, each key's code
 * shown (text_set(list widget, name table[code])) and stored, device widgets (0x00460438 /
 * 0x004604f4); left also render_mode(0, 0).
 * 0x00461ae0 device button (widget w): selected, 0x00460438; then busy_click(0x0050e440) while
 * busy, else MessageBoxA(0x004a8f34(), 0x004e0a89, 0x004e0a7b, 0x30), input polled twice
 * (0x00493748, 0x0049364c), busy_click(w). */
void mt_inputcfg_accept(const MtInputCfg* c);
void mt_inputcfg_defaults_left(const MtInputCfg* c);
void mt_inputcfg_defaults_right(const MtInputCfg* c);
void mt_inputcfg_device_button(const MtInputCfg* c, uint32_t widget);
/* 0x00461b48: right player device toggle (widget w): state 0 -> selected, set_device(4, 1) if the
 * device mode (al of 0x004932a0(4)) is 0; state 1 -> unselected, set_device(4, 0) if it is 1;
 * then 0x004604f4, the 16 key names shown again (names 0x004e797c), input polled. */
void mt_inputcfg_device_toggle_right(const MtInputCfg* c, uint32_t widget);
/* 0x00461c10 / 0x00461dc0: rebinding a key of the left / right player (w = the key's widget in
 * that player's list; index = its last match, -1 if none): highlighted (0xff00), redrawn,
 * dlg_setup(1 / 4, index), busy = waiting = 1, second = 0 (left also clears +0xc), then
 * dlg_step(index) after 0x004a8e6c until waiting is 0; the new code (+0x10, or +0x11 when second)
 * goes to key_set_a / _b and the widget text and the code byte; busy = 0, car panel destroyed
 * (0x00460370), list_redraw(0x0050e4f8, 0, 0, 0), SetRect / InvalidateRect, input polled, then
 * render_mode(0, 0) / refresh_right(0, 0), busy_click(w). */
void mt_inputcfg_rebind_left(const MtInputCfg* c, uint32_t widget);
void mt_inputcfg_rebind_right(const MtInputCfg* c, uint32_t widget);
/* 0x0046086c / 0x00460ea0 (index, check): the left / right player's key list colours. check == 0:
 * every code used by two keys (non 0) marks both (dup bytes 0x0050e11e / 0x0050e21e + 0x10 k),
 * marked keys red (method +0xb0 with 0xff), others 0, all redrawn. check != 0: the status line
 * cleared, every other key with the new code (0x0050e5c9) red and named in the status line
 * (0x004e098a / 0x004e09ed + the key's name, *(0x004e7a44 + 8 k)), the rest by their dup byte;
 * no clash: the status text, read back, is replaced by 0x004e09a7 / 0x004e0a0a when it differs
 * from 0x004e0999 / 0x004e09fc. */
void mt_inputcfg_mark_left(const MtInputCfg* c, int32_t index, int32_t check);
void mt_inputcfg_mark_right(const MtInputCfg* c, int32_t index, int32_t check);

#endif /* MT_INPUTCFG_H */
