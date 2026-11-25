/*
 * mt2_save.h: the save game / load game screens of TM2.EXE (PC only shell code, no PS1 pair),
 * ported from the PC machine code:
 *   0x0046d290 / 0x0046ee30  build the save / load screen widgets
 *   0x0046d834 / 0x0046f3d4  destroy them
 *   0x0046d8e4 / 0x0046f484  run the save / load screen
 *   0x0046da2c, 0x0046dd2c, 0x0046e034, 0x0046e33c, 0x0046e644, 0x0046e94c  save slot 1..6 click
 *   0x0046cd70 / 0x0046d000  slot labels from the registry (load / save screen)
 *   0x0046d17c               write a save to the registry
 *   0x0046ecb0               read a save from the registry
 *
 * Globals (u32 each, index (addr - 0x0058a950) / 4 in MtSave.g):
 *   0x0058a950 i32 done, 0x0058a954 i32 save result, 0x0058a958 i32 load result,
 *   0x0058a95c i32 slot being edited (1..6), 0x0058a960 i32 editing flag,
 *   0x0058a964 edit box widget, 0x0058a968 save help panel, 0x0058a96c / 0x0058a970 save screen
 *   buttons (0xb8 / 0xb7), 0x0058a974 load help panel, 0x0058a978 / 0x0058a97c load screen
 *   buttons, 0x0058a980 + 4 k (k = 0..5) slot buttons.
 * Widgets are handles; [[w + 0x34] + off](w, arg) is a method call (off 0 with 2 destroys).
 * Strings in the EXE are handles (their original addresses); text buffers on the stack are host
 * buffers.
 *
 * Verified against the original under emulation (tools/emu/check_mt2_save.sh).
 */
#ifndef MT2_SAVE_H
#define MT2_SAVE_H

#include <stdint.h>

#define MT2_SAVE_G(addr) (((addr) - 0x0058a950u) / 4u)

typedef struct {
    uint32_t* g; /* 0x0058a950 .. 0x0058a997 */
    uint32_t (*main_window)(void* user);                          /* 0x004a8f34 */
    uint32_t (*alloc)(void* user, uint32_t size);                  /* 0x004a8f48 */
    /* 0x004a490c text button (buf, hwnd, id, text, 5 stack words; s[] in stack order: s[0] = [esp + 4]) */
    uint32_t (*button_create)(void* user, uint32_t buf, uint32_t hwnd, uint32_t id, uint32_t text, const int32_t s[5]);
    uint32_t (*panel_create)(void* user, uint32_t buf, uint32_t hwnd, uint32_t id, uint32_t style, const int32_t s[4]); /* 0x004a5290 */
    uint32_t (*edit_create)(void* user, uint32_t buf, uint32_t hwnd, uint32_t id, uint32_t text, const int32_t s[5]);   /* 0x004a5f40 */
    void (*set_colors)(void* user, uint32_t w, uint32_t a, uint32_t b);                 /* 0x004a4de0 */
    void (*set_keys)(void* user, uint32_t w, uint32_t a, uint32_t b, uint32_t c, uint32_t s0); /* 0x004a4bdc */
    void (*method)(void* user, uint32_t w, uint32_t off, uint32_t arg);                 /* [[w+0x34]+off] */
    void (*list_clear)(void* user, uint32_t w);                                         /* 0x004a5630 */
    void (*list_add)(void* user, uint32_t w, uint32_t text);                            /* 0x004a5418 */
    void (*nav_links)(void* user, uint32_t w, uint32_t a, uint32_t b, uint32_t c, const uint32_t s[3]); /* 0x004a70ac */
    void (*show)(void* user, uint32_t w, int32_t cmd);                                  /* 0x004a6ee4 */
    void (*focus)(void* user, uint32_t w);                                              /* 0x004a6f80 */
    void (*enable)(void* user, uint32_t w, int32_t on);                                 /* 0x004a6dd8 */
    void (*get_text)(void* user, uint32_t w, char* buf, int32_t n);                     /* 0x004a6ef4 */
    void (*set_text)(void* user, uint32_t w, const char* text);                         /* 0x004a6f08 */
    void (*set_text_h)(void* user, uint32_t w, uint32_t text);                          /* 0x004a6f08, EXE string */
    uint32_t (*str_compare)(void* user, const char* a, uint32_t text);                  /* 0x004a91b0 */
    int32_t (*message_box)(void* user, uint32_t hwnd, uint32_t text, uint32_t caption, uint32_t type); /* MessageBoxA */
    void (*input_poll)(void* user);                                                     /* 0x00493748 */
    void (*input_update)(void* user);                                                   /* 0x0049364c */
    void (*redraw)(void* user, uint32_t w, uint32_t rect, int32_t erase);               /* 0x004a6e2c */
    void (*screen_begin)(void* user);                                                   /* 0x004a8e6c */
    uint32_t (*has_focus)(void* user, uint32_t w);                                      /* 0x004a6f9c */
    uint32_t (*get_key)(void* user);                                                    /* 0x004a2068 (al) */
    void (*escape_amp)(void* user, char* text, uint32_t flag);                          /* 0x0046f66c */
    void (*save_write)(void* user, char* text);                                         /* 0x0046d17c */
    uint32_t (*key_state)(void* user, uint32_t id, uint32_t b);                         /* 0x004936ac (al) */
    void (*reg_open)(void* user, void* key, uint32_t b);                                /* 0x004ac4b0 */
    void (*labels)(void* user, int load);       /* 0x0046d000 (save) / 0x0046cd70 (load) slot labels */
    /* registry and saved game record (0x0046cd70, 0x0046d000, 0x0046d17c, 0x0046ecb0) */
    int32_t (*reg_subkey)(void* user, void* key, uint32_t hkey, uint32_t b);           /* 0x004ac528 */
    int32_t (*reg_read)(void* user, void* key, uint32_t name, void* data, uint32_t* size); /* 0x004ac5e4, EXE name */
    int32_t (*reg_read_s)(void* user, void* key, const char* name, void* data, uint32_t* size); /* 0x004ac5e4 */
    void (*reg_write)(void* user, void* key, const char* name, const void* data, uint32_t size); /* 0x004ac65c */
    void (*reg_close)(void* user, void* key);                                           /* 0x004ac590 */
    void (*format_text)(void* user, char* out, const char* fmt);  /* sprintf 0x004b27e6 (out, fmt) */
    void (*format_slot)(void* user, char* out, uint32_t fmt, int32_t n); /* sprintf (out, EXE fmt, n) */
    uint32_t (*level_count)(void* user, uint32_t game, int32_t level);   /* 0x00474e0c (al) */
    uint8_t* human0;  /* 0x005030a8 human car 0 (0x0045afbc(0)): +0x856..+0x869 10 u16 saved */
    uint8_t* game;    /* 0x00624ea8 .. 0x00624ed0 game settings (see mt2_save_record_load) */
    void* user;
} MtSave;

/* 0x0046d290 (save, load = 0) / 0x0046ee30 (load, load = 1): six slot buttons (ids 0x79..0x7e,
 * at x 0x78, y 0x92 + 0x2f k, 0x190 x 0x19), the slot labels (0x0046d000 / 0x0046cd70), two more buttons (0xb7 at 0x17c / 0xb8 at 0x78, y 0x1b3, 0x8c x 0x17), the help panel
 * (0xb9, 0x100 style) with two help lines, click handlers (method 0xe0), the navigation links,
 * every widget shown (5), focus on button 0xb7. */
void mt2_save_widgets(const MtSave* s, int load);
/* 0x0046d834 / 0x0046f3d4: destroys the slot buttons, then the help panel and the two buttons
 * (non 0 ones). */
void mt2_save_destroy(const MtSave* s, int load);
/* 0x0046d8e4 / 0x0046f484: run the save / load screen until key 0x14 or 0x15; returns 0x0058a954
 * / 0x0058a958. */
uint32_t mt2_save_run(const MtSave* s);
uint32_t mt2_load_run(const MtSave* s);
/* 0x0046da2c + 0x308 (slot - 1) (slot 1..6): the save slot click: ask before overwriting, then an
 * edit box for the description; on Enter the description is saved (save_write). */
void mt2_save_slot(const MtSave* s, int slot);

/* 0x0046d000 (load = 0) / 0x0046cd70 (load = 1): slot labels from the registry values Slot1..6
 * (HKEY_LOCAL_MACHINE 0x80000002): the saved description, or the EXE's empty text; the load
 * screen also enables a slot only when it has a save, and redraws it. */
void mt2_save_labels(const MtSave* s, int load);
/* 0x0046d17c: writes a save (description text, not 0; an empty one, that is equal to the EXE's
 * 0x004e1d5c, gets a message box) under the value "Slot<0x0058a95c>": 0xa0 bytes, +0x00 the
 * description through sprintf (ORIGINAL BUG: it is the format), +0x80 i32 (int8)0x00624ead,
 * +0x84 i32 (int8)0x00624ec7, +0x88 i32 (u8)0x00624ebf, +0x8c 10 u16 human car 0 +0x856.. */
void mt2_save_record_write(const MtSave* s, char* text);
/* 0x0046ecb0: reads the save "Slot<0x0058a95c>" back: 0x0058a958 = 1 when loaded, 0 when the value
 * is missing or holds the EXE's empty text 0x004e2088. Loading sets 0x00624ea8 = 0x00624ea9 = 1,
 * 0x00624ead = +0x80, 0x00624ec7 = +0x84, 0x00624eaa = level_count(0x00624ea8, (int8)+0x84) - 1,
 * 0x00624ebf = min(+0x88 (signed), 6), the 10 u16 to human car 0; a message box when 0x00624ed0
 * is 0 and (int8)0x00624ec7 > 4. */
void mt2_save_record_load(const MtSave* s);

#endif /* MT2_SAVE_H */
