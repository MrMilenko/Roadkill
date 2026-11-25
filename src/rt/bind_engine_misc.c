/*
 * bind_engine_misc.c: the small engine leaves of src/engine/en_misc.c and en_misc2.c, called as
 * tools/emu/diff_engine_misc.py / diff_engine_misc2.py called them.
 *
 * en_misc: the record setters take the record's host bytes (EAX = its game address); the
 * renderer configuration globals 0x004e829c..0x004e82cf are EnRenderCfg in place (its layout
 * is the globals'); the render mode is the i32 at 0x004e8434.
 * en_misc2: EnMisc2Env on rt_mem_cb, free_fn calls 0x004aa39c with EAX = the pointer.
 */
#include "rt_env.h"
#include "engine/en_misc.h"
#include "engine/en_misc2.h"

#define A(i) rt_arg(a, n, (i))
#define I(i) ((int32_t)rt_arg(a, n, (i)))
#define REC rt_ptr(A(0))
#define CFG ((EnRenderCfg*)rt_ptr(0x004e829cu))
#define MODE ((int32_t*)rt_ptr(0x004e8434u))

/* ---- polygon record setters: EAX record, EDX, EBX, ECX, stack word 0 ---- */
static uint32_t b_prim_init(const uint32_t* a, int n) { en_prim_init(REC, I(1)); return 0; }
static uint32_t b_prim_flag3c(const uint32_t* a, int n) { en_prim_set_flag3c(REC, I(1)); return 0; }
static uint32_t b_prim_nop(const uint32_t* a, int n) { en_prim_nop(REC); return 0; }
static uint32_t b_prim_38(const uint32_t* a, int n) { en_prim_set_38(REC, I(1), I(2)); return 0; }
static uint32_t b_prim_mode36(const uint32_t* a, int n) { en_prim_set_mode36(REC, I(1)); return 0; }
static uint32_t b_prim_mode36b(const uint32_t* a, int n) { en_prim_set_mode36b(REC, I(1)); return 0; }
static uint32_t b_prim_word(const uint32_t* a, int n) { en_prim_set_word(REC, I(1)); return 0; }
static uint32_t b_prim_rgb(const uint32_t* a, int n) { en_prim_set_rgb(REC, I(1), I(2), I(3), I(4)); return 0; }
static uint32_t b_prim_rgb24(const uint32_t* a, int n) { en_prim_set_rgb24(REC, I(1), I(2)); return 0; }
static uint32_t b_prim_vec(const uint32_t* a, int n) { en_prim_set_vec(REC, I(1), I(2), I(3), I(4)); return 0; }
/* 0x0048e830 / 0x0048e854: EBX = the source's game address */
static uint32_t b_prim_vecp(const uint32_t* a, int n)
{
    int32_t v[3];
    memcpy(v, rt_ptr(A(2)), sizeof v);
    en_prim_set_vecp(REC, I(1), v);
    return 0;
}
static uint32_t b_prim_s16x3(const uint32_t* a, int n) { en_prim_set_s16x3(REC, I(1), rt_ptr(A(2))); return 0; }
static uint32_t b_prim_pair(const uint32_t* a, int n) { en_prim_set_pair(REC, I(1), I(2), I(3)); return 0; }
static uint32_t b_prim_uv_rect(const uint32_t* a, int n) { en_prim_set_uv_rect(REC, I(1), I(2), I(3), I(4)); return 0; }

/* ---- renderer configuration ---- */
static uint32_t b_cfg_29c(const uint32_t* a, int n) { en_cfg_store(CFG, 0x00, A(0)); return 0; }     /* 0x0048e94c */
static uint32_t b_cfg_flag_b(const uint32_t* a, int n) { en_cfg_store(CFG, 0x1c, A(0)); return 0; }  /* 0x0048e918 */
/* 0x0048e920: the stack argument (ret 4), no register arguments: a[0] */
static uint32_t b_cfg_flag_c(const uint32_t* a, int n) { en_cfg_store(CFG, 0x20, A(0)); return 0; }
static uint32_t b_cfg_tex_shift(const uint32_t* a, int n) { en_cfg_set_tex_shift(CFG, I(0)); return 0; }
static uint32_t b_cfg_set_depth(const uint32_t* a, int n) { en_cfg_set_depth_scale(CFG, I(0)); return 0; }
static uint32_t b_cfg_get_depth(const uint32_t* a, int n) { (void)a; (void)n; return (uint32_t)en_cfg_get_depth_scale(CFG); }
static uint32_t b_mode_get(const uint32_t* a, int n) { (void)a; (void)n; return (uint32_t)en_render_mode_get(MODE); }
static uint32_t b_mode_set(const uint32_t* a, int n) { en_render_mode_set(MODE, I(0)); return 0; }

/* ---- text record ---- */
/* 0x004ac01c (EAX record, EDX string). The original's EAX afterwards (record with its low byte
 * 0) is not used by any caller; 0 here. */
static uint32_t b_named_str5c(const uint32_t* a, int n) { en_named_set_str(REC, 0x5c, (const char*)rt_ptr(A(1))); return 0; }
static uint32_t b_named_1dc(const uint32_t* a, int n) { en_named_set32(REC, 0x1dc, A(1)); return 0; }   /* 0x004ac0a4 */
static uint32_t b_magic(const uint32_t* a, int n) { (void)a; (void)n; return en_magic(); }              /* 0x004ac9a8 */
static uint32_t b_check_f170(const uint32_t* a, int n) { return (uint32_t)en_check_f170(I(0), I(1)); }  /* 0x004ac9b0 */

/* ---- en_misc2 ---- */
static void m2_free(void* user, uint32_t p) { (void)user; rt_call(0x004aa39cu, &p, 1); }
static EnMisc2Env M2 = {rt_mem_cb, NULL, m2_free};

static uint32_t b_tab_block(const uint32_t* a, int n) { return en_tab_entry_block(&M2, A(0), I(1)); }   /* 0x00498634 */
static uint32_t b_tab_key(const uint32_t* a, int n) { return en_tab_entry_key(&M2, A(0), I(1)); }       /* 0x0049864c */
static uint32_t b_tab_find(const uint32_t* a, int n) { return en_tab_find_114(&M2, A(0), A(1)); }       /* 0x00498660 */
static uint32_t b_tab_get118(const uint32_t* a, int n) { return en_tab_get(&M2, A(0), I(1), 0x118); }   /* 0x004986ac */
static uint32_t b_tab_set118(const uint32_t* a, int n) { return en_tab_set(&M2, A(0), I(1), 0x118, A(2)); }
static uint32_t b_tab_get11c(const uint32_t* a, int n) { return en_tab_get(&M2, A(0), I(1), 0x11c); }
static uint32_t b_tab_set11c(const uint32_t* a, int n) { return en_tab_set(&M2, A(0), I(1), 0x11c, A(2)); }
/* 0x00498820: the harness did not compare EAX (no caller uses it); 0 here */
static uint32_t b_tab_remove(const uint32_t* a, int n) { en_tab_remove(&M2, A(0), A(1)); return 0; }
static uint32_t b_list_find(const uint32_t* a, int n) { return en_list_find(&M2, A(0), A(1)); }         /* 0x00497c50 */
static uint32_t b_word_at(const uint32_t* a, int n) { return en_word_at(&M2, A(0)); }                   /* 0x00497c78 */
static uint32_t b_free_all(const uint32_t* a, int n) { return en_list_free_all(&M2, A(0)); }            /* 0x00497a28 */
static uint32_t b_pitch(const uint32_t* a, int n) { (void)a; (void)n; return en_word_at(&M2, 0x00a04670u); } /* 0x004958f0 */
static uint32_t b_w30(const uint32_t* a, int n) { return en_word_at(&M2, A(0) + 0x30); }                /* 0x004988e4 */
static uint32_t b_w24(const uint32_t* a, int n) { return en_word_at(&M2, A(0) + 0x24); }                /* 0x00498968 */

void rt_bind_engine_misc(void)
{
    rt_bind(0x0048e468u, b_prim_init, "prim_init");
    rt_bind(0x0048e6c0u, b_prim_flag3c, "prim_set_flag3c");
    rt_bind(0x0048e6ccu, b_prim_nop, "prim_nop");
    rt_bind(0x0048e6d4u, b_prim_38, "prim_set_38");
    rt_bind(0x0048e700u, b_prim_mode36, "prim_set_mode36");
    rt_bind(0x0048e74cu, b_prim_mode36b, "prim_set_mode36b");
    rt_bind(0x0048e798u, b_prim_word, "prim_set_word");
    rt_bind(0x0048e7b8u, b_prim_rgb, "prim_set_rgb");
    rt_bind(0x0048e7ecu, b_prim_rgb24, "prim_set_rgb24");
    rt_bind(0x0048e810u, b_prim_vec, "prim_set_vec");
    rt_bind(0x0048e830u, b_prim_vecp, "prim_set_vecp");
    rt_bind(0x0048e854u, b_prim_s16x3, "prim_set_s16x3");
    rt_bind(0x0048e888u, b_prim_pair, "prim_set_pair");
    rt_bind(0x0048e8a4u, b_prim_uv_rect, "prim_set_uv_rect");
    rt_bind(0x0048e918u, b_cfg_flag_b, "subsys_render_flag_b");
    rt_bind(0x0048e920u, b_cfg_flag_c, "subsys_render_flag_c");
    rt_bind(0x0048e94cu, b_cfg_29c, "render_cfg_set_29c");
    rt_bind(0x0048e954u, b_cfg_tex_shift, "render_set_tex_shift");
    rt_bind(0x0048e97cu, b_cfg_set_depth, "render_set_depth_scale");
    rt_bind(0x0048e990u, b_cfg_get_depth, "render_get_depth_scale");
    rt_bind(0x0049ea60u, b_mode_get, "render_mode_get");
    rt_bind(0x0049ea68u, b_mode_set, "subsys_render_mode");
    rt_bind(0x004ac01cu, b_named_str5c, "named_set_str5c");
    rt_bind(0x004ac0a4u, b_named_1dc, "named_set_1dc");
    rt_bind(0x004ac9a8u, b_magic, "magic_dqmb");
    rt_bind(0x004ac9b0u, b_check_f170, "check_f170");

    rt_bind(0x004958f0u, b_pitch, "get_surface_pitch");
    rt_bind(0x00497a28u, b_free_all, "list_free_all");
    rt_bind(0x00497c50u, b_list_find, "list_find");
    rt_bind(0x00497c78u, b_word_at, "word_at");
    rt_bind(0x00498634u, b_tab_block, "table_entry_block");
    rt_bind(0x0049864cu, b_tab_key, "table_entry_key");
    rt_bind(0x00498660u, b_tab_find, "table_find_114");
    rt_bind(0x004986acu, b_tab_get118, "table_get_118");
    rt_bind(0x004986c8u, b_tab_set118, "table_set_118");
    rt_bind(0x004986dcu, b_tab_get11c, "table_get_11c");
    rt_bind(0x004986f8u, b_tab_set11c, "table_set_11c");
    rt_bind(0x00498820u, b_tab_remove, "table_remove");
    rt_bind(0x004988e4u, b_w30, "obj_word_30");
    rt_bind(0x00498968u, b_w24, "obj_word_24");
}

RT_BIND_AREA(engine_misc)
