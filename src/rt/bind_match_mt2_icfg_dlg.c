/*
 * bind_match_mt2_icfg_dlg.c: the key rebinding dialog 0x00462168..0x00462d90
 * (src/match/mt2_icfg_dlg.c), called the way tools/emu/diff_mt2_icfg_dlg.py called them: the
 * dialog object in eax (the destructor's flags in edx).
 *
 * text_local (a label text the original builds in its stack frame; a host string in the port):
 * copied to a 0x100 byte frame and passed to 0x004a6f08 (widget, text).
 */
#include "rt_env.h"
#include "match/mt2_icfg_dlg.h"

static uint8_t* d_resolve(void* u, uint32_t a) { (void)u; return rt_ptr(a); }

static uint32_t d_call(void* u, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                       const uint32_t* st, int k)
{
    (void)u;
    uint32_t a[4 + 8] = {eax, edx, ebx, ecx};
    if (k > 8) k = 8;
    for (int i = 0; i < k; i++) a[4 + i] = st[i];
    return rt_call(fn, a, 4 + k);
}

static void d_text_local(void* u, uint32_t w, const char* text)
{
    (void)u;
    uint32_t b = rt_frame_push(0x100);
    strncpy((char*)rt_ptr(b), text, 0xff);
    uint32_t a[2] = {w, b};
    rt_call(0x004a6f08u, a, 2);
    rt_frame_pop(0x100);
}

static const Mt2IcfgDlg D = {d_resolve, d_call, d_text_local, NULL};

#define R(i) rt_arg(a, n, i)
static uint32_t b_ctor(const uint32_t* a, int n) { return mt2_icfg_dlg_ctor(&D, R(0)); }
static uint32_t b_dtor(const uint32_t* a, int n) { return mt2_icfg_dlg_dtor(&D, R(0), R(1)); }
static uint32_t b_read_keyboard(const uint32_t* a, int n) { mt2_icfg_dlg_read_keyboard(&D, R(0)); return 0; }
static uint32_t b_read_dev4(const uint32_t* a, int n) { mt2_icfg_dlg_read_dev4(&D, R(0)); return 0; }
static uint32_t b_read_dev2(const uint32_t* a, int n) { mt2_icfg_dlg_read_dev2(&D, R(0)); return 0; }
static uint32_t b_build(const uint32_t* a, int n) { return mt2_icfg_dlg_build(&D, R(0)); }
static uint32_t b_ok(const uint32_t* a, int n) { return mt2_icfg_dlg_ok(&D, R(0)); }

void rt_bind_match_mt2_icfg_dlg(void)
{
    rt_bind(0x00462168u, b_ctor, "keydlg_ctor");
    rt_bind(0x004621f8u, b_dtor, "keydlg_dtor");
    rt_bind(0x004622fcu, b_read_keyboard, "keydlg_read_keyboard");
    rt_bind(0x00462500u, b_read_dev4, "keydlg_read_dev4");
    rt_bind(0x004627c4u, b_read_dev2, "keydlg_read_dev2");
    rt_bind(0x00462a88u, b_build, "keydlg_build");
    rt_bind(0x00462d90u, b_ok, "keydlg_ok");
}

RT_BIND_AREA(match_mt2_icfg_dlg)
