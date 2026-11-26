/*
 * bind_engine_widget.c: the small widget / menu accessors (src/engine/en_widget.c), called as
 * tools/emu/diff_engine_widget.py called them: EAX, EDX, EBX, ECX in the port's parameter order,
 * the stack words where the port takes s[3]. EnWidgetEnv reaches game memory through rt_mem_cb.
 */
#include "rt_env.h"
#include "engine/en_widget.h"

static EnWidgetEnv W = {rt_mem_cb, NULL};
#define A(i) rt_arg(a, n, (i))
#define STK3(s) uint32_t s[3] = {A(4), A(5), A(6)}       /* four register arguments */
#define STK3_2(s) uint32_t s[3] = {A(2), A(3), A(4)}     /* two register arguments (EAX, EDX) */

/* one arg (EAX i), byte result */
#define G1(name, fn) static uint32_t name(const uint32_t* a, int n) { return fn(&W, A(0)); }
/* no args */
#define G0(name, fn) static uint32_t name(const uint32_t* a, int n) { (void)a; (void)n; fn(&W); return 0; }
/* EAX obj, EDX v, setter */
#define S2(name, fn) static uint32_t name(const uint32_t* a, int n) { fn(&W, A(0), A(1)); return 0; }
/* EAX obj, EDX v, returns */
#define R2(name, fn) static uint32_t name(const uint32_t* a, int n) { return fn(&W, A(0), A(1)); }

G1(b_pair_get_a, en_wg_pair_get_a)          /* 0x004a1f8c */
G1(b_pair_get_b, en_wg_pair_get_b)          /* 0x004a2008 */
G1(b_t204c_get, en_wg_t204c_get)            /* 0x004a2214 */
G1(b_t23fc_get, en_wg_t23fc_get)            /* 0x004a2758 */
G1(b_t247c_get, en_wg_t247c_get)            /* 0x004a27c8 */
G0(b_pair_clear, en_wg_pair_clear)          /* 0x004a1f9c */
G0(b_t204c_clear, en_wg_t204c_clear)        /* 0x004a2220 */
G0(b_t214c_clear, en_wg_t214c_clear)        /* 0x004a22cc */
G0(b_t23fc_clear, en_wg_t23fc_clear)        /* 0x004a2764 */
G0(b_t24fc_clear, en_wg_t24fc_clear)        /* 0x004a2810 */
G0(b_set_c8b9f0, en_wg_set_c8b9f0)          /* 0x004a2e38 */
G0(b_pair_commit, en_wg_pair_commit)        /* 0x004a2018 */
G0(b_t204c_commit, en_wg_t204c_commit)      /* 0x004a2290 */
G0(b_t23fc_commit, en_wg_t23fc_commit)      /* 0x004a27d4 */

#ifdef ROADKILL_XBOX
/* The Xbox's pad layout (port/xbox; the console's default where the desktop keeps the 1997 one).
 * 0x004a27d4 copies the joystick defaults 0x00a8247c into the joystick map 0x00a823fc (the first
 * run, a different controller, Input Config's default): the defaults are the console's here. The
 * map is indexed by the game function (the Input Config rows' ids, 0x004e7a44) and holds the
 * joystick input: 1.. the buttons in input_joy.c's order (1 A, 2 B, 3 X, 4 Y, 5 White, 6 Black,
 * 7 Back, 8 Start, 11 / 12 the left / right trigger, 13..16 the d-pad up, down, left, right),
 * 0x21..0x24 the stick right / left / back / forward. */
static uint32_t b_t23fc_commit_xbox(const uint32_t* a, int n)
{
    static const uint8_t pad[0x80] = {
        [0x22] = 0x21, [0x23] = 0x22,   /* turn right / left: the left stick */
        [0x24] = 12, [0x25] = 11,       /* accelerate / brakes: the right / left trigger */
        [0x26] = 0x24, [0x27] = 0x23,   /* idle forward / reverse: the stick forward / back */
        [0x28] = 4,                     /* tight turn: Y */
        [0x29] = 2,                     /* turbo: B */
        [0x2a] = 1,                     /* machine guns: A */
        [0x2b] = 3,                     /* fire weapons: X */
        [0x2c] = 6, [0x2d] = 5,         /* next / previous weapon: Black / White */
        [0x4a] = 13, [0x4c] = 14,       /* radar / enemy list: d-pad up / down */
        [0x3c] = 15, [0x3d] = 16,       /* previous / next VR mode: d-pad left / right */
    };
    memcpy(rt_ptr(0x00a8247cu), pad, sizeof pad);
    return b_t23fc_commit(a, n);
}
#endif

/* 0x004a1ff8 (EAX i, DL v, EBX ofs) */
static uint32_t b_pair_set_b(const uint32_t* a, int n) { en_wg_pair_set_b(&W, A(0), (uint8_t)A(1), A(2)); return 0; }
/* 0x004a2278 / 0x004a274c / 0x004a27bc (EAX i, DL v) */
static uint32_t b_t20cc_set(const uint32_t* a, int n) { en_wg_t20cc_set(&W, A(0), (uint8_t)A(1)); return 0; }
static uint32_t b_t23fc_set(const uint32_t* a, int n) { en_wg_t23fc_set(&W, A(0), (uint8_t)A(1)); return 0; }
static uint32_t b_t247c_set(const uint32_t* a, int n) { en_wg_t247c_set(&W, A(0), (uint8_t)A(1)); return 0; }

G1(b_pair_table, en_wg_pair_table)          /* 0x004a217c (EAX size_out) */
G1(b_t204c_table, en_wg_t204c_table)        /* 0x004a25ec */
G1(b_t23fc_table, en_wg_t23fc_table)        /* 0x004a2d9c */

/* 0x004a8fe8 (EAX kind, EDX i): only AL is defined */
static uint32_t b_get_focus(const uint32_t* a, int n) { return en_wg_get_focus(&W, (int32_t)A(0), A(1)); }
/* 0x004a2e28 */
static uint32_t b_field4_addr(const uint32_t* a, int n) { return en_wg_field4_addr(A(0)); }
/* 0x004a2e2c (EAX obj, EDX i) */
static uint32_t b_entry(const uint32_t* a, int n) { return en_wg_entry(&W, A(0), (int32_t)A(1)); }
/* 0x004a2e43 (EAX node) */
static uint32_t b_list_push(const uint32_t* a, int n) { en_wg_list_push(&W, A(0)); return 0; }
/* 0x004a2e60 / 0x004a3620 leave EAX = the object, and their callers use it (the static init
 * 0x00477640 writes the derived vtable to [eax + 0x30]): answering 0 left the background
 * widgets with the base vtable, a pure virtual call on the network / audio / input screens */
static uint32_t b_reset_a(const uint32_t* a, int n) { en_wg_reset_a(&W, A(0)); return A(0); }   /* 0x004a2e60 */
static uint32_t b_reset_b(const uint32_t* a, int n) { en_wg_reset_b(&W, A(0)); return A(0); }   /* 0x004a3620 */
static uint32_t b_clear_2a4(const uint32_t* a, int n) { en_wg_clear_2a4(&W, A(0)); return 0; }  /* 0x004a3d88 */
static uint32_t b_zero(const uint32_t* a, int n) { (void)a; (void)n; return en_wg_zero(); }     /* 0x004a2ff0 */
G1(b_word0, en_wg_word0)                    /* 0x004a2ff4, 0x004a6dcc */
G1(b_word1, en_wg_word1)                    /* 0x004a2ff8 */
G1(b_word3, en_wg_word3)                    /* 0x004a6dd4 */
G1(b_get_c4, en_wg_get_c4)                  /* 0x004a4a78 */
G1(b_cur_block, en_wg_cur_block)            /* 0x004a81e8 */
static uint32_t b_cur_52c(const uint32_t* a, int n) { return en_wg_cur_word(&W, A(0), 0x52c); }   /* 0x004a822c */
static uint32_t b_cur_530(const uint32_t* a, int n) { return en_wg_cur_word(&W, A(0), 0x530); }   /* 0x004a8240 */
static uint32_t b_cur_53b0(const uint32_t* a, int n) { return en_wg_cur_word(&W, A(0), 0x53b0); } /* 0x004a825c */
R2(b_set_3c, en_wg_set_3c)                  /* 0x004a4168 */
R2(b_set_40, en_wg_set_40)                  /* 0x004a4174 */
R2(b_set_44, en_wg_set_44)                  /* 0x004a41ac */
S2(b_set_48, en_wg_set_48)                  /* 0x004a41b8 */
S2(b_set_54z_58, en_wg_set_54z_58)          /* 0x004a4478 */
S2(b_set_64z_54, en_wg_set_64z_54)          /* 0x004a595c */
S2(b_set_78z_68, en_wg_set_78z_68)          /* 0x004a5988 */
/* 0x004a4458 / 0x004a593c / 0x004a5968 (EAX obj, EDX v, 3 stack words: a[2 .. 4]) */
static uint32_t b_set_54_5c(const uint32_t* a, int n) { STK3_2(s); return en_wg_set_54_5c(&W, A(0), A(1), s); }
static uint32_t b_set_64_58(const uint32_t* a, int n) { STK3_2(s); return en_wg_set_64_58(&W, A(0), A(1), s); }
static uint32_t b_set_78_6c(const uint32_t* a, int n) { STK3_2(s); return en_wg_set_78_6c(&W, A(0), A(1), s); }
/* 0x004a4de0 (EAX obj, EDX, EBX) */
static uint32_t b_set_colors(const uint32_t* a, int n) { en_wg_set_colors(&W, A(0), A(1), A(2)); return 0; }
/* 0x004a6b78 (EAX v) */
static uint32_t b_store_instance(const uint32_t* a, int n) { en_wg_store_instance(&W, A(0)); return 0; }
/* 0x004a8078 (EAX obj, EDX i) */
static uint32_t b_flag_2a4(const uint32_t* a, int n) { return en_wg_flag_2a4(&W, A(0), (int32_t)A(1)); }
/* 0x004a8f40 */
static uint32_t b_get_bdfb24(const uint32_t* a, int n) { (void)a; (void)n; return en_wg_get_bdfb24(&W); }

/* 0x004a809c .. 0x004a81a0: axis k of EAX obj; positive getters define AL, negative AX */
#define AX(k) \
    static uint32_t b_axis##k##_pos(const uint32_t* a, int n) { return en_wg_axis_pos(&W, A(0), k); } \
    static uint32_t b_axis##k##_neg(const uint32_t* a, int n) { return en_wg_axis_neg(&W, A(0), k); }
AX(0) AX(1) AX(2) AX(3) AX(4) AX(5)

G1(b_pair_flags, en_wg_pair_flags)          /* 0x004a2084 */
G1(b_t204c_map, en_wg_t204c_map)            /* 0x004a244c */
G1(b_t23fc_map, en_wg_t23fc_map)            /* 0x004a28ac */
G1(b_image_setup, en_wg_image_setup)        /* 0x004a3438 */
G1(b_find_80, en_wg_find_80)                /* 0x004a3e24 */
/* 0x004a70ac (EAX obj, EDX, EBX, ECX, 3 stack words) */
static uint32_t b_set_nav_links(const uint32_t* a, int n)
{
    STK3(s);
    return en_wg_set_nav_links(&W, A(0), A(1), A(2), A(3), s);
}
/* 0x004a70fc (EAX obj, EDX i) */
static uint32_t b_nav_link(const uint32_t* a, int n) { return en_wg_nav_link(&W, A(0), (int32_t)A(1)); }
/* 0x004a7110 (EDX v) */
static uint32_t b_set_bdfaec(const uint32_t* a, int n) { en_wg_set_bdfaec(&W, A(1)); return 0; }
/* 0x004a81b8 (EAX obj, EDX i, EBX v) */
static uint32_t b_slot_294(const uint32_t* a, int n) { return en_wg_slot_294(&W, A(0), (int32_t)A(1), (int32_t)A(2)); }

void rt_bind_engine_widget(void)
{
    rt_bind(0x004a1f8cu, b_pair_get_a, "widget_get_label");
    rt_bind(0x004a1f9cu, b_pair_clear, "pair_table_clear");
    rt_bind(0x004a1ff8u, b_pair_set_b, "pair_table_set_b");
    rt_bind(0x004a2008u, b_pair_get_b, "widget_get_next");
    rt_bind(0x004a2018u, b_pair_commit, "pair_table_commit");
    rt_bind(0x004a2084u, b_pair_flags, "pair_table_flags");
    rt_bind(0x004a217cu, b_pair_table, "pair_table_get");
    rt_bind(0x004a2214u, b_t204c_get, "table_204c_get");
    rt_bind(0x004a2220u, b_t204c_clear, "table_204c_clear");
    rt_bind(0x004a2278u, b_t20cc_set, "table_20cc_set");
    rt_bind(0x004a2290u, b_t204c_commit, "table_204c_commit");
    rt_bind(0x004a22ccu, b_t214c_clear, "table_214c_clear");
    rt_bind(0x004a244cu, b_t204c_map, "table_204c_map");
    rt_bind(0x004a25ecu, b_t204c_table, "table_204c_info");
    rt_bind(0x004a274cu, b_t23fc_set, "table_23fc_set");
    rt_bind(0x004a2758u, b_t23fc_get, "widget_get_prev");
    rt_bind(0x004a2764u, b_t23fc_clear, "table_23fc_clear");
    rt_bind(0x004a27bcu, b_t247c_set, "table_247c_set");
    rt_bind(0x004a27c8u, b_t247c_get, "widget_get_value");
#ifdef ROADKILL_XBOX
    rt_bind(0x004a27d4u, b_t23fc_commit_xbox, "table_23fc_commit");
#else
    rt_bind(0x004a27d4u, b_t23fc_commit, "table_23fc_commit");
#endif
    rt_bind(0x004a2810u, b_t24fc_clear, "table_24fc_clear");
    rt_bind(0x004a28acu, b_t23fc_map, "table_23fc_map");
    rt_bind(0x004a2d9cu, b_t23fc_table, "table_23fc_info");
    rt_bind(0x004a2e28u, b_field4_addr, "obj_field4_addr");
    rt_bind(0x004a2e2cu, b_entry, "obj_entry");
    rt_bind(0x004a2e38u, b_set_c8b9f0, "set_c8b9f0");
    rt_bind(0x004a2e43u, b_list_push, "list_push_4e91e8");
    rt_bind(0x004a2e60u, b_reset_a, "obj_reset_a");
    rt_bind(0x004a2ff0u, b_zero, "return_zero");
    rt_bind(0x004a2ff4u, b_word0, "obj_word0");
    rt_bind(0x004a2ff8u, b_word1, "obj_word1");
    rt_bind(0x004a3438u, b_image_setup, "image_setup");
    rt_bind(0x004a3620u, b_reset_b, "obj_reset_b");
    rt_bind(0x004a3d88u, b_clear_2a4, "obj_clear_2a4");
    rt_bind(0x004a3e24u, b_find_80, "obj_find_80");
    rt_bind(0x004a4168u, b_set_3c, "obj_set_3c");
    rt_bind(0x004a4174u, b_set_40, "obj_set_40");
    rt_bind(0x004a41acu, b_set_44, "obj_set_44");
    rt_bind(0x004a41b8u, b_set_48, "obj_set_48");
    rt_bind(0x004a4458u, b_set_54_5c, "obj_set_54_5c");
    rt_bind(0x004a4478u, b_set_54z_58, "obj_set_58");
    rt_bind(0x004a4a78u, b_get_c4, "obj_get_c4");
    rt_bind(0x004a4de0u, b_set_colors, "widget_set_colors");
    rt_bind(0x004a593cu, b_set_64_58, "obj_set_64_58");
    rt_bind(0x004a595cu, b_set_64z_54, "obj_set_54");
    rt_bind(0x004a5968u, b_set_78_6c, "obj_set_78_6c");
    rt_bind(0x004a5988u, b_set_78z_68, "obj_set_68");
    rt_bind(0x004a6b78u, b_store_instance, "win32_store_instance");
    rt_bind(0x004a6dccu, b_word0, "ddraw_get_hwnd");
    rt_bind(0x004a6dd4u, b_word3, "ddraw_get_accel");
    rt_bind(0x004a70acu, b_set_nav_links, "widget_set_nav_links");
    rt_bind(0x004a70fcu, b_nav_link, "widget_get_nav_link");
    rt_bind(0x004a7110u, b_set_bdfaec, "set_bdfaec");
    rt_bind(0x004a8078u, b_flag_2a4, "obj_flag_2a4");
    rt_bind(0x004a809cu, b_axis0_pos, "obj_axis0_pos");
    rt_bind(0x004a80b0u, b_axis0_neg, "obj_axis0_neg");
    rt_bind(0x004a80ccu, b_axis1_pos, "obj_axis1_pos");
    rt_bind(0x004a80dcu, b_axis1_neg, "obj_axis1_neg");
    rt_bind(0x004a80f8u, b_axis2_pos, "obj_axis2_pos");
    rt_bind(0x004a8108u, b_axis2_neg, "obj_axis2_neg");
    rt_bind(0x004a8124u, b_axis3_pos, "obj_axis3_pos");
    rt_bind(0x004a8138u, b_axis3_neg, "obj_axis3_neg");
    rt_bind(0x004a8158u, b_axis4_pos, "obj_axis4_pos");
    rt_bind(0x004a816cu, b_axis4_neg, "obj_axis4_neg");
    rt_bind(0x004a818cu, b_axis5_pos, "obj_axis5_pos");
    rt_bind(0x004a81a0u, b_axis5_neg, "obj_axis5_neg");
    rt_bind(0x004a81b8u, b_slot_294, "obj_slot_294");
    rt_bind(0x004a81e8u, b_cur_block, "obj_cur_block");
    rt_bind(0x004a822cu, b_cur_52c, "obj_cur_52c");
    rt_bind(0x004a8240u, b_cur_530, "obj_cur_530");
    rt_bind(0x004a825cu, b_cur_53b0, "obj_cur_53b0");
    rt_bind(0x004a8f40u, b_get_bdfb24, "get_bdfb24");
    rt_bind(0x004a8fe8u, b_get_focus, "widget_mgr_get_focus");
}

RT_BIND_AREA(engine_widget)
