/*
 * viewer.c: arena viewer, a dev tool on top of the verified loaders.
 *
 *   tm2view <level.DPC> [level.TPC]
 *
 * Loads and relocates the DPC (dpc.c), registers and decodes its TPC textures (tpc.c), and each
 * frame walks the node tree from the camera position (dpc_walk.c) the way the game does, so level
 * of detail and the volume switches follow the camera. Every mesh in the render list is drawn
 * with its decoded polygons (dpc_mesh.c).
 *
 * Port choices, not from the original: OpenGL drawing, perspective and camera, no backface
 * culling, color modulation of textures as PS1 (0x80 = 1.0).
 *
 * Controls: mouse look, WASD move, Q/E down/up, Shift faster, L toggles LOD, F wireframe,
 * T textures, B sprites and beams, Esc quits.
 *
 *   tm2view <level.DPC> --shot out.bmp [x y z yaw pitch]   renders one view to a BMP and exits
 *
 * With TM2_TERRAIN=<path without extension> (e.g. disc/files/TERRAIN/DEN) set, G toggles an
 * overlay of the verified ground height query (grp.c) sampled on a grid: the physics terrain
 * drawn over the graphics.
 */
#include "formats/dpc.h"
#include "formats/dpc_mesh.h"
#include "formats/tpc.h"
#include "scene/dpc_walk.h"
#include "physics/grp.h"
#include "physics/terrain_probe.h"
#include "physics/terrain_tile.h"
#include "scene/car_collide.h"
#include "scene/dpc_collide.h"
#include "game/car.h"
#include "assets/exe_carstats.h"
#include "game/camera.h"
#include "game/car_hit.h"
#include "game/car_input.h"
#include "game/car_spawn.h"
#include "game/car_update.h"
#include "game/spawn.h"
#include "core/tm2rand.h"
#include "math/tm2math.h"

#include <SDL.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AB 0x10000000u      /* relocation base for DPC pointers */
#define ROOT_SHIFT 3        /* the game's starting scale shift (0x004e8448) */

static uint8_t* load_file(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* b = n > 0 ? malloc((size_t)n) : NULL;
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *size = (size_t)n;
    return b;
}

static uint32_t rd32(const uint8_t* b, size_t o) { return b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24; }

/* --- level data --------------------------------------------------------------------------- */

static uint8_t* g_dpc;
static size_t g_dpc_size;
static uint32_t g_roots[64];
static int g_root_count;

static TpcTable g_tpc;
static GLuint g_tex[TPC_MAX_PAGES];
static int g_tex_w[TPC_MAX_PAGES], g_tex_h[TPC_MAX_PAGES];

static DpcWalk g_walk;

static void upload_pages(const uint8_t* t, size_t tsize, int first, int end);
static void draw_mesh(const DpcDrawItem* it, const DpcWalk* w);
static void xform(const int32_t m[9], double vx, double vy, double vz, const int32_t pos[3], double scale, double out[3]);
static const int32_t IDENT[12] = { 4096, 0, 0, 0, 4096, 0, 0, 0, 4096, 0, 0, 0 };

static int load_level(const char* dpc_path, const char* tpc_path) {
    if (!(g_dpc = load_file(dpc_path, &g_dpc_size))) { fprintf(stderr, "cannot read %s\n", dpc_path); return -1; }
    int r = dpc_relocate(g_dpc, g_dpc_size, AB, 0);
    {   /* room after the level for the collision walk's entities and hit records (same address space) */
        uint8_t* grown = realloc(g_dpc, g_dpc_size + 0x2000);
        if (!grown) return -1;
        g_dpc = grown;
        memset(g_dpc + g_dpc_size, 0, 0x2000);
    }
    if (r < 0) { fprintf(stderr, "%s: relocation failed (%d)\n", dpc_path, r); return -1; }
    DpcHeader h;
    dpc_read_header(g_dpc, g_dpc_size, &h);
    size_t list = dpc_reloc(h.root_list, h.base, AB) - AB;
    uint32_t n = rd32(g_dpc, list);
    for (uint32_t i = 0; i < n && g_root_count < 64; i++) {
        uint32_t v = rd32(g_dpc, list + 4 + 4 * i);
        if (v) g_roots[g_root_count++] = v;
    }

    size_t tsize;
    uint8_t* t = load_file(tpc_path, &tsize);
    if (!t) { fprintf(stderr, "cannot read %s (drawing untextured)\n", tpc_path); return 0; }
    if (tpc_register(&g_tpc, t, tsize, 0, 0, 0) < 0) { fprintf(stderr, "%s: bad TPC\n", tpc_path); return 0; }
    upload_pages(t, tsize, 0, g_tpc.page_count);
    printf("%s: %d roots, %d texture pages\n", dpc_path, g_root_count, g_tpc.page_count);
    return 0;
}

static void upload_pages(const uint8_t* t, size_t tsize, int first, int end) {
    glGenTextures(end - first, g_tex + first);
    for (int i = first; i < end; i++) {
        const TpcPage* p = &g_tpc.pages[i];
        uint8_t* rgba = malloc((size_t)p->width * p->height * 4);
        if (!rgba || tpc_page_rgba(p, t, tsize, 0, rgba, NULL) != 0) { free(rgba); g_tex[i] = 0; continue; }
        g_tex_w[i] = (int)p->width;
        g_tex_h[i] = (int)p->height;
        glBindTexture(GL_TEXTURE_2D, g_tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)p->width, (GLsizei)p->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        free(rgba);
    }
}

/* camera in root shift units (what the walk's positions are in at the root) */
static double cam[3], yaw, pitch;

/* --- terrain overlay (physics ground height on a grid) ------------------------------------ */

static uint8_t g_grp[GRP_SIZE], g_rmp[RMP_TILE_COUNT * RMP_TILE_SIZE];
static RmpWorld g_rmp_world;
static float* g_ground;          /* x, y, z per sample */
static int g_ground_n, show_ground;
static double g_lo[3], g_hi[3];

static int load_terrain(const char* base) {
    char path[4096];
    size_t n;
    snprintf(path, sizeof path, "%s.GRP", base);
    uint8_t* g = load_file(path, &n);
    if (!g) { fprintf(stderr, "cannot read %s\n", path); return -1; }
    memcpy(g_grp, g, n < GRP_SIZE ? n : GRP_SIZE);
    free(g);
    snprintf(path, sizeof path, "%s.RMP", base);
    uint8_t* r = load_file(path, &n);
    if (!r) { fprintf(stderr, "cannot read %s\n", path); return -1; }
    memcpy(g_rmp, r, n < sizeof g_rmp ? n : sizeof g_rmp);
    free(r);
    g_rmp_world.tiles = g_rmp;
    g_rmp_world.tile_count = RMP_TILE_COUNT;
    g_rmp_world.zone_first = g_grp + 0x174a;
    g_rmp_world.zone_last = g_grp + 0x176d;
    g_rmp_world.event_type = 0;
    return 0;
}

static void sample_ground(double step) {
    int nx = (int)((g_hi[0] - g_lo[0]) / step) + 1, ny = (int)((g_hi[1] - g_lo[1]) / step) + 1;
    g_ground = malloc(sizeof(float) * 3 * (size_t)nx * ny);
    g_ground_n = 0;
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < ny; j++) {
            int32_t p[3] = { (int32_t)(g_lo[0] + i * step), (int32_t)(g_lo[1] + j * step), 3000 };
            int32_t h = grp_ground_height(&g_rmp_world, g_grp, p);
            float* q = g_ground + 3 * g_ground_n++;
            q[0] = (float)p[0]; q[1] = (float)p[1]; q[2] = (float)h;
        }
    printf("terrain overlay: %d samples, step %.0f\n", g_ground_n, step);
}

static void draw_ground(void) {
    glDisable(GL_TEXTURE_2D);
    glPointSize(3);
    glBegin(GL_POINTS);
    for (int i = 0; i < g_ground_n; i++) {
        const float* q = g_ground + 3 * i;
        double t = (q[2] - g_lo[2]) / (g_hi[2] - g_lo[2] + 1);
        glColor3d(1, 0.2 + 0.8 * t, 0.1);
        glVertex3d(q[0] - cam[0], q[1] - cam[1], q[2] - cam[2] + 2);
    }
    glEnd();
}

/* --- drive test: verified spawn, input and car update on the arena ----------------------- */
/* Integration experiment, not a verification: the car update's steps that are not ported yet
 * (collisions, weapons, surface effects, ...) are left out, and the car is drawn as a box. */

static int drive_on;
static uint8_t g_car[CAR_BLOCK_SIZE], g_car_entity[0x130], g_pts[140 * 24];
static SpawnList g_spawns;
static CarFx g_fx;
static CarSpawnEnv g_spawn_env;
static CarInputEnv g_input_env;
static CarUpdateOps g_ops;
static const Uint8* g_keys;
static int g_script_gas;        /* shot mode: hold gas */
static double drive_acc;

static uint8_t* spawn_entity(void* u, uint8_t* car) { (void)u; (void)car; return g_car_entity; }
static uint8_t key_input(void* u, int id, int slot) {
    (void)u; (void)slot;
    if (g_script_gas) return id == 0x24;
    if (!g_keys) return 0;
    switch (id) {
    case 0x24: return g_keys[SDL_SCANCODE_W] || g_keys[SDL_SCANCODE_UP];        /* gas */
    case 0x25: return g_keys[SDL_SCANCODE_S] || g_keys[SDL_SCANCODE_DOWN];      /* brake / reverse */
    case 0x22: return g_keys[SDL_SCANCODE_A] || g_keys[SDL_SCANCODE_LEFT];
    case 0x23: return g_keys[SDL_SCANCODE_D] || g_keys[SDL_SCANCODE_RIGHT];
    case 0x29: return g_keys[SDL_SCANCODE_LSHIFT];                               /* turbo */
    default: return 0;
    }
}
static void op_drive_input(void* u, uint8_t* car) { (void)u; car_drive_input(car, 1, &g_fx); }
static void op_wheels(void* u, uint8_t* car) { (void)u; car_wheels_place(car, 1, g_grp, &g_fx); }
static void op_terrain(void* u, uint8_t* car) {
    (void)u;
    static const uint8_t no_platforms[0x38 * 2];
    CarEnv env = { &g_rmp_world, no_platforms, 0, 0, 0 };
    car_ground_contact(car, 1, &env);
}
static void op_drive(void* u, uint8_t* car) { (void)u; car_drive(car, 1, g_grp + 0x125e, &g_fx); }

static void collide_init(void);

/* --- the car's own model: CARSDB\<code>.DPC / .TPC from the player's files ---------------------- */

#define CAR_AB 0x30000000u   /* relocation base of the car model (its own address space) */
static uint8_t* g_cm;
static size_t g_cm_size;
static uint32_t g_cm_roots[16];
static int g_cm_root_count;
static DpcWalk g_cm_walk;
static int lod_on;           /* defined with the other view toggles below */

/* two-letter file codes by car id 0x10..0xF0, as the jump table at 0x004748fc picks them
 * (0xD0 Sweet Tooth and 0xE0 Minion come after BB in the string list) */
static const char* car_code(int car_id) {
    static const char* codes[15] = { "HH", "OT", "WH", "MG", "PV", "TH", "SP", "RK", "IR", "AX", "FL", "HS", "ST", "MN", "BB" };
    int i = (car_id >> 4) - 1;
    return (car_id & 0xF) == 0 && i >= 0 && i < 15 ? codes[i] : NULL;
}

static int load_car_model(const char* files, int car_id) {
    const char* code = car_code(car_id);
    if (!code) return -1;
    char dpc[4096], tpc[4096];
    snprintf(dpc, sizeof dpc, "%s/CARSDB/%s.DPC", files, code);
    snprintf(tpc, sizeof tpc, "%s/CARSDB/%s.TPC", files, code);
    size_t tsize;
    uint8_t* t = load_file(tpc, &tsize);
    if (!t || !(g_cm = load_file(dpc, &g_cm_size))) { fprintf(stderr, "cannot read %s / %s\n", dpc, tpc); return -1; }
    /* as the game loads a car: textures appended after the arena's pages, the model's page
     * numbers shifted by the first appended page (dpc_relocate tex_shift) */
    int first = g_tpc.page_count;
    if (tpc_register(&g_tpc, t, tsize, 0, 1, 0) < 0) { fprintf(stderr, "%s: bad TPC\n", tpc); return -1; }
    upload_pages(t, tsize, first, g_tpc.page_count);
    if (dpc_relocate(g_cm, g_cm_size, CAR_AB, (int16_t)first) < 0) { fprintf(stderr, "%s: relocation failed\n", dpc); return -1; }
    DpcHeader h;
    dpc_read_header(g_cm, g_cm_size, &h);
    size_t list = dpc_reloc(h.root_list, h.base, CAR_AB) - CAR_AB;
    uint32_t n = rd32(g_cm, list);
    for (uint32_t i = 0; i < n && g_cm_root_count < 16; i++) {
        uint32_t v = rd32(g_cm, list + 4 + 4 * i);
        if (v) g_cm_roots[g_cm_root_count++] = v;
    }
    printf("car model %s: %d roots, texture pages %d..%d\n", code, g_cm_root_count, first, g_tpc.page_count - 1);
    return 0;
}

/* the chase camera's globals (0x004fef04 fixed offset, 0x004fef10 angles, 0x004fef18 previous
 * position, 0x004fef24 lock frames); the drive test has one player, no split screen, no AI to watch */
static int32_t g_cam_fixed[3], g_cam_prev[3];
static int16_t g_cam_fixed_ang[4], g_cam_lock;
static CamEnv g_cam_env = { NULL, 0, 0, &g_cam_lock, g_cam_prev, g_cam_fixed, g_cam_fixed_ang, NULL, NULL, NULL };

static int drive_init(const char* base, int car_id) {
    char path[4096];
    size_t n;
    snprintf(path, sizeof path, "%s.PTS", base);
    uint8_t* pts = load_file(path, &n);
    if (!pts) { fprintf(stderr, "cannot read %s\n", path); return -1; }
    memcpy(g_pts, pts, n < sizeof g_pts ? n : sizeof g_pts);
    free(pts);
    /* the game fills the spawn list during level setup (not ported yet): use the first nodes */
    int k = 0;
    for (int i = 0; i < 140 && k < SPAWN_MAX; i++) {
        int32_t x = (int32_t)rd32(g_pts, 24 * i), y = (int32_t)rd32(g_pts, 24 * i + 4);
        if (x || y) g_spawns.node[k++] = (int16_t)i;
    }
    /* sin / atan tables from the player's files (disc/files next to the TERRAIN folder) */
    char files[4096], sintab[4096], exe[4096];
    snprintf(files, sizeof files, "%s", base);
    for (int up = 0; up < 2; up++) { char* sl = strrchr(files, '/'); if (sl) *sl = 0; }
    snprintf(sintab, sizeof sintab, "%s/SINTAB.DAT", files);
    snprintf(exe, sizeof exe, "%s/TM2.EXE", files);
    if (tm2_math_load_tables(sintab, exe) != 0) { fprintf(stderr, "cannot load %s / %s\n", sintab, exe); return -1; }
    if (tm2_car_stats_load(exe) != 0) { fprintf(stderr, "cannot read the car stats from %s\n", exe); return -1; }
    if (load_car_model(files, car_id) != 0) fprintf(stderr, "no model for car 0x%02x: drawing its box\n", car_id);
    tm2_set_frame_ticks60(1);
    tm2_set_frame_fps(60);
    tm2_srand(1);
    memset(g_car, 0, sizeof g_car);
    g_car[0x90] = (uint8_t)car_id;
    g_car[0x138] = 1;                      /* stats block of a player car */
    g_spawn_env = (CarSpawnEnv){ g_pts, 140, &g_spawns, 1, 1, g_grp, g_grp + 0x125e, &g_fx, 0, 0, 0, 0, spawn_entity, 0 };
    car_spawn_player(g_car, &g_spawn_env);
    car_camera_init(g_car);                /* camera part of the player car setup 0x00437850 */
    g_car[0x106] = 2;                      /* chase view; C cycles 1 / 2 / 3 */
    {   /* start the camera on its goal behind the car (the original's first frames ease in from
         * wherever the previous view left it) */
        int32_t m[9], v[3] = { 0, -0x118, 0x70 }, o[3];
        int16_t ang[3] = { 0, 0, (int16_t)(g_car[0x6f8] | g_car[0x6f9] << 8) };
        tm2_build_matrix(ang, m);
        tm2_mat_vec_mul_row(m, v, o);
        for (int k = 0; k < 3; k++) {
            uint32_t c = rd32(g_car, 0x6e4 + 4 * k) + (uint32_t)o[k];
            for (int b = 0; b < 4; b++) g_car[0x14 + 4 * k + b] = (uint8_t)(c >> (8 * b));
        }
    }
    g_input_env = (CarInputEnv){ key_input, 0, 0, 0, 0, 0, 1, 0, 1, 0 };
    g_ops = (CarUpdateOps){ 0 };
    g_ops.drive_input = op_drive_input;
    g_ops.wheels_place = op_wheels;
    g_ops.terrain = op_terrain;
    g_ops.drive = op_drive;
    collide_init();
    drive_on = 1;
    printf("drive test: car 0x%02x at %d %d %d\n", car_id, (int)rd32(g_car, 0x6e4), (int)rd32(g_car, 0x6e8), (int)rd32(g_car, 0x6ec));
    return 0;
}

/* --- car against the level: collision points (0x0049cec0) and the hit response ------------- */

static DpcCollideCtx g_coll_ctx;
static CarCollideState g_coll;
static CarHitEnv g_hit_env;
static CarHitApplyEnv g_apply_env;
static CarRecoverEnv g_recover_env;
static int32_t g_pair_push[15 * 45];         /* car pair table 0x0050d0d8; one car, so no car hits */
static uint32_t g_ent_root, g_ent_car, g_recs, g_prev_mat, g_prev_pos;

static uint8_t* region(uint32_t a) { return g_dpc + (a - AB); }
static int32_t hit_entity_type(void* u, uint32_t e) { (void)u; return (int32_t)rd32(region(e), 0x158); }
static int32_t recover_ground(void* u, const int32_t p[3]) { (void)u; return grp_ground_height(&g_rmp_world, g_grp, p); }

/* the level collision tree is the node with id 1 (level setup 0x0044f20c kind 1, scene root +0x15C) */
static uint32_t find_collision_root(void) {
    uint32_t stack[4096];
    int sp = 0;
    for (int i = g_root_count - 1; i >= 0; i--) stack[sp++] = g_roots[i];
    while (sp > 0) {
        uint32_t v = stack[--sp];
        if (v < AB || v - AB + 0x40 > g_dpc_size) continue;
        const uint8_t* n = g_dpc + (v - AB);
        if (n[0] <= 13 && n[0] != 6 && n[0] != 7 && (n[2] | n[3] << 8) == 1) return v;
        uint32_t kids[64];
        int k = 0;
        switch (n[0]) {
        case 1: for (int i = 0; i < n[0x14] && k < 64; i++) kids[k++] = rd32(n, 0x18 + 4 * i); break;
        case 3: for (int i = 0; i < n[0xb] && k < 64; i++) kids[k++] = rd32(n, 0x20 + 4 * i); break;
        case 4: for (int i = 0; i < n[0x12] && k < 64; i++) kids[k++] = rd32(n, 0x14 + 4 * i); break;
        case 5: for (int i = 0; i < n[0x36] && k < 64; i++) kids[k++] = rd32(n, 0x38 + 4 * i); break;
        case 9: for (int i = 0; i < n[6] && k < 64; i++) kids[k++] = rd32(n, 0x10 + 4 * i); break;
        case 0xb: kids[k++] = rd32(n, 8); kids[k++] = rd32(n, 0xc); break;
        case 0xc: for (int i = 0; i < (n[6] | n[7] << 8) && k < 64; i++) kids[k++] = rd32(n, 8 + 4 * i); break;
        default: break;
        }
        for (int i = k - 1; i >= 0 && sp < 4096; i--) if (kids[i]) stack[sp++] = kids[i];
    }
    return 0;
}

static void collide_init(void) {
    uint32_t a = AB + (uint32_t)((g_dpc_size + 15) & ~(size_t)15);
    g_ent_root = a; g_ent_car = a + 0x180; g_recs = a + 0x300; g_prev_mat = a + 0x400; g_prev_pos = a + 0x440;
    uint8_t* sr = region(g_ent_root);
    uint32_t root = find_collision_root();
    sr[0x158] = 9;                                   /* scene root entity type (0x00492df4) */
    for (int i = 0; i < 4; i++) sr[0x15c + i] = (uint8_t)(root >> (8 * i));
    uint8_t* ce = region(g_ent_car);
    ce[8] = 1; ce[0xc] = 1;
    car_collision_points(g_car, 1, ce);
    g_coll_ctx = (DpcCollideCtx){ g_dpc, g_dpc_size + 0x2000, AB, 0, 0, 0 };
    g_coll = (CarCollideState){ 0 };
    g_coll.shift = 3;                                /* 0x004e8430 */
    g_coll.scene_root = g_ent_root;
    g_hit_env = (CarHitEnv){ 0 };
    g_hit_env.entity_type = hit_entity_type;
    g_apply_env = (CarHitApplyEnv){ 0 };
    g_recover_env = (CarRecoverEnv){ g_pair_push, recover_ground, NULL };
    printf("collision tree 0x%08x\n", root);
}

/* the all cars update's per car collision steps (0x00458470), for the one player car */
static void collide_step(void) {
    const uint8_t* body = g_car + 0x6c4;
    uint8_t* ce = region(g_ent_car);
    memcpy(ce + 0xb0, body + 0xb0, 36);              /* entity transform = body matrix and position */
    memcpy(ce + 0xe8, body + 0x20, 12);
    memcpy(region(g_prev_mat), body + 0xf8, 36);     /* last clear state kept by car_hit_step */
    memcpy(region(g_prev_pos), body + 0xe0, 12);
    car_collide(&g_coll_ctx, &g_coll, g_ent_car, g_recs, 1, g_prev_mat, g_prev_pos);
    memcpy(g_car + 0x234, region(g_recs), 6 * 0x20);
    car_hits_resolve(g_car, 1, 0, &g_hit_env);
    car_hits_gather(g_car, 1, 6, &g_apply_env);
    car_hit_step(g_car, 1, &g_apply_env);
    car_hit_recover(g_car, 1, &g_recover_env);
}

static void drive_step(void) {
    car_read_input(g_car, &g_input_env);
    car_player_update(g_car, &g_ops);
    car_chase_camera(g_car, &g_cam_env);
    collide_step();
    if (getenv("TM2_DRIVE_DEBUG")) {
        const uint8_t* b = g_car + 0x6c4;
        printf("speed %d vel %d %d %d pos %d %d %d m %d %d %d %d hdg %d 14c %d ctl gas %d dd %d de %d\n",
               (int16_t)(b[0xa] | b[0xb] << 8), (int16_t)(b[8] | b[9] << 8), (int16_t)(b[0xa] | b[0xb] << 8),
               (int16_t)(b[0xc] | b[0xd] << 8), (int)rd32(b, 0x20), (int)rd32(b, 0x24), (int)rd32(b, 0x28),
               (int)rd32(b, 0xb0), (int)rd32(b, 0xb4), (int)rd32(b, 0xbc), (int)rd32(b, 0xc0), (int)rd32(b, 0x34),
               (int16_t)(g_car[0x14c] | g_car[0x14d] << 8), g_car[0xe0], g_car[0xdd], g_car[0xde]);
    }
}

/* the viewer's camera from the ported chase camera: position +0x14, pitch +0x0C, yaw +0x10 (4096 a turn).
 * Game yaw is atan2(dx, dy), the viewer's convention; game pitch grows looking down, the viewer's
 * pitch is negative looking down. */
static void drive_camera(void) {
    for (int k = 0; k < 3; k++) cam[k] = (int32_t)rd32(g_car, 0x14 + 4 * k);
    yaw = (int16_t)(g_car[0x10] | g_car[0x11] << 8) * 2 * M_PI / 4096.0;
    pitch = -(int16_t)(g_car[0xc] | g_car[0xd] << 8) * 2 * M_PI / 4096.0;
}

/* Port choice until the game's object draw is ported: the model's root gets the car body matrix
 * (+0x774, world = M^T * local as car_move uses it) and the car position. xform computes
 * m * (v + pos), so with m = M^T the root position is M * (car - camera). */
static int walk_car_model(void) {
    if (!g_cm) return 0;
    int32_t m[9], mt[12] = { 0 };
    for (int k = 0; k < 9; k++) m[k] = (int32_t)rd32(g_car, 0x774 + 4 * k);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) mt[3 * r + c] = m[3 * c + r];
    double d[3];
    for (int k = 0; k < 3; k++) d[k] = (int32_t)rd32(g_car, 0x6e4 + 4 * k) - cam[k];
    int32_t pos[3];
    for (int r = 0; r < 3; r++) pos[r] = (int32_t)lround((m[3 * r] * d[0] + m[3 * r + 1] * d[1] + m[3 * r + 2] * d[2]) / 4096.0);
    DpcWalk* w = &g_cm_walk;
    w->buf = g_cm; w->size = g_cm_size; w->addr_base = CAR_AB;
    w->lod_disabled = !lod_on;
    w->frame_clear = 0; w->frame_set = 1;
    w->visible = NULL; w->anim = NULL;
    w->slot_index = 0; w->group_count = 0; w->item_count = 0;
    for (int i = 0; i < g_cm_root_count; i++) {
        if (i) w->slot_index++;
        memcpy(w->mat[1][w->slot_index], mt, sizeof mt);
        dpc_walk(w, g_cm_roots[i], 0, pos, mt, IDENT, IDENT, ROOT_SHIFT);
    }
    return 1;
}

/* TM2_CARCHECK: the drawn model's extent in the car's own axes (local = M * (world - car)), next
 * to the collision box from the stats, to check the model sits on the car the right way round */
static void car_check(void) {
    int32_t m[9];
    for (int k = 0; k < 9; k++) m[k] = (int32_t)rd32(g_car, 0x774 + 4 * k);
    double lo[3] = { 1e30, 1e30, 1e30 }, hi[3] = { -1e30, -1e30, -1e30 }, c[3];
    for (int k = 0; k < 3; k++) c[k] = (int32_t)rd32(g_car, 0x6e4 + 4 * k) - cam[k];
    for (int i = 0; i < g_cm_walk.item_count; i++) {
        const DpcDrawItem* it = &g_cm_walk.items[i];
        size_t node = it->node - CAR_AB, verts = rd32(g_cm, node + 4) - CAR_AB, o2 = rd32(g_cm, node + 0xc) - CAR_AB;
        uint32_t count = rd32(g_cm, node + 0x10);
        const int32_t* mm = g_cm_walk.mat[0][it->slot];
        double scale = ldexp(1.0, ROOT_SHIFT - it->shift);
        for (uint32_t pi = 0; pi < count; pi++) {
          DpcPoly p;
          if (dpc_poly_decode(g_cm, g_cm_size, o2, &p) != 0) break;
          o2 += (size_t)p.words * 4;
          for (int k = 0; k < (p.vcount < 3 ? 1 : p.vcount); k++) {
            size_t vo = verts + 8 * (size_t)(uint16_t)p.index[k];
            if (vo + 6 > g_cm_size) continue;
            double o[3], r[3];
            xform(mm, (int16_t)(g_cm[vo] | g_cm[vo + 1] << 8), (int16_t)(g_cm[vo + 2] | g_cm[vo + 3] << 8),
                  (int16_t)(g_cm[vo + 4] | g_cm[vo + 5] << 8), it->pos, scale, o);
            for (int q = 0; q < 3; q++) r[q] = o[q] - c[q];
            for (int q = 0; q < 3; q++) {
                double l = (m[3 * q] * r[0] + m[3 * q + 1] * r[1] + m[3 * q + 2] * r[2]) / 4096.0;
                if (l < lo[q]) lo[q] = l;
                if (l > hi[q]) hi[q] = l;
            }
          }
        }
    }
    const uint8_t* st = g_car + 0x138;
    printf("model x %.0f..%.0f  y %.0f..%.0f  z %.0f..%.0f   box: half width %d, front %d, rear %d, top %d\n",
           lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], (int16_t)(st[0x48] | st[0x49] << 8) / 2,
           (int16_t)(st[0x4e] | st[0x4f] << 8), (int16_t)(st[0x50] | st[0x51] << 8), 0x18 - (int16_t)(st[0x42] | st[0x43] << 8));
}

static void draw_car(void) {
    if (walk_car_model()) {
        static int checked;
        if (!checked && getenv("TM2_CARCHECK")) { checked = 1; car_check(); }
        for (int i = 0; i < g_cm_walk.item_count; i++) draw_mesh(&g_cm_walk.items[i], &g_cm_walk);
        return;
    }
    const uint8_t* s = g_car + 0x138;
    double w = (int16_t)(s[0x48] | s[0x49] << 8) / 2.0, front = (int16_t)(s[0x4e] | s[0x4f] << 8),
           rear = (int16_t)(s[0x50] | s[0x51] << 8), top = 0x18 - (int16_t)(s[0x42] | s[0x43] << 8);
    double c[8][3];
    for (int i = 0; i < 8; i++) {
        double l[3] = { (i & 1) ? w : -w, (i & 2) ? front : rear, (i & 4) ? top : 0 };
        int32_t m[9];
        for (int k = 0; k < 9; k++) m[k] = (int32_t)rd32(g_car, 0x774 + 4 * k);
        for (int r = 0; r < 3; r++)       /* world = M^T * local, as car_move */
            c[i][r] = (m[r] * l[0] + m[r + 3] * l[1] + m[r + 6] * l[2]) / 4096.0 + (int32_t)rd32(g_car, 0x6e4 + 4 * r) - cam[r];
    }
    static const int faces[6][4] = { { 0, 1, 3, 2 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 3, 7, 5 } };
    static const float col[6][3] = { { .2f, .2f, .2f }, { .9f, .1f, .6f }, { .6f, .1f, .4f }, { 1, .3f, .7f }, { .7f, .1f, .5f }, { .7f, .1f, .5f } };
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    for (int f = 0; f < 6; f++) {
        glColor3fv(col[f]);
        for (int v = 0; v < 4; v++) glVertex3dv(c[faces[f][v]]);
    }
    glEnd();
    glPointSize(7);
    glBegin(GL_POINTS);                    /* the four wheel probes */
    glColor3f(0.2f, 1, 0.3f);
    for (int k = 0; k < 4; k++) {
        const uint8_t* st = g_car + 0x58c + 0x18 + 0x48 * k;
        glVertex3d((int32_t)rd32(st, 0x3c) - cam[0], (int32_t)rd32(st, 0x40) - cam[1], (int32_t)rd32(st, 0x44) - cam[2]);
    }
    glEnd();
}

/* --- per frame walk and draw -------------------------------------------------------------- */


static int lod_on = 1, wire = 0, textures = 1, sprites = 1;
static long frame_polys;

static void walk_from_camera(void) {
    DpcWalk* w = &g_walk;
    w->buf = g_dpc; w->size = g_dpc_size; w->addr_base = AB;
    w->lod_disabled = !lod_on;
    w->frame_clear = 0; w->frame_set = 1;
    w->visible = NULL; w->anim = NULL;
    w->slot_index = 0; w->group_count = 0; w->item_count = 0;
    int32_t pos[3] = { (int32_t)-cam[0], (int32_t)-cam[1], (int32_t)-cam[2] };
    for (int i = 0; i < g_root_count; i++) {
        /* viewer choice: each root gets a fresh slot (the original reuses the last slot index) */
        if (i) w->slot_index++;
        memcpy(w->mat[1][w->slot_index], IDENT, sizeof IDENT);
        dpc_walk(w, g_roots[i], 0, pos, IDENT, IDENT, IDENT, ROOT_SHIFT);
    }
}

/* camera basis in world space, set by set_camera (sprites face the camera) */
static double cam_right[3], cam_up[3];

static void xform(const int32_t m[9], double vx, double vy, double vz, const int32_t pos[3], double scale, double out[3]) {
    double x = (vx + pos[0]) * scale, y = (vy + pos[1]) * scale, z = (vz + pos[2]) * scale;
    out[0] = (m[0] * x + m[1] * y + m[2] * z) / 4096.0;
    out[1] = (m[3] * x + m[4] * y + m[5] * z) / 4096.0;
    out[2] = (m[6] * x + m[7] * y + m[8] * z) / 4096.0;
}

static void vertex(const int32_t m[9], const int16_t* v, const int32_t pos[3], double scale) {
    double o[3];
    xform(m, v[0], v[1], v[2], pos, scale, o);
    glVertex3dv(o);
}

/* sprite (billboard_quad 0x00490780) and beam (line_segment_quad 0x004908b0) corners, in the
 * order the renderer assigns the four UV words */
static int sprite_corners(const DpcPoly* p, const int32_t m[9], const int16_t* v, const int32_t pos[3], double scale,
                          double out[4][3]) {
    double c[3], w = p->size_w * scale, h = p->size_h * scale;
    if (p->vcount == DPC_POLY_SPRITE) {
        /* screen aligned, half extents +0x06 / +0x08 (the renderer doubles them for the far
         * corner); camera space y points down, so corner 0 is top left */
        xform(m, v[0], v[1], v[2], pos, scale, c);
        static const double sx[4] = { -1, 1, 1, -1 }, sy[4] = { 1, 1, -1, -1 };
        for (int i = 0; i < 4; i++)
            for (int k = 0; k < 3; k++) out[i][k] = c[k] + sx[i] * w * cam_right[k] + sy[i] * h * cam_up[k];
        return 1;
    }
    /* beam: vertex 0 +/- +0x08 along model z, widened by +0x06 across the view */
    double a[3], b[3], d[3], o[3];
    xform(m, v[0], v[1], v[2] + p->size_h, pos, 1.0, a);
    xform(m, v[0], v[1], v[2] - p->size_h, pos, 1.0, b);
    for (int k = 0; k < 3; k++) { a[k] *= scale; b[k] *= scale; d[k] = b[k] - a[k]; }
    o[0] = a[1] * d[2] - a[2] * d[1]; o[1] = a[2] * d[0] - a[0] * d[2]; o[2] = a[0] * d[1] - a[1] * d[0];
    double l = sqrt(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
    if (l == 0) return 0;
    for (int k = 0; k < 3; k++) {
        o[k] *= w / l;
        out[0][k] = a[k] + o[k]; out[1][k] = a[k] - o[k]; out[2][k] = b[k] - o[k]; out[3][k] = b[k] + o[k];
    }
    return 1;
}

static void color(uint32_t c, int textured) {
    double k = textured ? 1.0 / 128.0 : 1.0 / 255.0;
    /* polygon colors are 0x00RRGGBB: FUN_0049653c puts bits 19..23 into the red field of 555/565 */
    double r = ((c >> 16) & 0xff) * k, g = ((c >> 8) & 0xff) * k, b = (c & 0xff) * k;
    glColor3d(r > 1 ? 1 : r, g > 1 ? 1 : g, b > 1 ? 1 : b);
}

static void draw_mesh(const DpcDrawItem* it, const DpcWalk* w) {
    const uint8_t* buf = w->buf;
    size_t size = w->size;
    uint32_t base = w->addr_base;
    size_t node = it->node - base;
    size_t verts = rd32(buf, node + 4) - base, polys = rd32(buf, node + 0xc) - base;
    uint32_t count = rd32(buf, node + 0x10);
    const int32_t* m = w->mat[0][it->slot];
    double scale = ldexp(1.0, ROOT_SHIFT - it->shift);
    size_t o = polys;
    for (uint32_t k = 0; k < count; k++) {
        DpcPoly p;
        if (dpc_poly_decode(buf, size, o, &p) != 0) return;
        o += (size_t)p.words * 4;
        int tex = textures && p.textured && p.page < g_tpc.page_count && g_tex[p.page];
        if (tex) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, g_tex[p.page]); }
        else glDisable(GL_TEXTURE_2D);
        if (p.vcount < 3) {
            if (!sprites) continue;
            size_t vo = verts + 8 * (size_t)(uint16_t)p.index[0];
            if (vo + 6 > size) continue;
            int16_t v[3] = { (int16_t)(buf[vo] | buf[vo + 1] << 8), (int16_t)(buf[vo + 2] | buf[vo + 3] << 8),
                             (int16_t)(buf[vo + 4] | buf[vo + 5] << 8) };
            double q[4][3];
            if (!sprite_corners(&p, m, v, it->pos, scale, q)) continue;
            color(p.color[0], p.textured);
            glBegin(GL_TRIANGLE_FAN);
            for (int i = 0; i < 4; i++) {
                if (tex) glTexCoord2d((p.u[i] + 0.5) / g_tex_w[p.page], (p.v[i] + 0.5) / g_tex_h[p.page]);
                glVertex3dv(q[i]);
            }
            glEnd();
            frame_polys++;
            continue;
        }
        glBegin(GL_TRIANGLE_FAN);
        for (int i = 0; i < p.vcount; i++) {
            size_t vo = verts + 8 * (size_t)(uint16_t)p.index[i];
            if (vo + 6 > size) break;
            int16_t v[3] = { (int16_t)(buf[vo] | buf[vo + 1] << 8), (int16_t)(buf[vo + 2] | buf[vo + 3] << 8),
                             (int16_t)(buf[vo + 4] | buf[vo + 5] << 8) };
            color(p.color[p.shading >= 2 && i < p.ncolors ? i : 0], p.textured);
            if (tex) glTexCoord2d((p.u[i] + 0.5) / g_tex_w[p.page], (p.v[i] + 0.5) / g_tex_h[p.page]);
            vertex(m, v, it->pos, scale);
        }
        glEnd();
        frame_polys++;
    }
}

static void set_camera(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    double nearz = 16, farz = 1 << 19, fy = nearz * tan(30.0 * M_PI / 180.0), fx = fy * width / (double)height;
    glFrustum(-fx, fx, -fy, fy, nearz, farz);
    /* game convention: +Y forward at heading 0, +Z up */
    double f[3] = { cos(pitch) * sin(yaw), cos(pitch) * cos(yaw), sin(pitch) };
    double r[3] = { f[1], -f[0], 0 };
    double rl = sqrt(r[0] * r[0] + r[1] * r[1]);
    r[0] /= rl; r[1] /= rl;
    double u[3] = { r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0] };
    for (int k = 0; k < 3; k++) { cam_right[k] = r[k]; cam_up[k] = u[k]; }
    GLdouble mv[16] = { r[0], u[0], -f[0], 0, r[1], u[1], -f[1], 0, r[2], u[2], -f[2], 0, 0, 0, 0, 1 };
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd(mv);     /* geometry is already camera relative (the walk subtracts cam) */
}

/* first view: walk from the origin without LOD and look at the middle of what is there */
static void place_camera(void) {
    int keep = lod_on;
    lod_on = 0;
    cam[0] = cam[1] = cam[2] = 0;
    walk_from_camera();
    lod_on = keep;
    double lo[3] = { 1e30, 1e30, 1e30 }, hi[3] = { -1e30, -1e30, -1e30 };
    for (int i = 0; i < g_walk.item_count; i++) {
        const DpcDrawItem* it = &g_walk.items[i];
        const int32_t* m = g_walk.mat[0][it->slot];
        double s = ldexp(1.0, ROOT_SHIFT - it->shift);
        double p[3] = { it->pos[0] * s, it->pos[1] * s, it->pos[2] * s };
        for (int k = 0; k < 3; k++) {
            double v = (m[3 * k] * p[0] + m[3 * k + 1] * p[1] + m[3 * k + 2] * p[2]) / 4096.0;
            if (v < lo[k]) lo[k] = v;
            if (v > hi[k]) hi[k] = v;
        }
    }
    if (g_walk.item_count == 0) return;
    for (int k = 0; k < 3; k++) { g_lo[k] = lo[k]; g_hi[k] = hi[k]; }
    for (int k = 0; k < 3; k++) cam[k] = (lo[k] + hi[k]) / 2;
    cam[2] = hi[2] + (hi[0] - lo[0]) * 0.1;
    pitch = -0.5;
    printf("level extent x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f\n", lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]);
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <level.DPC> [level.TPC] | --shot out.bmp [x y z yaw pitch]\n", argv[0]); return 2; }
    const char* shot = NULL;
    double shot_view[5];
    int shot_has_view = 0;
    if (argc > 3 && strcmp(argv[2], "--shot") == 0) {
        shot = argv[3];
        if (argc > 8) { for (int i = 0; i < 5; i++) shot_view[i] = atof(argv[4 + i]); shot_has_view = 1; }
    }
    char tpc_path[4096];
    if (argc > 2 && !shot) snprintf(tpc_path, sizeof tpc_path, "%s", argv[2]);
    else snprintf(tpc_path, sizeof tpc_path, "%.*s.TPC", (int)strlen(argv[1]) - 4, argv[1]);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window* win = SDL_CreateWindow("OpenTM2 viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 800,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                                       (shot ? SDL_WINDOW_HIDDEN : 0));
    if (!win) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    SDL_GLContext gl = SDL_GL_CreateContext(win);
    if (!gl) { fprintf(stderr, "GL: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);

    if (load_level(argv[1], tpc_path) != 0) return 1;
    place_camera();
    const char* terr = getenv("TM2_TERRAIN");
    if (terr && load_terrain(terr) == 0) {
        sample_ground(64);
        show_ground = !getenv("TM2_DRIVE");
        if (getenv("TM2_DRIVE")) {
            drive_init(terr, (int)strtol(getenv("TM2_DRIVE"), 0, 16));
            const char* fr = getenv("TM2_DRIVE_FRAMES");
            if (fr) {
                g_script_gas = 1;
                for (int i = 0; i < atoi(fr); i++) drive_step();
                g_script_gas = 0;
                printf("after %s frames: car at %d %d %d, speed %d\n", fr, (int)rd32(g_car, 0x6e4), (int)rd32(g_car, 0x6e8),
                       (int)rd32(g_car, 0x6ec), (int16_t)(g_car[0x6ce] | g_car[0x6cf] << 8));
                printf("camera %d %d %d (car space: %d across, %d along, %d up), pitch %d, yaw %d, car heading %d\n",
                       (int)rd32(g_car, 0x14), (int)rd32(g_car, 0x18), (int)rd32(g_car, 0x1c),
                       0, 0, 0, (int16_t)(g_car[0xc] | g_car[0xd] << 8), (int16_t)(g_car[0x10] | g_car[0x11] << 8),
                       (int16_t)(g_car[0x6f8] | g_car[0x6f9] << 8));
            }
            drive_camera();
        }
    }
    if (shot_has_view) {
        cam[0] = shot_view[0]; cam[1] = shot_view[1]; cam[2] = shot_view[2];
        yaw = shot_view[3]; pitch = shot_view[4];
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glClearColor(0.05f, 0.05f, 0.08f, 1);
    if (!shot) SDL_SetRelativeMouseMode(SDL_TRUE);

    Uint64 last = SDL_GetPerformanceCounter(), title_at = 0;
    int running = 1, frames = 0;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: running = 0; break;
                case SDLK_l: lod_on = !lod_on; break;
                case SDLK_f: wire = !wire; break;
                case SDLK_t: textures = !textures; break;
                case SDLK_b: sprites = !sprites; break;
                case SDLK_g: show_ground = !show_ground; break;
                case SDLK_c: if (drive_on) { g_car[0x106] = (uint8_t)(g_car[0x106] % 3 + 1); printf("view %d\n", g_car[0x106]); } break;
                default: break;
                }
            } else if (e.type == SDL_MOUSEMOTION && !drive_on) {
                yaw += e.motion.xrel * 0.004;
                pitch -= e.motion.yrel * 0.004;
                if (pitch > 1.5) pitch = 1.5;
                if (pitch < -1.5) pitch = -1.5;
            }
        }
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / (double)SDL_GetPerformanceFrequency();
        last = now;
        const Uint8* k = SDL_GetKeyboardState(NULL);
        if (drive_on) {
            g_keys = k;
            drive_acc += dt;
            for (int guard = 0; drive_acc >= 1.0 / 60 && guard < 8; guard++, drive_acc -= 1.0 / 60) drive_step();
            if (drive_acc > 1) drive_acc = 0;
            drive_camera();
        }
        double speed = (drive_on ? 0 : (k[SDL_SCANCODE_LSHIFT] ? 20000 : 4000)) * dt;
        double fwd[2] = { sin(yaw), cos(yaw) }, side[2] = { cos(yaw), -sin(yaw) };
        if (k[SDL_SCANCODE_W]) { cam[0] += fwd[0] * speed; cam[1] += fwd[1] * speed; }
        if (k[SDL_SCANCODE_S]) { cam[0] -= fwd[0] * speed; cam[1] -= fwd[1] * speed; }
        if (k[SDL_SCANCODE_D]) { cam[0] += side[0] * speed; cam[1] += side[1] * speed; }
        if (k[SDL_SCANCODE_A]) { cam[0] -= side[0] * speed; cam[1] -= side[1] * speed; }
        if (k[SDL_SCANCODE_E]) cam[2] += speed;
        if (k[SDL_SCANCODE_Q]) cam[2] -= speed;

        int dw, dh;
        SDL_GL_GetDrawableSize(win, &dw, &dh);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, wire ? GL_LINE : GL_FILL);
        set_camera(dw, dh);
        walk_from_camera();
        frame_polys = 0;
        for (int i = 0; i < g_walk.item_count; i++) draw_mesh(&g_walk.items[i], &g_walk);
        if (show_ground && g_ground_n) draw_ground();
        if (drive_on) draw_car();
        if (shot && ++frames >= 3) {
            uint8_t* px = malloc((size_t)dw * dh * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, dw, dh, GL_RGB, GL_UNSIGNED_BYTE, px);
            SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, dw, dh, 24, SDL_PIXELFORMAT_RGB24);
            for (int y = 0; y < dh; y++) memcpy((uint8_t*)s->pixels + (size_t)y * s->pitch, px + (size_t)(dh - 1 - y) * dw * 3, (size_t)dw * 3);
            SDL_SaveBMP(s, shot);
            printf("%s: %d meshes, %ld polygons, cam %.0f %.0f %.0f\n", shot, g_walk.item_count, frame_polys, cam[0], cam[1], cam[2]);
            SDL_FreeSurface(s);
            free(px);
            break;
        }
        SDL_GL_SwapWindow(win);

        frames++;
        if (now - title_at > SDL_GetPerformanceFrequency()) {
            char t[256];
            snprintf(t, sizeof t, "OpenTM2 viewer  %d fps  %d meshes  %ld polygons  LOD %s  cam %.0f %.0f %.0f",
                     frames, g_walk.item_count, frame_polys, lod_on ? "on" : "off", cam[0], cam[1], cam[2]);
            SDL_SetWindowTitle(win, t);
            frames = 0;
            title_at = now;
        }
    }
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
