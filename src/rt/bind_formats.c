/*
 * bind_formats.c: the file format ports of src/formats (TIM, TPC, DPC), called the way
 * tools/emu/diff_tim.py / diff_tpc.py / diff_dpc.py called the original: buffers are game
 * memory, the ports get the host bytes of the game address (and the game address as their
 * addr_base, so the pointers they write are game addresses).
 *
 * The originals take no buffer size: the ports get the bytes from the buffer to the end of the
 * address space.
 */
#include "rt_env.h"

#include "formats/dpc.h"
#include "formats/tim.h"
#include "formats/tpc.h"

#define A(i) rt_arg(a, n, (i))

static uint32_t c3(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    uint32_t v[3] = {eax, edx, ebx};
    return rt_call(fn, v, 3);
}
static size_t room(uint32_t addr) { return (size_t)(RT_SIZE - (addr & (RT_SIZE - 1))); }

/* ---- TIM ---- */

/* 0x00474060(buf, clut out, image out): the 12 byte block headers; returns buf + the offset past
 * the image block */
static uint32_t b_tim_parse_headers(const uint32_t* a, int n)
{
    uint32_t buf = A(0);
    TimBlock clut, img;
    size_t end = tim_parse_headers(rt_ptr(buf), room(buf), &clut, &img);
    uint32_t outs[2] = {A(1), A(2)};
    const TimBlock* blk[2] = {&clut, &img};
    for (int k = 0; k < 2; k++) {
        rt_w32(outs[k], blk[k]->length);
        rt_w16(outs[k] + 4, blk[k]->x);
        rt_w16(outs[k] + 6, blk[k]->y);
        rt_w16(outs[k] + 8, blk[k]->w);
        rt_w16(outs[k] + 10, blk[k]->h);
    }
    return buf + (uint32_t)end;
}

/* 0x004978d8(pixels, count): 565 when 0x00496284() (the display's pixel format byte) is set */
static uint32_t b_tim_convert_pixels(const uint32_t* a, int n)
{
    int32_t count = (int32_t)A(1);
    int to_565 = (uint8_t)rt_call(0x00496284u, NULL, 0) != 0;
    if (count > 0) tim_convert_pixels(rt_ptr(A(0)), (size_t)count, to_565);
    return 0;
}

/* ---- TPC ---- */

#define TPC_PAGES 0x00a30c48u     /* 512 x 0x30 */
#define TPC_COUNTERS 0x00a36c48u  /* page_count, shared_count, counter_a, next_page */

/* 0x00496fd8(buf, append): the table is the original's (pages and counters copied in and out
 * around the port). append == 0 first releases the surfaces (0x004b270c), as the original; the
 * 555 -> 565 palette conversion when the screen is not split (0x004958ac() == 0) and the display
 * is 565 (0x00496284() == 1). */
static uint32_t b_tpc_register(const uint32_t* a, int n)
{
    static TpcTable t;
    uint32_t buf = A(0), append = A(1);
    if (append == 0) rt_call(0x004b270cu, NULL, 0);
    int conv = rt_call(0x004958acu, NULL, 0) == 0 && (uint8_t)rt_call(0x00496284u, NULL, 0) == 1;
    t.page_count = (int32_t)rt_r32(TPC_COUNTERS);
    t.shared_count = (int32_t)rt_r32(TPC_COUNTERS + 4);
    t.counter_a = (int32_t)rt_r32(TPC_COUNTERS + 8);
    t.next_page = (int32_t)rt_r32(TPC_COUNTERS + 12);
    memcpy(t.pages, rt_ptr(TPC_PAGES), sizeof t.pages);
    int r = tpc_register(&t, rt_ptr(buf), room(buf), buf, append != 0, conv);
    memcpy(rt_ptr(TPC_PAGES), t.pages, sizeof t.pages);
    rt_w32(TPC_COUNTERS, (uint32_t)t.page_count);
    rt_w32(TPC_COUNTERS + 4, (uint32_t)t.shared_count);
    rt_w32(TPC_COUNTERS + 8, (uint32_t)t.counter_a);
    rt_w32(TPC_COUNTERS + 12, (uint32_t)t.next_page);
    return (uint32_t)r;
}

/* ---- DPC ---- */

#define DPC_BASE 0x00a5eb60u   /* the relocation base, set by dpc_set_base 0x0049ee90 */

/* 0x0049eee0(root, file base, texture shift in bx): pointers become DPC_BASE + offset */
static uint32_t b_dpc_relocate_tree(const uint32_t* a, int n)
{
    uint32_t g = rt_r32(DPC_BASE);
    dpc_relocate_tree(rt_ptr(g), room(g), g, A(1), A(0), (int16_t)A(2));
    return 0;
}

/* 0x0049f440(node, texture shift in dx) */
static uint32_t b_dpc_shift_tex_pages(const uint32_t* a, int n)
{
    dpc_shift_tex_pages(rt_base, RT_SIZE, 0, A(0), (int16_t)A(1));
    return 0;
}

/* 0x004502b0(buf, texture shift): the original's root list loop (the port's dpc_relocate does the
 * same walk but without the calls between the roots): a bad header goes to 0x004a1ee1(-1); the
 * allocator 0x004a1314 gets header[4]; every root (relocated against buf, a null one ends it) is
 * relocated (0x0049eee0) and handed to 0x004a15d4(root, header[4], 0x0044f20c). */
static uint32_t b_dpc_load_relocate(const uint32_t* a, int n)
{
    uint32_t buf = A(0), shift = A(1) & 0xffffu;
    if (rt_r32(buf) != 0x4d504344u) c3(0x004a1ee1u, 0xffffffffu, buf, 0);
    if (rt_r32(buf + 4) != 0x43u) c3(0x004a1ee1u, 0xffffffffu, buf, 0);
    uint32_t cnt_alloc = rt_r32(buf + 0x10), base = rt_r32(buf + 0xc);
    c3(0x004a1314u, cnt_alloc, buf, 0);
    uint32_t list = dpc_reloc(rt_r32(buf + 0x14), base, buf);
    uint32_t count = rt_r32(list), p = list + 4;
    for (uint32_t i = 0; i < count; i++, p += 4) {
        uint32_t v = dpc_reloc(rt_r32(p), base, buf);
        rt_w32(p, v);
        if (v == 0) break;
        c3(0x0049eee0u, v, base, shift);
        c3(0x004a15d4u, v, cnt_alloc, 0x0044f20cu);
    }
    return 0;
}

void rt_bind_formats(void)
{
    rt_bind(0x00474060u, b_tim_parse_headers, "tim_parse_headers");
    rt_bind(0x004978d8u, b_tim_convert_pixels, "tim_convert_pixels");
    rt_bind(0x00496fd8u, b_tpc_register, "tpc_register");
    rt_bind(0x0049eee0u, b_dpc_relocate_tree, "dpc_relocate_tree");
    rt_bind(0x0049f440u, b_dpc_shift_tex_pages, "dpc_shift_tex_pages");
    rt_bind(0x004502b0u, b_dpc_load_relocate, "dpc_load_relocate");
}

RT_BIND_AREA(formats)
