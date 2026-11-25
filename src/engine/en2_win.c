/*
 * en2_win.c: widget creation, registry and input mode helpers of TM2.EXE 0x00488000..0x004b0000
 * (see en2_win.h). Verified under emulation: tools/emu/check_en2_win.sh.
 */
#include "en2_win.h"
#include "en2_leaf.h"
#include "en_misc.h"

#define EN2_lstrlenA 0x00c90320u
#define REG_PATH     0x00bdfb38u   /* the registry key path buffer */
#define REG_DEFAULT  0x004e35b4u   /* "Software\SingleTrac\default" in the EXE */

uint32_t en2_reg_set_path(En2Env* e, uint32_t o, uint32_t name)
{
    if (name != 0) {
        en2_call2(e, EN2_lstrcpyA, REG_PATH, name);
    } else if (en2_r32(e, 0x004e8528u) == 1) {
        en2_w32(e, 0x004e8528u, 0);
        en2_call2(e, EN2_lstrcpyA, REG_PATH, REG_DEFAULT);
    }
    return o;
}
uint32_t en2_reg_default_path(En2Env* e, uint32_t v)
{
    if (v != 0) {
        en2_call2(e, EN2_lstrcpyA, REG_PATH, v);
        return 0;
    }
    if (en2_r32(e, 0x004e8528u) == 1) {
        en2_w32(e, 0x004e8528u, 0);
        en2_call2(e, EN2_lstrcpyA, REG_PATH, REG_DEFAULT);
    }
    return 1;
}
uint32_t en2_reg_write_string(En2Env* e, uint32_t o, uint32_t name, uint32_t str)
{
    uint32_t len = en2_call1(e, EN2_lstrlenA, str);
    return en2_call6(e, EN2_RegSetValueExA, en2_r32(e, o), name, 0, 1 /* REG_SZ */, str, len);
}
uint32_t en2_widget_create_4a4264(En2Env* e, uint32_t a, uint32_t b)
{
    uint32_t o = en2_call2(e, 0x004a3fa0u, a, b);
    for (uint32_t k = 0; k < 4; k++) en2_component_clear(e, o + 0x6c + 0x14 * k);
    en2_w32(e, o + 0x34, 0x004e5e70u);
    en2_w32(e, o + 0x54, 0);
    en2_w32(e, o + 0x58, 0);
    en2_w32(e, o + 0x5c, 0);
    en2_w32(e, o + 0x60, 0);
    en2_w32(e, o + 0x64, 0);
    return o;
}
uint32_t en2_list_select_current(En2Env* e, uint32_t o)
{
    uint32_t r = en2_call4(e, EN2_SendMessageA, en2_r32(e, o), 0x188 /* LB_GETCURSEL */, 0, 0);
    return en2_call4(e, EN2_SendMessageA, en2_r32(e, o), 0x199, r, 0);
}
uint32_t en2_node_list_free(En2Env* e, uint32_t o)
{
    uint32_t p = en2_r32(e, o);
    en2_w32(e, o + 8, p);
    while (p != 0) {
        p = en2_r32(e, o + 8);
        uint32_t next = en2_r32(e, p + 0x94);
        en2_call1(e, 0x004aa39cu, p);
        en2_w32(e, o + 8, next);
        p = next;
    }
    en2_w32(e, o + 4, 0);
    en2_w32(e, o, 0);
    return o;
}
static uint32_t create(En2Env* e, uint32_t o, uint32_t cls, uint32_t edx, uint32_t ebx, uint32_t s0, uint32_t s1,
                       uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    uint32_t v[10] = {o, cls, edx, ebx, s0, s1, x, y, w, h};
    return e->call(e->user, 0x004a4068u, v, 10);
}
uint32_t en2_create_listview(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    return create(e, o, 0x004e311du, edx, ebx, 0x004e311cu, ecx, x, y, w, h);
}
uint32_t en2_create_combobox(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    return create(e, o, 0x004e3107u, edx, ebx, 0x004e3106u, ecx | 0x40200000u, x, y, w, h);
}
uint32_t en2_create_trackbar(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    return create(e, o, 0x004e312cu, edx, ebx, 0x004e312bu, ecx | 0x40000000u, x, y, w, h);
}
uint32_t en2_create_edit(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t style, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    return create(e, o, 0x004e3110u, edx, ebx, ecx, style | 0x40000000u, x, y, w, h);
}
uint32_t en2_key_release(En2Env* e, uint32_t k)
{
    int32_t* mode = (int32_t*)(void*)e->mem(e->user, 0x004e8434u);
    if (en2_get_a5d370(e) == 1 && en_render_mode_get(mode) == 2)
        en2_w8(e, 0x00a5d710u + en2_r16(e, 0x004e9240u + 2u * k), 0);
    if (en_render_mode_get(mode) == 2 && k == 0x10) {
        en2_set_a5da20(e, 0);
        en2_call1(e, 0x0049ed48u, en2_r8(e, 0x004e9240u + 2u * k));
    }
    return 1;
}
uint32_t en2_input_mode_set(En2Env* e, uint32_t v)
{
    if (v == 0) {
        en2_w32(e, 0x00a5d370u, 0);
        en2_display_mode(e, 0);
        en2_input_release(e);
        return en2_display_496190(e, 1);
    }
    if (v != 1 && v != 2) return v;
    en2_w32(e, 0x00a5d370u, v);
    en2_display_mode(e, 1);
    return en2_display_496190(e, v == 1 ? 0 : 1);
}
uint32_t en2_create_static(En2Env* e, uint32_t o, uint32_t edx, uint32_t ebx, uint32_t ecx, uint32_t style, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    return create(e, o, 0x004e3115u, edx, ebx, ecx, style | 0x40000000u, x, y, w, h);
}
