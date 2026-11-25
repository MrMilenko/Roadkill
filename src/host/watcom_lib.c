/*
 * watcom_lib.c: the small library routines linked above 0x004c0000 in TM2.EXE (after the game's
 * own code: C runtime helpers and the DirectX wrapper classes) that the ported functions call,
 * ported from the original's machine code (port code, not verified under the emulator: each
 * comment gives the original's behaviour it follows). Watcom convention: a[0..3] = EAX, EDX,
 * EBX, ECX as far as the callee takes registers, then its stack words (compact). A COM
 * method is called through the vtable in game memory, as the original does, with the object
 * first (PF2_COM style words).
 */
#include "win32_host.h"
#include "host/settings/settings.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/* a COM method of obj: vtable slot off, the stdcall words after the object */
static uint32_t com3(uint32_t obj, uint32_t off, int n, uint32_t w1, uint32_t w2, uint32_t w3)
{
    uint32_t v[4] = {obj, w1, w2, w3};
    return rt_call(rt_r32(rt_r32(obj) + off), v, n + 1);
}
static uint32_t com(uint32_t obj, uint32_t off, int n, uint32_t w1, uint32_t w2)
{
    uint32_t v[3] = {obj, w1, w2};
    return rt_call(rt_r32(rt_r32(obj) + off), v, n + 1);
}

/* 0x004c9f00: a lone ret (the base class open): eax unchanged */
static uint32_t l_ret(const uint32_t* a, int n) { return W(0); }

/* 0x004c9f04 (o eax): the music base constructor: o+4 = -1, o+8 = vtable 0x004e632c; eax kept */
static uint32_t l_music_base_ctor(const uint32_t* a, int n)
{
    rt_w32(W(0) + 4, 0xffffffffu);
    rt_w32(W(0) + 8, 0x004e632cu);
    return W(0);
}

/* 0x004c9f83 atoi(s eax): blanks skipped (ctype 0x004e633c, index c + 1, bit 2), a sign, then
 * digits (bit 0x20) */
static uint32_t l_atoi(const uint32_t* a, int n)
{
    uint32_t p = W(0);
    while (rt_r8(0x004e633cu + (uint8_t)(rt_r8(p) + 1)) & 2) p++;
    uint8_t s = rt_r8(p);
    if (s == '+' || s == '-') p++;
    uint32_t v = 0;
    while (rt_r8(0x004e633cu + (uint8_t)(rt_r8(p) + 1)) & 0x20) v = v * 10u + rt_r8(p++) - '0';
    return s == '-' ? 0u - v : v;
}

/* 0x004c38f4 (x eax): u32 0x00c8b370 = x; eax kept */
static uint32_t l_set_c8b370(const uint32_t* a, int n) { rt_w32(0x00c8b370u, W(0)); return W(0); }

/* the mouse device object (0x00a82164): +0x27c open; the state's buttons at +0x284..0x286, the
 * x / y / z deltas at +0x288 / +0x28c, +0x25c */
static uint32_t l_mouse_x(const uint32_t* a, int n) { return rt_r32(W(0) + 0x27c) ? rt_r32(W(0) + 0x288) : 0; }   /* 0x004c4130 */
static uint32_t l_mouse_y(const uint32_t* a, int n) { return rt_r32(W(0) + 0x27c) ? rt_r32(W(0) + 0x28c) : 0; }   /* 0x004c414c */
static uint32_t l_mouse_z(const uint32_t* a, int n) { return rt_r32(W(0) + 0x27c) ? rt_r32(W(0) + 0x25c) : 0; }   /* 0x004c4168 */
static uint32_t l_mouse_b0(const uint32_t* a, int n) { return rt_r32(W(0) + 0x27c) ? rt_r8(W(0) + 0x284) & 0x80u : 0; }   /* 0x004c41c8 */
static uint32_t l_mouse_b2(const uint32_t* a, int n) { return rt_r32(W(0) + 0x27c) ? rt_r8(W(0) + 0x286) & 0x80u : 0; }   /* 0x004c41e8 */
/* 0x004c4208: button 1; ORIGINAL BUG: a closed device answers the caller's edx (here 0) */
static uint32_t l_mouse_b1(const uint32_t* a, int n) { return rt_r32(W(0) + 0x27c) ? rt_r8(W(0) + 0x285) & 0x80u : 0; }

/* 0x004c1024 (o eax): releases the Direct3D objects: with o+8: (o+0xc)->vt[0x6c](), then
 * (o+8)->QueryInterface(IID 0x004e55d4, &p): p -> p->vt[0x1c](), p->Release(); (o+8)->Release(),
 * o+8 = 0; o+0xc and o+0x10 Release()d and zeroed when set; o+0 = 0 */
static uint32_t l_d3d_release(const uint32_t* a, int n)
{
    uint32_t o = W(0);
    if (rt_r32(o + 8)) {
        uint32_t loc = rt_frame_push(16);
        com(rt_r32(o + 0xc), 0x6c, 0, 0, 0);
        com(rt_r32(o + 8), 0, 2, 0x004e55d4u, loc);
        uint32_t p = rt_r32(loc);
        if (p) { com(p, 0x1c, 0, 0, 0); com(p, 8, 0, 0, 0); }
        com(rt_r32(o + 8), 8, 0, 0, 0);
        rt_w32(o + 8, 0);
        rt_frame_pop(16);
    }
    if (rt_r32(o + 0xc)) { com(rt_r32(o + 0xc), 8, 0, 0, 0); rt_w32(o + 0xc, 0); }
    if (rt_r32(o + 0x10)) { com(rt_r32(o + 0x10), 8, 0, 0, 0); rt_w32(o + 0x10, 0); }
    rt_w32(o, 0);
    return o;
}
/* 0x004c101c (o eax): 0x004c1024(o), returns 0 */
static uint32_t l_d3d_close(const uint32_t* a, int n) { l_d3d_release(a, n); return 0; }

/* 0x004c1132 qsort(base eax, count edx, size ebx, compare ecx: a Watcom function (eax, edx)).
 * The host sorts with a stable merge sort through the game's comparator: the original's quick sort
 * may order equal elements differently (NOT VERIFIED against it). */
static uint32_t sort_cmp, sort_size;
static void merge(uint8_t* b, uint8_t* tmp, uint32_t base, uint32_t n)
{
    if (n < 2) return;
    uint32_t h = n / 2;
    merge(b, tmp, base, h);
    merge(b, tmp, base + h * sort_size, n - h);
    uint32_t i = 0, j = h, k = 0, sz = sort_size;
    while (i < h && j < n) {
        uint32_t v[2] = {base + i * sz, base + j * sz};
        if ((int32_t)rt_call(sort_cmp, v, 2) <= 0) memcpy(tmp + k++ * sz, b + i++ * sz, sz);
        else memcpy(tmp + k++ * sz, b + j++ * sz, sz);
    }
    while (i < h) memcpy(tmp + k++ * sz, b + i++ * sz, sz);
    while (j < n) memcpy(tmp + k++ * sz, b + j++ * sz, sz);
    memcpy(b, tmp, (size_t)n * sz);
}
static uint32_t l_qsort(const uint32_t* a, int n)
{
    uint32_t base = W(0), cnt = W(1), oc = sort_cmp, os = sort_size;
    sort_cmp = W(3);
    sort_size = W(2);
    if (cnt > 1 && sort_size) {
        uint8_t* tmp = malloc((size_t)cnt * sort_size);
        /* the comparator reads the elements at their game addresses: sort in place there */
        merge(rt_ptr(base), tmp, base, cnt);
        free(tmp);
    }
    sort_cmp = oc;
    sort_size = os;
    return 0;
}

/* ---- POSIX style file I/O (the original: a handle table 0x00c8be48 over Win32 files) ----
 * Host side: game handles 3.. index a host fd table; paths mapped as CreateFileA does
 * (w32_host_path). Watcom open flags: 0/1/2 access, 0x10 append, 0x20 create, 0x40 truncate,
 * 0x80 exclusive, 0x100 text / 0x200 binary (no text mode translation here). */
#define NFD 64
static int fds[NFD];
static int fd_get(uint32_t h) { return h >= 3 && h < NFD && fds[h] > 0 ? fds[h] - 1 : -1; }

/* 0x004ca197 open(name, mode, ...): cdecl, no register arguments: the words at a[0..] */
static uint32_t l_open(const uint32_t* a, int n)
{
    uint32_t name = W(0), mode = W(1);
    char path[1024];
    w32_host_path(w32_str(name), path, sizeof path);
    int fl = (mode & 3) == 1 ? O_WRONLY : (mode & 3) == 2 ? O_RDWR : O_RDONLY;
    if (mode & 0x10) fl |= O_APPEND;
    if (mode & 0x20) fl |= O_CREAT;
    if (mode & 0x40) fl |= O_TRUNC;
    if (mode & 0x80) fl |= O_EXCL;
    int fd = open(path, fl, 0644);
    if (settings_bool("debug.trace_files", 0)) fprintf(stderr, "tm2: open %s -> %s %s\n", w32_str(name), path, fd < 0 ? "FAILED" : "ok");
    if (fd < 0) return 0xffffffffu;
    for (uint32_t h = 3; h < NFD; h++) if (!fds[h]) { fds[h] = fd + 1; return h; }
    close(fd);
    return 0xffffffffu;
}
/* 0x004c9ff0 read(handle eax, buffer edx, bytes ebx): the count, -1 on an error */
static uint32_t l_read(const uint32_t* a, int n)
{
    int fd = fd_get(W(0));
    if (fd < 0) return 0xffffffffu;
    uint32_t got = 0;
    while (got < W(2)) {
        ssize_t r = read(fd, rt_ptr(W(1) + got), W(2) - got);
        if (r < 0) return got ? got : 0xffffffffu;
        if (r == 0) break;
        got += (uint32_t)r;
    }
    return got;
}
/* 0x004ca372 filelength(handle eax) */
static uint32_t l_filelength(const uint32_t* a, int n)
{
    struct stat st;
    int fd = fd_get(W(0));
    return fd >= 0 && fstat(fd, &st) == 0 ? (uint32_t)st.st_size : 0xffffffffu;
}
/* 0x004ca3cb close(handle eax): 0, -1 for a bad handle */
static uint32_t l_close(const uint32_t* a, int n)
{
    int fd = fd_get(W(0));
    if (fd < 0) return 0xffffffffu;
    close(fd);
    fds[W(0)] = 0;
    return 0;
}
/* 0x004ca4e2 lseek(handle eax, offset edx, whence ebx): the new position, -1 */
static uint32_t l_lseek(const uint32_t* a, int n)
{
    int fd = fd_get(W(0));
    if (fd < 0) return 0xffffffffu;
    off_t r = lseek(fd, (off_t)(int32_t)W(1), W(2) == 1 ? SEEK_CUR : W(2) == 2 ? SEEK_END : SEEK_SET);
    return r < 0 ? 0xffffffffu : (uint32_t)r;
}

/* 0x004cbba0 (dst eax, pattern edx, count ecx): the runtime fill: bytes up to 4 alignment then
 * the pattern rotated (ror 8 a byte), dwords, and the tail bytes dl, dh, dl (as the original) */
static uint32_t l_fill(const uint32_t* a, int n)
{
    uint32_t d = W(0), p = W(1), c = W(3);
    if (!c) return d;
    while (d & 3) { rt_w8(d++, (uint8_t)p); p = p >> 8 | p << 24; if (!--c) return d; }
    for (uint32_t k = c >> 2; k; k--) { rt_w32(d, p); d += 4; }
    c &= 3;
    if (c) { rt_w8(d, (uint8_t)p); if (c > 1) rt_w8(d + 1, (uint8_t)(p >> 8)); if (c > 2) rt_w8(d + 2, (uint8_t)p); }
    return d;
}

/* 0x004c3913 vfprintf(stream eax, format edx, &va ebx): the game's only stream is the debug
 * buffer 0x004e8fea; the host prints it to stderr when TM2_DEBUG=1. Returns the count. */
static uint32_t va_word(void* u) { uint32_t* va = u; uint32_t w = rt_r32(*va); *va += 4; return w; }
static uint32_t l_vfprintf(const uint32_t* a, int n)
{
    char buf[2048];
    uint32_t va = rt_r32(W(2));
    int len = w32_format(buf, sizeof buf, w32_str(W(1)), va_word, &va);
    if (settings_bool("debug.debug", 0)) fprintf(stderr, "tm2 debug: %s", buf);
    return (uint32_t)len;
}

/* ---- the DirectInput mouse object (0x00a82164): +0 IDirectInput, +4 the device, +0x24c
 * DIDEVCAPS (0x2c bytes), +0x278 acquired, +0x27c open, +0x284..0x287 buttons, +0x288 / 0x28c /
 * 0x290 x / y / z, +0x294 0 ---- */
static uint32_t gcall0(uint32_t fn) { return rt_call(fn, NULL, 0); }
static void di_error(uint32_t hr) { uint32_t v[1] = {hr}; rt_call(0x004937b8u, v, 1); }

/* 0x004c4028 (o eax): unacquire when acquired, release the device and the interface */
static uint32_t l_mouse_close(const uint32_t* a, int n)
{
    uint32_t o = W(0);
    if (rt_r32(o + 0x278)) {
        rt_w32(o + 0x278, 0);
        if (rt_r32(o + 4)) com(rt_r32(o + 4), 0x20, 0, 0, 0);
    }
    if (rt_r32(o + 4)) { com(rt_r32(o + 4), 8, 0, 0, 0); rt_w32(o + 4, 0); }
    if (rt_r32(o)) { com(rt_r32(o), 8, 0, 0, 0); rt_w32(o, 0); }
    return o;
}

/* 0x004c3d90 (o eax): the mouse object's constructor (static constructor 0x004a21b0 on
 * 0x00a82164): +0, +4, +0x278..+0x280, +0x288..+0x294 dwords and +0x284..+0x287 bytes zeroed.
 * Library code (above 0x004c0000), from the disassembly. */
static uint32_t l_mouse_ctor(const uint32_t* a, int n)
{
    uint32_t o = W(0);
    static const uint32_t dw[] = {0x4, 0x278, 0x27c, 0x280, 0x288, 0x28c, 0x290, 0x294, 0x0};
    for (unsigned i = 0; i < sizeof dw / sizeof dw[0]; i++) rt_w32(o + dw[i], 0);
    for (uint32_t k = 0x284; k < 0x288; k++) rt_w8(o + k, 0);
    return o;
}

/* 0x004c3e54 (o eax): opens the system mouse: DirectInputCreateA(hinst 0x004a8f40(), 0x500, &o+0,
 * 0); CreateDevice(GUID_SysMouse 0x004e53c4, o+4, 0); SetDataFormat(c_dfDIMouse 0x004d1410);
 * SetCooperativeLevel(window 0x004a8f34(), 6 nonexclusive | foreground); SetProperty(
 * DIPROP_AXISMODE, {0x14, 0x10, 0, 0, 1 relative}); Acquire; the caps (+0x24c, size 0x2c).
 * A failure: 0x004937b8(hr), the objects released, 0. Else +0x27c = 1, returns caps +0x10. */
static uint32_t l_mouse_open(const uint32_t* a, int n)
{
    uint32_t o = W(0);
    uint32_t L = rt_frame_push(0x30);   /* +0 the property, +0x14 the GUID */
    memcpy(rt_ptr(L + 0x14), rt_ptr(0x004e53c4u), 16);
    uint32_t hinst = gcall0(0x004a8f40u), hwnd = gcall0(0x004a8f34u);
    uint32_t v1[1] = {o};
    l_mouse_close(v1, 1);
    uint32_t r = 0, hr;
    uint32_t dc[4] = {hinst, 0x500, o, 0};
    if ((hr = rt_call(rt_import_slot("DINPUT.dll!DirectInputCreateA"), dc, 4)) != 0) { di_error(hr); goto out; }
    if ((hr = com3(rt_r32(o), 0x0c, 3, L + 0x14, o + 4, 0)) != 0) {
        di_error(hr);
        com(rt_r32(o), 8, 0, 0, 0);
        goto out;
    }
    uint32_t d = rt_r32(o + 4);
    rt_w32(L, 0x14); rt_w32(L + 4, 0x10); rt_w32(L + 8, 0); rt_w32(L + 12, 0); rt_w32(L + 16, 1);
    if ((hr = com(d, 0x2c, 1, 0x004d1410u, 0)) != 0 || (hr = com(d, 0x34, 2, hwnd, 6)) != 0 ||
        (hr = com(d, 0x18, 2, 2, L)) != 0 || (hr = com(d, 0x1c, 0, 0, 0)) != 0) {
        di_error(hr);
        com(rt_r32(o + 4), 8, 0, 0, 0);
        com(rt_r32(o), 8, 0, 0, 0);
        goto out;
    }
    memset(rt_ptr(o + 0x24c), 0, 0x2c);
    rt_w32(o + 0x24c, 0x2c);
    if ((hr = com(rt_r32(o + 4), 0x0c, 1, o + 0x24c, 0)) != 0) {
        di_error(hr);
        com(rt_r32(o + 4), 8, 0, 0, 0);
        com(rt_r32(o), 8, 0, 0, 0);
        goto out;
    }
    r = rt_r32(o + 0x25c);
    rt_w32(o + 0x27c, 1);
out:
    rt_frame_pop(0x30);
    return r;
}

/* 0x004c407c (o eax): open: GetDeviceState(16, &state): ok -> x / y / z / buttons stored, +0x294
 * = 0; DIERR_INPUTLOST -> +0x278 = 0, Acquire: ok -> +0x278 = 1, else 0x004937b8(hr) */
static uint32_t l_mouse_poll(const uint32_t* a, int n)
{
    uint32_t o = W(0);
    if (!rt_r32(o + 0x27c)) return o;
    uint32_t st = rt_frame_push(16);
    uint32_t hr = com(rt_r32(o + 4), 0x24, 2, 0x10, st);
    if (hr == 0) {
        rt_w32(o + 0x288, rt_r32(st));
        rt_w32(o + 0x28c, rt_r32(st + 4));
        rt_w32(o + 0x290, rt_r32(st + 8));
        for (uint32_t k = 0; k < 4; k++) rt_w8(o + 0x284 + k, rt_r8(st + 12 + k));
        rt_w32(o + 0x294, 0);
    } else if (hr == 0x8007001eu) {
        rt_w32(o + 0x278, 0);
        hr = com(rt_r32(o + 4), 0x1c, 0, 0, 0);
        if (hr) di_error(hr); else rt_w32(o + 0x278, 1);
    }
    rt_frame_pop(16);
    return hr;
}

/* 0x004c5d9c: the joystick library's caps record (JOYCAPS of the selected joystick) 0x00c8bba8 */
static uint32_t l_joy_caps(const uint32_t* a, int n) { (void)a; (void)n; return 0x00c8bba8u; }

void rt_bind_watcom_lib(void)
{
    rt_bind(0x004c5d9cu, l_joy_caps, "lib_joy_caps");
    rt_bind(0x004c3d90u, l_mouse_ctor, "lib_mouse_ctor");
    rt_bind(0x004c3e54u, l_mouse_open, "lib_mouse_open");
    rt_bind(0x004c4028u, l_mouse_close, "lib_mouse_close");
    rt_bind(0x004c407cu, l_mouse_poll, "lib_mouse_poll");
    rt_bind(0x004ca197u, l_open, "crt_open");
    rt_bind(0x004c9ff0u, l_read, "crt_read");
    rt_bind(0x004ca372u, l_filelength, "crt_filelength");
    rt_bind(0x004ca3cbu, l_close, "crt_close");
    rt_bind(0x004ca4e2u, l_lseek, "crt_lseek");
    rt_bind(0x004cbba0u, l_fill, "crt_fill");
    rt_bind(0x004c3913u, l_vfprintf, "crt_vfprintf");
    rt_bind(0x004c1132u, l_qsort, "crt_qsort");
    rt_bind(0x004c9f00u, l_ret, "lib_music_base_open");
    rt_bind(0x004c9f04u, l_music_base_ctor, "lib_music_base_ctor");
    rt_bind(0x004c9f83u, l_atoi, "crt_atoi");
    rt_bind(0x004c38f4u, l_set_c8b370, "lib_set_c8b370");
    rt_bind(0x004c4130u, l_mouse_x, "lib_mouse_dx");
    rt_bind(0x004c414cu, l_mouse_y, "lib_mouse_dy");
    rt_bind(0x004c4168u, l_mouse_z, "lib_mouse_dz");
    rt_bind(0x004c41c8u, l_mouse_b0, "lib_mouse_button0");
    rt_bind(0x004c41e8u, l_mouse_b2, "lib_mouse_button2");
    rt_bind(0x004c4208u, l_mouse_b1, "lib_mouse_button1");
    rt_bind(0x004c1024u, l_d3d_release, "lib_d3d_release");
    rt_bind(0x004c101cu, l_d3d_close, "lib_d3d_close");
}

RT_BIND_AREA(watcom_lib)
