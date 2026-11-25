/*
 * bind_combat_pts.c: the per arena PTS route node loaders (src/combat/cb_pts.c), called the way
 * tools/emu/diff_combat_pts.py called them: no register arguments; the context holds host
 * pointers to 0x004fe1b0 / 0x004fe1b4 / 0x004fe1e4 and to the shared load buffer *(0x00a3a5c8),
 * read fresh on each call.
 *
 * Emulator-only interface replaced: the port's callbacks take host strings (the port's own 300
 * byte path buffer, a name literal). The original's are game addresses (a stack buffer of the
 * loader), so each callback copies them into a frame and passes its game address:
 * file_build_path 0x00499408(out, name, kind) and file_load_resource 0x00498d18(buffer, path).
 */
#include "rt_env.h"
#include "combat/cb_pts.h"

#define PTS_PATH 300u
#define PTS_NAME 64u

static void pts_build(void* u, char* out, const char* name, int32_t kind)
{
    (void)u;
    uint32_t f = rt_frame_push(PTS_PATH + PTS_NAME);
    size_t nl = strlen(name);
    if (nl > PTS_NAME - 1)
        nl = PTS_NAME - 1;
    memcpy(rt_ptr(f + PTS_PATH), name, nl);
    uint32_t a[3] = {f, f + PTS_PATH, (uint32_t)kind};
    rt_call(0x00499408u, a, 3);
    memcpy(out, rt_ptr(f), PTS_PATH);
    out[PTS_PATH - 1] = 0;
    rt_frame_pop(PTS_PATH + PTS_NAME);
}

static void pts_load(void* u, uint8_t* buffer, const char* path)
{
    (void)u;
    uint32_t f = rt_frame_push(PTS_PATH);
    size_t pl = strlen(path);
    if (pl > PTS_PATH - 1)
        pl = PTS_PATH - 1;
    memcpy(rt_ptr(f), path, pl);
    uint32_t a[2] = {rt_addr(buffer), f};
    rt_call(0x00498d18u, a, 2);
    rt_frame_pop(PTS_PATH);
}

static const CbPtsEnv* env(void)
{
    static CbPtsEnv e;
    e.node_count = (int32_t*)(void*)rt_ptr(0x004fe1b0u);
    e.sections = rt_ptr(0x004fe1b4u);
    e.nodes = rt_ptr(0x004fe1e4u);
    e.load_buffer = rt_ptr(rt_r32(0x00a3a5c8u));
    e.build_path = pts_build;
    e.load_resource = pts_load;
    e.user = NULL;
    return &e;
}

static uint32_t b_clear(const uint32_t* a, int n) { (void)a; (void)n; cb_pts_used_clear(env()); return 0; }

#define LOAD(name, arena) \
    static uint32_t name(const uint32_t* a, int n) { (void)a; (void)n; cb_pts_load(arena, env()); return 0; }
#define SETUP(name, arena) \
    static uint32_t name(const uint32_t* a, int n) { (void)a; (void)n; cb_pts_sections_setup(arena, env()); return 0; }

LOAD(b_fwy, CB_ARENA_FWY)
LOAD(b_dish, CB_ARENA_DISH)
LOAD(b_ice, CB_ARENA_ICE)
LOAD(b_lava, CB_ARENA_LAVA)
LOAD(b_paris, CB_ARENA_PARIS)
LOAD(b_hkong, CB_ARENA_HKONG)
LOAD(b_roof, CB_ARENA_ROOF)
LOAD(b_den, CB_ARENA_DEN)
LOAD(b_swamp, CB_ARENA_SWAMP)
LOAD(b_cyb, CB_ARENA_CYB)
LOAD(b_sroof, CB_ARENA_SROOF)
SETUP(b_set_ice, CB_ARENA_ICE)
SETUP(b_set_hkong, CB_ARENA_HKONG)
SETUP(b_set_den, CB_ARENA_DEN)

void rt_bind_combat_pts(void)
{
    rt_bind(0x0043bd60u, b_clear, "startNodesClearUsed");
    rt_bind(0x0043be70u, b_fwy, "terrainLoadPtsFwy");
    rt_bind(0x0043bffcu, b_dish, "terrainLoadPtsDish");
    rt_bind(0x0043c0ecu, b_set_ice, "startSectionsIce");
    rt_bind(0x0043c1c4u, b_ice, "terrainLoadPtsIce");
    rt_bind(0x0043c2d0u, b_lava, "terrainLoadPtsLava");
    rt_bind(0x0043c470u, b_paris, "terrainLoadPtsParis");
    rt_bind(0x0043c568u, b_set_hkong, "startSectionsHkong");
    rt_bind(0x0043c634u, b_hkong, "terrainLoadPtsHkong");
    rt_bind(0x0043c734u, b_roof, "terrainLoadPtsRoof");
    rt_bind(0x0043c824u, b_set_den, "startSectionsDen");
    rt_bind(0x0043c8f8u, b_den, "terrainLoadPtsDen");
    rt_bind(0x0043c9dcu, b_swamp, "terrainLoadPtsSwamp");
    rt_bind(0x0043cb48u, b_cyb, "terrainLoadPtsCyb");
    rt_bind(0x0043ccc0u, b_sroof, "terrainLoadPtsSroof");
}

RT_BIND_AREA(combat_pts)
