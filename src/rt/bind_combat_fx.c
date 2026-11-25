/*
 * bind_combat_fx.c: the effect sprite variants and animation loaders (src/combat/cb_fx.c), the
 * effect drawing (src/combat/cb_fx_draw.c) and two font helpers (src/combat/cb_font.c), bound the
 * way tools/emu/diff_combat_fx.py, diff_combat_fxdraw.py and diff_combat_font.py call them.
 *
 * Runtime equivalents of emulator-only interfaces:
 *   - cb_fx_draw keeps its primitive record (0x7C bytes) as a host local and passes the host
 *     pointer to the renderer callbacks; the original's is a stack local. The adapter pushes one
 *     game frame per draw call and each callback copies the record into it, calls the original
 *     address with that game address, and copies it back (same game address for every callback of
 *     one draw, so a renderer that keeps it sees one record).
 */
#include "rt_env.h"
#include "combat/cb_fx.h"
#include "combat/cb_fx_draw.h"
#include "combat/cb_font.h"

static WMem m_mem;

static const WMem* mem(void)
{
    m_mem.n = 1;
    m_mem.r[0].addr = 0;
    m_mem.r[0].size = RT_SIZE;
    m_mem.r[0].host = rt_base;
    return &m_mem;
}

static uint32_t call4(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, int n)
{
    uint32_t a[4] = {a0, a1, a2, a3};
    return rt_call(fn, a, n);
}

/* ---- FxEnv / GroundEnv (as diff_weapons_fx.py / diff_combat_fx.py) ---- */

static void fx_locate(void* u, uint32_t pos, int32_t view)
{
    (void)u;
    call4(0x004b8010u, pos, (uint32_t)view, 0, 0, 2);
}

static void fx_play(void* u, int32_t id, int32_t vol, int32_t pan, int32_t prio)
{
    (void)u;
    call4(0x0046f8b0u, (uint32_t)id, (uint32_t)vol, (uint32_t)pan, (uint32_t)prio, 4);
}

static FxEnv fx_env(void)
{
    FxEnv e;
    e.mem = mem();
    e.sprites = rt_ptr(0x00500304u);
    e.sprites_addr = 0x00500304u;
    e.particles = rt_ptr(0x00500034u);
    e.boom_cooldown = (int32_t*)(void*)rt_ptr(0x00500030u);
    e.split = (int32_t)rt_r32(0x00520e90u);
    e.snd_volume = (const int32_t*)(const void*)rt_ptr(0x00c8500cu);
    e.snd_pan = (const int32_t*)(const void*)rt_ptr(0x00c85058u);
    e.frames = 0x00501f26u;
    e.sound_locate = fx_locate;
    e.sound_play = fx_play;
    e.user = NULL;
    return e;
}

/* ---- the sprite variants: eax = position (game address), dl = flag ---- */

#define FX_VARIANT(name, port)                                                          \
    static uint32_t name(const uint32_t* a, int n)                                      \
    {                                                                                   \
        FxEnv e = fx_env();                                                             \
        return port(&e, rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1));                     \
    }
FX_VARIANT(b_spark_blue, cb_fx_spark_blue)
FX_VARIANT(b_blast12, cb_fx_blast12)
FX_VARIANT(b_blast24, cb_fx_blast24)
FX_VARIANT(b_flame5, cb_fx_flame5)
FX_VARIANT(b_flame10, cb_fx_flame10)
FX_VARIANT(b_smoke_a2, cb_fx_smoke_a2)
FX_VARIANT(b_smoke_a8, cb_fx_smoke_a8)
FX_VARIANT(b_smoke_b2, cb_fx_smoke_b2)
FX_VARIANT(b_smoke_b3, cb_fx_smoke_b3)
FX_VARIANT(b_smoke_b5, cb_fx_smoke_b5)
FX_VARIANT(b_smoke_c1, cb_fx_smoke_c1)
FX_VARIANT(b_smoke_c2, cb_fx_smoke_c2)
FX_VARIANT(b_smoke_c3, cb_fx_smoke_c3)
FX_VARIANT(b_lightning, cb_fx_lightning)
FX_VARIANT(b_explosion_quiet8, cb_fx_explosion_quiet8)

/* 0x00450e64 flame pair: the GroundEnv (jitter point 0x004fef70, fires 0x004ff308) */
static uint32_t b_flame_pair(const uint32_t* a, int n)
{
    FxEnv fx = fx_env();
    GroundEnv g = {&fx, rt_ptr(0x004ff308u), (int32_t*)(void*)rt_ptr(0x004fef70u), 0x004fef70u};
    return cb_fx_flame_pair(&g, rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1));
}

/* ---- animation loaders: eax = the sprite rect record ---- */

static int32_t anim_parse(void* u, uint32_t src, uint32_t dst, int32_t k)
{
    (void)u;
    return (int32_t)call4(0x00496d04u, src, dst, (uint32_t)k, 0, 3);
}

static int32_t anim_contrail(void* u, uint32_t table)
{
    (void)u;
    return (int32_t)call4(0x00412638u, table, 0, 0, 0, 1);
}

static uint32_t anim(int which, const uint32_t* a, int n)
{
    CbAnimEnv e = {0x00501f26u, anim_parse, anim_contrail, NULL};
    return (uint32_t)cb_fx_anim_store(&e, which, rt_arg(a, n, 0));
}

#define FX_ANIM(name, which) \
    static uint32_t name(const uint32_t* a, int n) { return anim(which, a, n); }
FX_ANIM(b_anim_explosion, CB_ANIM_EXPLOSION)
FX_ANIM(b_anim_smoke_burst, CB_ANIM_SMOKE_BURST)
FX_ANIM(b_anim_freeze, CB_ANIM_FREEZE)
FX_ANIM(b_anim_blast, CB_ANIM_BLAST)
FX_ANIM(b_anim_smoke_a, CB_ANIM_SMOKE_A)
FX_ANIM(b_anim_smoke_b, CB_ANIM_SMOKE_B)
FX_ANIM(b_anim_smoke_c, CB_ANIM_SMOKE_C)
FX_ANIM(b_anim_ricochet, CB_ANIM_RICOCHET)
FX_ANIM(b_anim_spark, CB_ANIM_SPARK)
FX_ANIM(b_anim_puff, CB_ANIM_PUFF)
FX_ANIM(b_anim_contrail, CB_ANIM_CONTRAIL)
FX_ANIM(b_anim_flame, CB_ANIM_FLAME)
FX_ANIM(b_anim_lightning, CB_ANIM_LIGHTNING)

/* ---- drawing: the primitive record lives in a game frame while a callback runs ---- */

#define PRIM_N (CB_PRIM_SIZE + 8)
static uint32_t prim_addr;   /* the game frame of the current draw */

static void prim_call(uint32_t fn, uint8_t* prim, int slot, uint32_t* a, int n)
{
    memcpy(rt_ptr(prim_addr), prim, PRIM_N);
    a[slot] = prim_addr;
    rt_call(fn, a, n);
    memcpy(prim, rt_ptr(prim_addr), PRIM_N);
}

static void d_init(void* u, uint8_t* p, int32_t k)
{
    uint32_t a[2] = {0, (uint32_t)k};
    (void)u;
    prim_call(0x0048e468u, p, 0, a, 2);
}

static void d_vec(void* u, uint8_t* p, int32_t w, int32_t x, int32_t y, int32_t z)
{
    uint32_t a[5] = {0, (uint32_t)w, (uint32_t)x, (uint32_t)y, (uint32_t)z};
    (void)u;
    prim_call(0x0048e810u, p, 0, a, 5);
}

static void d_700(void* u, uint8_t* p, int32_t v)
{
    uint32_t a[2] = {0, (uint32_t)v};
    (void)u;
    prim_call(0x0048e700u, p, 0, a, 2);
}

static void d_6d4(void* u, uint8_t* p, int32_t x, int32_t y)
{
    uint32_t a[3] = {0, (uint32_t)x, (uint32_t)y};
    (void)u;
    prim_call(0x0048e6d4u, p, 0, a, 3);
}

static void d_color(void* u, uint8_t* p, int32_t w, int32_t r, int32_t g, int32_t b)
{
    uint32_t a[5] = {0, (uint32_t)w, (uint32_t)r, (uint32_t)g, (uint32_t)b};
    (void)u;
    prim_call(0x0048e7b8u, p, 0, a, 5);
}

static void d_uv(void* u, uint8_t* p, int32_t u0, int32_t v0, int32_t u1, int32_t v1)
{
    uint32_t a[5] = {0, (uint32_t)u0, (uint32_t)v0, (uint32_t)u1, (uint32_t)v1};
    (void)u;
    prim_call(0x0048e8a4u, p, 0, a, 5);
}

static void d_page(void* u, uint8_t* p, int32_t pg)
{
    uint32_t a[2] = {0, (uint32_t)pg};
    (void)u;
    prim_call(0x0048e798u, p, 0, a, 2);
}

static void d_draw(void* u, uint32_t view, uint8_t* p)
{
    uint32_t a[2] = {view, 0};
    (void)u;
    prim_call(0x00491698u, p, 1, a, 2);
}

static void d_6c0(void* u, uint8_t* p, int32_t v)
{
    uint32_t a[2] = {0, (uint32_t)v};
    (void)u;
    prim_call(0x0048e6c0u, p, 0, a, 2);
}

static void d_particles(void* u, uint32_t view, int32_t mode)
{
    (void)u;
    call4(0x0045186cu, view, (uint32_t)mode, 0, 0, 2);
}

static CbFxDrawEnv draw_env(void)
{
    CbFxDrawEnv e = {mem(), d_init, d_vec, d_700, d_6d4, d_color, d_uv, d_page, d_draw, NULL,
                     d_6c0, d_particles};
    return e;
}

/* 0x0045186c particles draw (eax = view) */
static uint32_t b_particles_draw(const uint32_t* a, int n)
{
    CbFxDrawEnv e = draw_env();
    uint32_t saved = prim_addr;
    prim_addr = rt_frame_push(PRIM_N);
    cb_fx_particles_draw(&e, rt_arg(a, n, 0));
    rt_frame_pop(PRIM_N);
    prim_addr = saved;
    return 0;
}

/* 0x0045052c sprites draw (eax = view, edx = mode) */
static uint32_t b_sprites_draw(const uint32_t* a, int n)
{
    CbFxDrawEnv e = draw_env();
    uint32_t saved = prim_addr;
    prim_addr = rt_frame_push(PRIM_N);
    cb_fx_sprites_draw(&e, rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1));
    rt_frame_pop(PRIM_N);
    prim_addr = saved;
    return 0;
}

/* ---- font (diff_combat_font.py): font data 0x005027f8[font], spacing 0x005025d8[font], cursor
 * 0x005024d0 / 0x005024d4, texture base u8 0x005024d8 + 8 * font (u) and + 4 (v), sprite record
 * 0x00502518 + 0x18 * font, sprite draw 0x004967f4(eax = record) ---- */

static TM2Font font_of(uint32_t font)
{
    TM2Font f = {rt_ptr(rt_r32(0x005027f8u + 4 * font)), (int32_t)rt_r32(0x005025d8u + 4 * font)};
    return f;
}

static void font_draw(void* u, uint8_t* sprite)
{
    (void)u;
    call4(0x004967f4u, rt_addr(sprite), 0, 0, 0, 1);
}

/* 0x00451bf0 char width (eax = font, dl = c) */
static uint32_t b_char_width(const uint32_t* a, int n)
{
    TM2Font f = font_of(rt_arg(a, n, 0));
    return (uint32_t)cb_font_char_width(&f, (uint8_t)rt_arg(a, n, 1));
}

/* 0x00451f28 print centered (eax = font, edx = x, ebx = y, ecx = string) */
static uint32_t b_print_centered(const uint32_t* a, int n)
{
    uint32_t font = rt_arg(a, n, 0);
    TM2Font f = font_of(font);
    TM2FontPrint p;
    p.x = (int32_t*)(void*)rt_ptr(0x005024d0u);
    p.y = (int32_t*)(void*)rt_ptr(0x005024d4u);
    p.base_u = rt_r8(0x005024d8u + 8 * font);
    p.base_v = rt_r8(0x005024dcu + 8 * font);
    p.sprite = rt_ptr(0x00502518u + 0x18 * font);
    p.draw = font_draw;
    p.user = NULL;
    return (uint32_t)cb_font_print_centered(&f, &p, (int32_t)rt_arg(a, n, 1), (int32_t)rt_arg(a, n, 2),
                                            rt_ptr(rt_arg(a, n, 3)));
}

void rt_bind_combat_fx(void)
{
    rt_bind(0x0045075cu, b_spark_blue, "fx_spark_blue");
    rt_bind(0x00450b94u, b_blast12, "fx_blast12");
    rt_bind(0x00450be0u, b_blast24, "fx_blast24");
    rt_bind(0x00450de4u, b_flame5, "fx_flame5");
    rt_bind(0x00450e24u, b_flame10, "fx_flame10");
    rt_bind(0x00450e64u, b_flame_pair, "fx_flame_pair");
    rt_bind(0x00451014u, b_smoke_a2, "fx_smoke_a2");
    rt_bind(0x004510e0u, b_smoke_a8, "fx_smoke_a8");
    rt_bind(0x00451164u, b_smoke_b2, "fx_smoke_b2");
    rt_bind(0x004511a8u, b_smoke_b3, "fx_smoke_b3");
    rt_bind(0x004511ecu, b_smoke_b5, "fx_smoke_b5");
    rt_bind(0x00451230u, b_smoke_c1, "fx_smoke_c1");
    rt_bind(0x00451274u, b_smoke_c2, "fx_smoke_c2");
    rt_bind(0x004512b4u, b_smoke_c3, "fx_smoke_c3");
    rt_bind(0x0045133cu, b_lightning, "fx_lightning");
    rt_bind(0x00451430u, b_explosion_quiet8, "fx_explosion_quiet8");
    rt_bind(0x00451498u, b_anim_flame, "explodeStoreFlameAnimation");
    rt_bind(0x004514acu, b_anim_explosion, "explodeStoreExplosionAnimation");
    rt_bind(0x004514c0u, b_anim_smoke_burst, "explodeStoreSmokeBurstAnimation");
    rt_bind(0x004514d4u, b_anim_freeze, "explodeStoreFreezeAnimation");
    rt_bind(0x004514e8u, b_anim_blast, "explodeStoreBlastAnimation");
    rt_bind(0x004514fcu, b_anim_smoke_a, "explodeStoreSmokeAAnimation");
    rt_bind(0x00451510u, b_anim_smoke_b, "explodeStoreSmokeBAnimation");
    rt_bind(0x00451524u, b_anim_smoke_c, "explodeStoreSmokeCAnimation");
    rt_bind(0x00451538u, b_anim_ricochet, "explodeStoreRicochetAnimation");
    rt_bind(0x0045154cu, b_anim_spark, "explodeStoreSparkAnimation");
    rt_bind(0x00451560u, b_anim_puff, "explodeStorePuffAnimation");
    rt_bind(0x00451574u, b_anim_contrail, "explodeStoreContrailAnimation");
    rt_bind(0x00451594u, b_anim_lightning, "explodeStoreLightningAnimation");
    rt_bind(0x0045052cu, b_sprites_draw, "fxSpritesDraw");
    rt_bind(0x0045186cu, b_particles_draw, "fxImpactParticlesDraw");
    rt_bind(0x00451bf0u, b_char_width, "fontCharWidth");
    rt_bind(0x00451f28u, b_print_centered, "fontPrintCenteredXY");
}

RT_BIND_AREA(combat_fx)
