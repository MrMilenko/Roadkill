/*
 * win32_host.h: shared pieces of the host services (src/host/win32_*.c, watcom_*.c): the Win32
 * imports and the Watcom C runtime the game calls, implemented on POSIX + SDL2 (port code, not
 * emulation). Arguments arrive as rt_call words (rt.h): a Win32 import gets its stdcall words in
 * order, a Watcom function EAX, EDX, EBX, ECX then the stack words; strings and structs are game
 * addresses (rt_ptr).
 *
 * Threads: game code is not thread safe (rt.c's call chain and frame stack are global). Every
 * thread runs game code holding the one host lock (win32_lock); the host releases it only while
 * a thread blocks (Sleep, WaitForSingleObject, a thread's exit), so threads interleave at those
 * points only. See docs in win32_kernel.c.
 */
#ifndef TM2_WIN32_HOST_H
#define TM2_WIN32_HOST_H

#include "rt.h"

#include <stddef.h>
#include <stdint.h>

/* the i-th word, 0 when not passed */
static inline uint32_t w32_arg(const uint32_t* a, int n, int i) { return i < n ? a[i] : 0u; }
#define W(i) w32_arg(a, n, (i))

/* a game C string (NULL for address 0) */
static inline const char* w32_str(uint32_t addr) { return addr ? (const char*)rt_ptr(addr) : NULL; }
/* copy a host string to game memory, at most cap bytes with the NUL (cap 0: unbounded) */
void w32_put_str(uint32_t dst, const char* s, uint32_t cap);
/* a game address holding a copy of s that lives for the whole run (fake area) */
uint32_t w32_static_str(const char* s);

/* handles: small host objects behind a Win32 HANDLE value */
enum { H_FREE, H_FILE, H_MUTEX, H_EVENT, H_THREAD, H_MODULE, H_GDI, H_MISC, H_KEY };
uint32_t w32_handle_new(int type, void* p);
void* w32_handle_get(uint32_t h, int type);   /* NULL when not that type */
int w32_handle_type(uint32_t h);
void w32_handle_free(uint32_t h);

/* the game's paths on the host: drive letters dropped, '\' -> '/', each component matched case
 * insensitively under the data root (TM2_DATA, default disc/files). Returns out. */
const char* w32_host_path(const char* game, char* out, size_t cap);
const char* w32_data_root(void);
char w32_cd_letter(void);   /* the drive the disc folder shows up as (TM2_CD, default 'D') */

/* the last error, GetLastError's answer */
extern uint32_t w32_last_error;

/* the host lock all game code runs under; w32_block_begin / end around a blocking wait */
void w32_lock(void);
void w32_unlock(void);
void w32_block_begin(void);
void w32_block_end(void);
void w32_yield(void);   /* main thread: let a waiting game thread run */
uint32_t w32_thread_id(void);   /* GetCurrentThreadId: 1 for the main thread */

/* a game allocation for host services (GlobalAlloc, VirtualAlloc, ...): zeroed, 16 aligned */
uint32_t w32_alloc(uint32_t n);
void w32_free(uint32_t p);
uint32_t w32_alloc_size(uint32_t p);

/* call a window procedure (game or host): the four stdcall words at a[0..3] (compact) */
uint32_t w32_call_wndproc(uint32_t proc, uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp);

/* printf on game words: next() returns the next 32 bit argument word */
typedef uint32_t (*W32Next)(void* user);
int w32_format(char* out, size_t cap, const char* fmt, W32Next next, void* user);

/* milliseconds since start up (timeGetTime) */
uint32_t w32_ms(void);

/* the PE resources of the loaded image: the game address of the IMAGE_RESOURCE_DATA_ENTRY for
 * (type, name) where each is an integer id (< 0x10000) or a game string address; 0 if none */
uint32_t w32_find_resource(uint32_t type, uint32_t name);

/* SDL: the main window (NULL before CreateWindowExA made it); for the display workers */
struct SDL_Window;
struct SDL_Window* win32_sdl_window(void);
uint32_t win32_main_hwnd(void);

/* scripted input (win32_keys.c): the keyboard state every reader uses (SDL's merged with the
 * TM2_KEYS presses), indexed by SDL scancode; win32_keys_frame() runs once per presented frame */
const uint8_t* host_keyboard_state(int* n);
void win32_keys_frame(int frame);
void win32_keys_tick(void);
int dx_sdl_screenshot(const char* path);

#endif /* TM2_WIN32_HOST_H */
