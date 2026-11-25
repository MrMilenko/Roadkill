/*
 * bind_match_extra.c: the leftovers 0x00462d34, 0x004671e8, 0x00469b34, 0x0046ec54, 0x0046ec80
 * (src/match/mt_extra.c), called the way tools/emu/diff_match_extra.py called them (one argument
 * in EAX).
 *
 * list_text 0x004a559c (widget, index, buffer): the port passes its host stack buffer as `ptr`
 * (the harness only checked it was there); the original passes its stack buffer in EBX. Here
 * the buffer is a frame of 0x80 bytes, copied back to the port's buffer after the call.
 */
#include "rt_env.h"
#include "match/mt_extra.h"

static uint32_t xcall(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    uint32_t a[4] = {eax, edx, ebx, 0};
    return rt_call(fn, a, 4);
}
#define XCB(nm, addr) \
    static uint32_t nm(void* u, uint32_t eax, uint32_t edx, uint32_t ebx, void* ptr) \
    { (void)u; (void)ptr; return xcall((addr), eax, edx, ebx); }

XCB(cb_text_set, 0x004a6f08u)
XCB(cb_net_on, 0x004aaa88u)
XCB(cb_net_off, 0x004aaaccu)
XCB(cb_list_select, 0x004a3964u)
XCB(cb_page_1, 0x004622fcu)
XCB(cb_page_2, 0x004627c4u)
XCB(cb_page_4, 0x00462500u)
XCB(cb_list_cursel, 0x004a54ccu)

static uint32_t cb_list_text(void* u, uint32_t eax, uint32_t edx, uint32_t ebx, void* ptr)
{
    (void)u;
    if (!ptr) return xcall(0x004a559cu, eax, edx, ebx);
    uint32_t f = rt_frame_push(0x80u);
    uint32_t r = xcall(0x004a559cu, eax, edx, f);
    memcpy(ptr, rt_ptr(f), 0x80u);
    rt_frame_pop(0x80u);
    return r;
}

static uint8_t* x_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

static const MtExtra* env(void)
{
    static MtExtra e;
    static int done;
    if (!done) {
        e.disp_960 = (const int32_t*)rt_ptr(0x0058a960u);
        e.disp_950 = (int32_t*)rt_ptr(0x0058a950u);
        e.disp_954 = (int32_t*)rt_ptr(0x0058a954u);
        e.net_flag = (int32_t*)rt_ptr(0x0050fa24u);
        e.resolve = x_resolve;
        e.text_set = cb_text_set;
        e.net_on = cb_net_on;
        e.net_off = cb_net_off;
        e.list_select = cb_list_select;
        e.page_1 = cb_page_1;
        e.page_2 = cb_page_2;
        e.page_4 = cb_page_4;
        e.list_cursel = cb_list_cursel;
        e.list_text = cb_list_text;
        done = 1;
    }
    return &e;
}

#define R0 rt_arg(a, n, 0)
static uint32_t b_page_dispatch(const uint32_t* a, int n) { return (uint32_t)mt_page_dispatch(env(), R0); }
static uint32_t b_net_set_flag(const uint32_t* a, int n) { mt_net_set_flag(env(), (int32_t)R0); return 0; }
static uint32_t b_list_read_selected(const uint32_t* a, int n) { mt_list_read_selected(env(), R0); return 0; }
static uint32_t b_display_text2_off(const uint32_t* a, int n) { mt_display_text2_off(env(), R0); return 0; }
static uint32_t b_display_text2_on(const uint32_t* a, int n) { mt_display_text2_on(env(), R0); return 0; }

void rt_bind_match_extra(void)
{
    rt_bind(0x00462d34u, b_page_dispatch, "page_dispatch");
    rt_bind(0x004671e8u, b_net_set_flag, "net_set_flag");
    rt_bind(0x00469b34u, b_list_read_selected, "list_read_selected");
    rt_bind(0x0046ec54u, b_display_text2_off, "display_text2_off");
    rt_bind(0x0046ec80u, b_display_text2_on, "display_text2_on");
}

RT_BIND_AREA(match_extra)
