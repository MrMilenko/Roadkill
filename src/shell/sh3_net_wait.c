/*
 * sh3_net_wait.c: the network car wait screen, TM2.EXE 0x0047e010 (see sh3_net_wait.h and
 * sh3_screen.h for the frame convention). The scene definition is the original's frame as it
 * stands at each call, final values only.
 */
#include "sh3_net_wait.h"

#define R8(a) sh_r8(e, a)
#define R32(a) sh_r32(e, a)
#define W8(a, v) sh_w8(e, a, v)
#define W16(a, v) sh_w16(e, a, v)
#define W32(a, v) sh_w32(e, a, v)
#define FB(o) (fb + (uint32_t)(o))
#define F8(o, v) W8(FB(o), v)
#define F16(o, v) W16(FB(o), v)
#define F32(o, v) W32(FB(o), v)
#define C0(f) sh3_call(e, f, 0, 0, 0, 0, 0, 0)
#define C1(f, a) sh3_call(e, f, 1, a, 0, 0, 0, 0)
#define C2(f, a, b) sh3_call(e, f, 2, a, b, 0, 0, 0)
#define C3(f, a, b, c) sh3_call(e, f, 3, a, b, c, 0, 0)
#define C5(f, a, b, c, d, x) sh3_call(e, f, 5, a, b, c, d, x)

static const Sh3Action actions[11] = {
    {0x0f, 0x000, 2, 0x25c}, {0x0f, 0x000, 1, 0x238}, {0x0f, 0x000, 0, 0x2cc}, {0x10, 0x000, 2, 0x25c},
    {0x10, 0x000, 1, 0x238}, {0x10, 0x000, 0, 0x2e0}, {0x00, 0x000, 0, 0x2b8}, {0x00, 0x000, 0, 0x29c},
    {0x00, 0x000, 0, 0x238}, {0x00, 0x000, 0, 0x308}, {0x02, 0x308, 2, 0x2f4},
};

static void layout1(const ShEnv* e, uint32_t fb) {
    /* +0x134 */
    F16(0x134, 0); F32(0x138, 0); F32(0x13c, 0x4e5740); F16(0x140, 1);
    F16(0x142, 1); F16(0x144, 0xffff); F32(0x148, 1); F32(0x14c, 0);
    F32(0x150, 0); F32(0x154, 0); F32(0x158, 0); F32(0x15c, 0);
    F32(0x160, 0); F32(0x164, 0); F32(0x178, 0); F32(0x180, 0);
    F8(0x184, 6); F32(0x188, 0x4e2997); F8(0x18c, 1); F16(0x18e, 0x140);
    F16(0x190, 0x19a);
    /* +0x194 */
    F16(0x194, 0); F32(0x198, 0); F32(0x19c, 0x4e5730); F16(0x1a0, 1);
    F16(0x1a2, 1); F16(0x1a4, 0xffff); F32(0x1a8, 1); F32(0x1ac, 0);
    F32(0x1b0, 0); F32(0x1b4, 0); F32(0x1b8, 0); F32(0x1bc, 0);
    F32(0x1c0, 0); F32(0x1c4, 0); F32(0x1d8, 0); F32(0x1e0, 0);
    F32(0x1e4, 0);
    /* +0x1e8 */
    F16(0x1e8, 0x232d); F32(0x1ec, 0); F32(0x1f0, 0x4e57a0); F16(0x1f4, 1);
    F16(0x1f6, 1); F16(0x1f8, 0xffff); F32(0x1fc, 1); F32(0x200, 0);
    F32(0x204, 0); F32(0x208, 0xfffffc90); F32(0x20c, 0xffffffb0); F32(0x210, 0);
    F32(0x214, 0); F32(0x218, 0); F32(0x22c, 0); F32(0x234, 0);
}

static void layout2(const ShEnv* e, uint32_t fb) {
    uint32_t g626010 = R32(0x00626010u);
    /* +0x238 */
    F32(0x238, 0xffffffff); F32(0x23c, 0xffffffff); F32(0x240, 0x4e575c); F32(0x244, 0x50fad8);
    F32(0x248, 0); F32(0x250, 0); F32(0x264, 0x4e5794); F32(0x26c, 0x1d);
    F32(0x270, 0); F32(0x274, 0); F32(0x278, g626010); F32(0x284, 0x4e5794);
    F32(0x28c, 0x1c); F32(0x290, 0); F32(0x294, 0); F32(0x298, g626010);
    /* +0x29c */
    F32(0x29c, 0xffffffff); F32(0x2a0, 0xffffffff); F32(0x2a4, 0x4e5778); F32(0x2a8, FB(0x1e8));
    F32(0x2ac, 0x14); F32(0x2b0, 0); F32(0x2b4, 0); F32(0x2c0, 0x4e5770);
    F32(0x2c4, 0x50fad8); F32(0x2c8, FB(0x318)); F32(0x2d4, 0x4e5760); F32(0x2d8, 0x50fad8);
    F8(0x2dc, 1); F32(0x2e8, 0x4e5760); F32(0x2ec, 0x50fad8); F8(0x2f0, 2);
    F32(0x2fc, 0x4e5760); F32(0x300, 0x50fad8); F8(0x304, 0); F32(0x310, 0x4e572c);
    F32(0x314, 0x50fad8); F32(0x318, 0x5a); F32(0x31c, 0x96); F32(0x320, 0x226);
    F32(0x324, 0x1ae);
    W32(0x0050fb24u, FB(0));    /* the scene image record (frame +0) */
}

uint32_t sh3_net_wait(const ShEnv* e, uint32_t fb) {
    C5(0x00469f08u, SH3_SCENE, 0x004780fcu, 0x0047a18cu, 0, 0);    /* sceneInit */
    W32(0x0050fae8u, 0); W32(0x0050faecu, 0xfffffb50u); W32(0x0050faf0u, 0x50);
    W16(0x0050faf4u, 0xaa); W16(0x0050faf6u, 0); W16(0x0050faf8u, 0);
    C3(0x00469c40u, FB(0), 0x004e2983u, 0x2e630);   /* image record "SHELLDB1\CARSEL.TIM" */
    layout1(e, fb);
    C0(0x00452d28u);
    layout2(e, fb);
    C2(0x0046a138u, SH3_SCENE, FB(0x1e8));
    C2(0x0046a138u, SH3_SCENE, FB(0x194));
    sh3_add_actions(e, fb, actions, 11);
    C2(0x0046a324u, SH3_SCENE, 0x004e2998u);   /* scene resources */
    C1(0x00469c78u, FB(0));
    F32(0x248, 1);
    C2(0x00469cc4u, FB(0x1e8), R8(0x00624eadu));
    C1(0x00463514u, 0x10);                     /* session_set_current */
    uint32_t r = C1(0x0046a5fcu, SH3_SCENE);   /* sceneRun */
    C0(0x00463bdcu);                           /* session_player_bytes */
    if (C0(0x004632d4u)) C2(0x00474154u, 1000000, 1000000);
    if (R8(0x0050fb1cu) == 0) r = 0x00485100u;
    F32(0x13c, 0x004e5740u);
    C1(0x00452d48u, R8(FB(0x184)));            /* font color byte set */
    return r;
}
