/*
 * bind_physics.c: the terrain ports of src/physics (RMP tiles, probes, GRP zones), called the
 * way tools/emu/diff_terrain.py / diff_terrain_probe.py / diff_grp.py called the originals. The
 * blocks (probe state, body, tile) are game memory; the RmpWorld points at the original's
 * globals: the tile table 0x004fb17c, its count 0x004fb174, the GRP file at 0x004fca18 with the
 * zone tile ranges at 0x004fe162 / 0x004fe185, the event type 0x00624ec7.
 *
 * tm2math host state: the tables (atan2) loaded once from TM2_DATA (default disc/files), the
 * frame ticks copied from 0x004e8300 before each probe call.
 *
 * Flags the originals take as a byte (the probe's on_terrain argument) are passed as that byte.
 */
#include "rt_env.h"

#include <stdio.h>
#include <stdlib.h>

#include "math/tm2math.h"

#include "physics/grp.h"
#include "physics/terrain_probe.h"
#include "physics/terrain_tile.h"

#define A(i) rt_arg(a, n, (i))

#define G_TILES 0x004fb17cu
#define G_GRP 0x004fca18u

static void host(void)
{
    static int loaded;
    if (!loaded) {
        const char* dir = getenv("TM2_DATA");
        char s[1024], x[1024];
        if (!dir || !*dir) dir = "disc/files";
        snprintf(s, sizeof s, "%s/SINTAB.DAT", dir);
        snprintf(x, sizeof x, "%s/TM2.EXE", dir);
        if (tm2_math_load_tables(s, x) != 0) rt_stop("math tables (set TM2_DATA to the game's files)", 0);
        loaded = 1;
    }
    tm2_set_frame_ticks60((int16_t)rt_r16(0x004e8300u));
}

static RmpWorld world(void)
{
    host();
    RmpWorld w = {rt_ptr(G_TILES), (int32_t)rt_r32(0x004fb174u), rt_ptr(0x004fe162u), rt_ptr(0x004fe185u), rt_r8(0x00624ec7u)};
    return w;
}

/* 0x0042bdd4(tile, &{u, v}) */
static uint32_t b_rmp_height_rect(const uint32_t* a, int n)
{
    return (uint32_t)rmp_height_rect(rt_ptr(A(0)), (int32_t)rt_r32(A(1)), (int32_t)rt_r32(A(1) + 4));
}
/* 0x0042c098(tile, r, angle) */
static uint32_t b_rmp_height_ring(const uint32_t* a, int n) { return (uint32_t)rmp_height_ring(rt_ptr(A(0)), (int32_t)A(1), (int32_t)A(2)); }
/* 0x0042b9a8(body, state, tile index): the tile is entry k of the table */
static uint32_t b_rmp_tile_test(const uint32_t* a, int n)
{
    host();
    int32_t k = (int32_t)A(2);
    return (uint32_t)rmp_tile_test(rt_ptr(A(0)), rt_ptr(A(1)), rt_ptr(G_TILES + (uint32_t)(k * RMP_TILE_SIZE)), k);
}
/* 0x0045c2b4(zone) */
static uint32_t b_rmp_extra_tiles(const uint32_t* a, int n) { return (uint32_t)rmp_extra_tiles(rt_r8(0x00624ec7u), (int32_t)A(0)); }
/* 0x0042b5b0(body, state, tile) */
static uint32_t b_rmp_probe_slope(const uint32_t* a, int n)
{
    RmpWorld w = world();
    rmp_probe_slope(&w, rt_ptr(A(0)), rt_ptr(A(1)), (int32_t)A(2));
    return 0;
}
/* 0x0042b1f0(body, state): al */
static uint32_t b_rmp_probe_update(const uint32_t* a, int n)
{
    RmpWorld w = world();
    return (uint32_t)rmp_probe_update(&w, rt_ptr(A(0)), rt_ptr(A(1)));
}
/* 0x0042c17c(state, on terrain in dl) */
static uint32_t b_rmp_probe_integrate(const uint32_t* a, int n) { host(); rmp_probe_integrate(rt_ptr(A(0)), (uint8_t)A(1)); return 0; }
/* 0x0042b8a4(speed, span, state, on terrain in cl) */
static uint32_t b_rmp_probe_finish(const uint32_t* a, int n)
{
    host();
    rmp_probe_finish((int32_t)A(0), (int32_t)A(1), rt_ptr(A(2)), (uint8_t)A(3));
    return 0;
}
/* 0x0042cba8(x, y, z) on the loaded GRP */
static uint32_t b_grp_find_zone(const uint32_t* a, int n) { return (uint32_t)grp_find_zone(rt_ptr(G_GRP), (int32_t)A(0), (int32_t)A(1), (int32_t)A(2)); }
/* 0x0042c2d4(&{x, y, z}) */
static uint32_t b_grp_ground_height(const uint32_t* a, int n)
{
    RmpWorld w = world();
    int32_t p[3] = {(int32_t)rt_r32(A(0)), (int32_t)rt_r32(A(0) + 4), (int32_t)rt_r32(A(0) + 8)};
    return (uint32_t)grp_ground_height(&w, rt_ptr(G_GRP), p);
}

void rt_bind_physics(void)
{
    rt_bind(0x0042bdd4u, b_rmp_height_rect, "rmp_height_rect");
    rt_bind(0x0042c098u, b_rmp_height_ring, "rmp_height_ring");
    rt_bind(0x0042b9a8u, b_rmp_tile_test, "rmp_tile_test");
    rt_bind(0x0045c2b4u, b_rmp_extra_tiles, "rmp_extra_tiles");
    rt_bind(0x0042b5b0u, b_rmp_probe_slope, "rmp_probe_slope");
    rt_bind(0x0042b1f0u, b_rmp_probe_update, "rmp_probe_update");
    rt_bind(0x0042c17cu, b_rmp_probe_integrate, "rmp_probe_integrate");
    rt_bind(0x0042b8a4u, b_rmp_probe_finish, "rmp_probe_finish");
    rt_bind(0x0042cba8u, b_grp_find_zone, "grp_find_zone");
    rt_bind(0x0042c2d4u, b_grp_ground_height, "grp_ground_height");
}

RT_BIND_AREA(physics)
