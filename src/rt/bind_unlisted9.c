/*
 * bind_unlisted9.c: unlisted 0x00485480, 0x004854c0, 0x004858d0, 0x004a5234 (scene / screen vtable slots and a
 * constructor), ported in src/platform/pf8_sceneops.c; the four registers (compact: Watcom register arguments).
 */
#include "rt_env.h"
#include "platform/pf8_sceneops.h"

#define B(name, fn) \
    static uint32_t name(const uint32_t* a, int n) \
    { \
        Pf2Env e = rt_pf2env(); \
        uint32_t r = fn(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3)); \
        rt_en2env_done(); \
        return r; \
    }
B(b_scene_flag, pf8_00485480)
B(b_scene_draw, pf8_004854c0)
B(b_scene_next, pf8_004858d0)
B(b_object_ctor, pf8_004a5234)

void rt_bind_unlisted9(void)
{
    rt_bind(0x00485480u, b_scene_flag, "unlisted_scene_flag");
    rt_bind(0x004854c0u, b_scene_draw, "unlisted_scene_draw");
    rt_bind(0x004858d0u, b_scene_next, "unlisted_scene_next");
    rt_bind(0x004a5234u, b_object_ctor, "unlisted_object_ctor");
}

RT_BIND_AREA(unlisted9)
