/*
 * bind_unlisted15.c: unlisted 0x0047a308 (the network screen step) and the list view wrappers 0x004a616c,
 * 0x004a6390, 0x004a641c, 0x004a6480, ported in src/platform/pf8_netscreen.c. Compact: four register
 * arguments, then the stack words at a[4..] (0x004a616c four, 0x004a641c / 0x004a6480 one).
 */
#include "rt_env.h"
#include "platform/pf8_netscreen.h"

#define A(i) rt_arg(a, n, i)
static uint32_t b_screen_network(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_0047a308(&e, A(0), A(1), A(2), A(3));
    rt_en2env_done();
    return r;
}

static uint32_t b_listview_ctor(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_004a616c(&e, A(0), A(1), A(2), A(3), A(4), A(5), A(6), A(7));
    rt_en2env_done();
    return r;
}

static uint32_t b_listview_item_data(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_004a6390(&e, A(0), A(1), A(2), A(3));
    rt_en2env_done();
    return r;
}

static uint32_t b_listview_item_text(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_004a641c(&e, A(0), A(1), A(2), A(3), A(4));
    rt_en2env_done();
    return r;
}

static uint32_t b_listview_insert_column(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_004a6480(&e, A(0), A(1), A(2), A(3), A(4));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted15(void)
{
    rt_bind(0x0047a308u, b_screen_network, "unlisted_screen_network");
    rt_bind(0x004a616cu, b_listview_ctor, "unlisted_listview_ctor");
    rt_bind(0x004a6390u, b_listview_item_data, "unlisted_listview_item_data");
    rt_bind(0x004a641cu, b_listview_item_text, "unlisted_listview_item_text");
    rt_bind(0x004a6480u, b_listview_insert_column, "unlisted_listview_insert_column");
}

RT_BIND_AREA(unlisted15)
