/*
 * pf_mouse.h: TM2.EXE's mouse module (0x004b2820..0x004b2f20), ported over a backend that stands
 * in for the Win32 calls (docs/PLATFORM.md, "Input: mouse"). The game polls the mouse through
 * GetAsyncKeyState and GetCursorPos (not DirectInput) into a 0x34 byte state its callers keep.
 *
 * Mouse state (PfMouse, offsets in the original):
 *   +0x00 present (GetSystemMetrics(SM_MOUSEPRESENT) != 0)   +0x04 buttons (SM_CMOUSEBUTTONS)
 *   +0x08 / +0x0c cursor x / y in client coordinates, -1 = unknown
 *   +0x10 button bits (1 primary, 2 secondary, 4 middle)   +0x14 / +0x18 / +0x1c primary /
 *   secondary / middle down   +0x20 / +0x24 reference size (640 x 480, then the screen size)
 *   +0x28 axis range (0x7f)   +0x2c (0, unused here)   +0x30 axis dead zone (0x19)
 * Globals: 0x00c84810 / 0x00c84814 / 0x00c84818 virtual keys of the primary / secondary / middle
 * button (1 / 2 / 4, swapped by "SwapMouseButtons"); 0x004e86a8 "registry not read yet".
 *
 * Ported (emulator diff: tools/emu/check_pf_mouse.sh): 0x004b2820 init, 0x004b295c reread swap,
 * 0x004b29e0 poll, 0x004b2ad4 cursor client position, 0x004b2b00 centred position, 0x004b2b84 /
 * 0x004b2bdc / 0x004b2c44 / 0x004b2c9c axes, 0x004b2d00 / 0x004b2d60 / 0x004b2db8 / 0x004b2e48
 * set cursor, 0x004b2ee0 / 0x004b2f00 button down, getters 0x004b2b78 / 0x004b2b7c / 0x004b2b80 /
 * 0x004b2c40 / 0x004b2ed8 / 0x004b2edc.
 */
#ifndef PF_MOUSE_H
#define PF_MOUSE_H

#include <stdint.h>

typedef struct {
    int32_t present, buttons, x, y, bits, left, right, middle, ref_w, ref_h, range, unused2c, dead;
} PfMouse;

typedef struct { int32_t x, y; } PfPoint;

typedef struct {
    void* user;
    int32_t (*get_system_metrics)(void* user, int32_t index);            /* USER32 GetSystemMetrics */
    int16_t (*get_async_key_state)(void* user, int32_t vk);              /* USER32 GetAsyncKeyState */
    int32_t (*get_cursor_pos)(void* user, PfPoint* pt);                  /* USER32 GetCursorPos */
    int32_t (*screen_to_client)(void* user, PfPoint* pt);                /* USER32 ScreenToClient(main window) */
    int32_t (*client_to_screen)(void* user, PfPoint* pt);                /* USER32 ClientToScreen(main window) */
    int32_t (*set_cursor_pos)(void* user, int32_t x, int32_t y);         /* USER32 SetCursorPos */
    void (*get_hwnd)(void* user);                                         /* 0x004a8f34 app_get_hwnd (the window the
                                                                             two conversions use) */
    int32_t (*screen_width)(void* user);                                  /* 0x004958d8 -> display +0x58 */
    int32_t (*screen_height)(void* user);                                 /* 0x004958e4 -> display +0x5c */
    /* the registry helpers 0x004ac4b0 (init with the subkey), 0x004ac528 (open under the root,
       0 = ok), 0x004ac5e4 (read a value into buf, *size in / out, 0 = ok), 0x004ac590 (close) */
    void (*reg_init)(void* user, const char* subkey);
    int32_t (*reg_open)(void* user, uint32_t root);
    int32_t (*reg_read)(void* user, const char* value, char* buf, int32_t* size);
    void (*reg_close)(void* user);
} PfMouseBackend;

typedef struct {
    int32_t vk_primary, vk_secondary, vk_middle;   /* 0x00c84810 / 0x00c84814 / 0x00c84818 */
    int32_t registry_unread;                       /* 0x004e86a8 */
    const PfMouseBackend* be;
} PfMouseSys;

PfMouse* pf_mouse_init(PfMouseSys* g, PfMouse* m);
void pf_mouse_reread_swap(PfMouseSys* g);
void pf_mouse_poll(PfMouseSys* g, PfMouse* m);
int32_t pf_mouse_cursor_client(PfMouseSys* g, const PfMouse* m, PfPoint* pt);
int32_t pf_mouse_centered_pos(PfMouseSys* g, const PfMouse* m, int32_t* x, int32_t* y);
int32_t pf_mouse_axis_right(const PfMouse* m);
int32_t pf_mouse_axis_left(const PfMouse* m);
int32_t pf_mouse_axis_down(const PfMouse* m);
int32_t pf_mouse_axis_up(const PfMouse* m);
int32_t pf_mouse_set_cursor_pt(PfMouseSys* g, const PfPoint* p);
int32_t pf_mouse_set_cursor(PfMouseSys* g, int32_t x, int32_t y);
int32_t pf_mouse_set_cursor_x(PfMouseSys* g, const PfMouse* m, int32_t x);
int32_t pf_mouse_set_cursor_y(PfMouseSys* g, const PfMouse* m, int32_t y);
int32_t pf_mouse_primary_down(PfMouseSys* g);
int32_t pf_mouse_secondary_down(PfMouseSys* g);

#endif /* PF_MOUSE_H */
