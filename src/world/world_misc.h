/*
 * world_misc.h: small world / arena helpers ported from TM2.EXE (leaves of the arena setup and
 * update code). PS1 decomp names in brackets are the pairs from tools/match/pairs.tsv; where the
 * PC function does something else it says so.
 *
 * Pointer fields inside the original's records and globals are kept as 32 bit handles (the
 * original's pointers), so the records keep the original layout; a resolve callback turns a
 * handle into memory where a function writes through it.
 *
 * Verified against the original under emulation (tools/emu/check_world_misc.sh).
 */
#ifndef TM2_WORLD_MISC_H
#define TM2_WORLD_MISC_H

#include <stdint.h>

/* objRegistrySetStep 0x00470320(id, a, b) [PS1 name, not ported]: used as a callback below. */
typedef void (*WorldSetStepFn)(void* user, int32_t id, int32_t a, int32_t b);

/* ---- missile records: 16 x 0x4BC at 0x004f11dc ----------------------------------------------
 *   +0x000 u8  index      +0x004 i16 life (-15 = idle)      +0x034 ptr object
 * Object fields written: +0x008 i32 = 0, +0x00C i32 = 0, +0x158 i32 = 10 (registry kind). */
#define WORLD_MISSILE_COUNT 16
#define WORLD_MISSILE_SIZE 0x4BC

/* 0x00415b3c [fxRecordGet]: missile record i (0..15), else 0. */
uint8_t* world_missile_record(uint8_t* records, int32_t i);

typedef struct {
    uint32_t (*entity_alloc)(void* user);               /* 0x00492d90 (takes no argument;
                                                           Ghidra gives it a spurious one) */
    void (*entity_insert)(void* user, uint32_t entity); /* 0x00492c4c */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    void* user;
} WorldEntityEnv;

/* 0x00413960 [missilesCreateObjects]: gives each missile record a new object (allocated and
 * inserted into the entity list), registry kind 10, life -15, index i. */
void world_missiles_create(uint8_t* records, const WorldEntityEnv* env);

/* ---- AI routes ---------------------------------------------------------------------------- */

/* 0x00430414 [aiRouteSpeedLimit]: speed limit between zones a and b:
 * (table[a * 35 + b] * 19 * ticks60) / 100 << 5 (signed division). table = 0x004fdc99, which is
 * GRP block + 0x1281 (a 35 x 35 byte table). ticks60 from tm2_frame_ticks60(). */
int32_t world_ai_route_speed_limit(const uint8_t* table, int32_t a, int32_t b);

/* 0x004325c8 [aiRoutePatchEiffelTower]: sets 7 route bytes to 2. base = 0x004fea6a; the bytes at
 * +0x00, +0x30, +0x48, +0x78, +0xF0, +0x150, +0x180. */
void world_ai_route_patch_eiffel(uint8_t* base);

/* 0x0045b488 [aiDifficultySpeedScale]: 0x78 for mode 1, 0x8C for mode 2, else 0x4B. mode is the
 * u32 at 0x00502f98, compared unsigned (Ghidra shows a signed "< 2", which would give 0x78 for
 * negative values: DECOMPILER TRAP). */
int32_t world_ai_difficulty_speed_scale(uint32_t mode);

/* ---- Paris: Eiffel tower ------------------------------------------------------------------
 * State block at 0x004f08e0 (offsets from there):
 *   +0x00 u8 = 0   +0x02 u8 = 0   +0x1C i32 = 0   +0x20 i32 = -0x1800   +0x24 i32 = 0xF0
 *   +0x28 i16[3] rotation = 0      +0x0C ptr tower object (its +0x30 i32 = 0, +0x04 matrix)
 */
typedef struct {
    uint8_t* state;             /* 0x004f08e0 */
    uint8_t* tower;             /* *(0x004f08ec) */
    WorldSetStepFn set_step;
    void* user;
} WorldEiffelEnv;

/* 0x004125b0 [eiffelTowerReset]: resets the tower state, set_step(0xC80, 0, 0), then rebuilds the
 * tower's matrix (+0x04) from the zero rotation with math_build_matrix 0x0048c360. */
void world_eiffel_reset(const WorldEiffelEnv* env);

/* ---- GRP section list ---------------------------------------------------------------------- */

/* 0x0043bd74 [worldSectionPolyReset]: removes `section` from the i16 list at 0x004fe1b4 whose
 * count is the signed byte at list + 0x2D (0x004fe1e1). Each match is closed up by moving the
 * following entries (count - i of them, reading one entry past the end) down one slot; the count
 * is decremented and the scan goes on at the next index, so a match moved into the current slot
 * is skipped (ORIGINAL BUG: of two adjacent matches only the first is removed). The count is
 * reread from memory after every store, as the original does (entry 22 overlaps it). list must
 * have 2 * 128 + 2 bytes. */
void world_section_list_remove(uint8_t* list, int32_t section);

/* 0x0045c300 [worldSectionHasMovingSurfaces]: 1 for sections 4..11, 14, 15, 17, 18. */
int world_section_has_moving_surfaces(int32_t section);

/* ---- effect records --------------------------------------------------------------------- */

/* 0x00451458 [fxSpritesReset]: 150 sprite records of 0x30 at 0x00500304 (+0x10 i32 = 0,
 * +0x0C i16 = 0, +0x07 u8 = index, +0x0A u8 = 0xFF), then clears the first i32 of 20 records of
 * 0x24 at 0x00500050. */
void world_fx_sprites_reset(uint8_t* sprites, uint8_t* trails);

typedef struct {
    uint8_t* slots;             /* 0x00501f26: 4 animation slots of 0x20 */
    /* 0x00496d04(src, dst, 4) */
    void (*parse)(void* user, const uint8_t* src, uint8_t* dst, int32_t n);
    /* 0x004a1f1e printf like error: ("\nError Parsing Fragment Sprite %d\n", id) */
    void (*error)(void* user, int32_t id);
    void* user;
} WorldDebrisAnimEnv;

/* 0x00451604 [explodeStoreDebrisAnimation]: a record whose first byte is not 0 is an error;
 * else ids 0x29E..0x2A1 parse it into slot id - 0x29E. Other ids: nothing. */
void world_debris_animation_store(int32_t id, const uint8_t* rec, const WorldDebrisAnimEnv* env);

/* 0x00430b5c [fxRandomDebrisBurst]: rand() & 3 picks the debris kind for 0x004516bc(kind, p);
 * then rand() & 15: 0 -> 0x00451014(p, 0), 1 -> 0x004512b4(p, 0), 2..5 -> 0x00451274(p, 0). */
typedef struct {
    void (*debris)(void* user, int32_t kind, int32_t* p);   /* 0x004516bc */
    void (*fx_a)(void* user, int32_t* p, int32_t arg);      /* 0x00451014 */
    void (*fx_b)(void* user, int32_t* p, int32_t arg);      /* 0x004512b4 */
    void (*fx_c)(void* user, int32_t* p, int32_t arg);      /* 0x00451274 */
    void* user;
} WorldDebrisBurstEnv;
void world_fx_random_debris_burst(int32_t* p, const WorldDebrisBurstEnv* env);

/* ---- arena objects ------------------------------------------------------------------------ */

/* 0x00453e90 [slamClawRegister]: an object of kind 5 becomes the slam claw: *claw = obj,
 * *claw_id = 0x2E3 (0x00502e94 / 0x00502e9c) and 0x0048c50c(0x2E3, obj + 4) builds its matrix.
 * rot_x is that call. */
void world_slam_claw_register(uint32_t handle, uint8_t* obj, uint32_t* claw, int32_t* claw_id,
                              void (*rot_x)(void* user, int32_t angle, uint8_t* out), void* user);

/* 0x00457398 [fxTargetHitAt]: when the target object (0x00502f60) exists, moves it to v
 * (+0xE8, +0xEC, +0xF0) and set_step(0x29D, 0, 1). */
void world_fx_target_hit_at(uint8_t* target, const int32_t v[3], WorldSetStepFn set_step, void* user);

/* ---- track vehicles: two records of 0x38 at 0x00502988 (pointer at 0x00502f18) -------------
 * (the same memory as the collapsing sections table, see collapse.h; the arenas use one or the
 * other)  +0x0C i32  +0x34 ptr object */

/* 0x00455e94 [worldTrackVehiclesReset]: clears +0x0C and +0x34 of both records and points the
 * table pointer at them. */
void world_track_vehicles_reset(uint8_t* table, uint32_t table_handle, uint32_t* table_ptr);

/* 0x00455ec0 [worldTrackVehicleAttach]: obj != 0: record i +0x34 = obj, obj +0x158 = i + 100,
 * then start(i) (0x00455ef0, not ported). */
void world_track_vehicle_attach(uint8_t* table, uint32_t handle, uint8_t* obj, int32_t i,
                                void (*start)(void* user, int32_t i), void* user);

/* 0x00456c70 [trackVehiclesShouldStop]: 1 if any human car's section (i16 at +0xC6 of its block)
 * has moving surfaces (world_section_has_moving_surfaces). */
int world_track_vehicles_should_stop(int32_t car_count, uint8_t* (*car)(void* user, int32_t i), void* user);

/* ---- scene (front end) objects ------------------------------------------------------------ */

/* 0x00469cdc [sceneObjGetChoiceState]: the object at holder +4; kind 9 -> its byte +7, else (or
 * none) 0xFF. holder_obj is that pointer. */
uint8_t world_scene_choice_state(const uint8_t* holder_obj);

/* 0x0047070c (paired with parisSinkZoneContains, which is WRONG: it steps choices): for each of
 * the 3 objects of row `row` (0x006211dc, 3 pointers per row) that exists, steps its choice
 * (+7) forward (dir >= 0, wraps to 0 at the count +6) or back (wraps to count - 1, compared
 * unsigned). Rows above 15 do nothing. */
void world_scene_choice_step(const uint32_t* rows, int32_t row, int32_t dir,
                             uint8_t* (*resolve)(void* user, uint32_t handle), void* user);

/* ---- shared ---------------------------------------------------------------------------- */

/* The original's inline integer square root (0x00456dfc, 0x0045c3e3: bit by bit from 0x40000000;
 * each "jl" after a subtraction is a signed compare of the two operands, so a negative input
 * gives 0). */
uint32_t world_isqrt_inline(int32_t n);

/* ---- player cars ------------------------------------------------------------------------- */

/* 0x0045ba64 [sub_8011E114]: resets human car block `car` (0x005030a8 + i * 0x898):
 * player_car_setup 0x00437850(car, car id = i16 +0x90, handicap[i16 +0x00]) with handicap =
 * the bytes at 0x00502f80, then 0x0047068c(car id, 0) and +0x13D = 0. (The original also loads
 * the signed byte +0x13C into ebx for the second call, which overwrites it unread.) */
void world_player_car_reset(uint8_t* car, const uint8_t* handicap,
                            void (*setup)(void* user, uint8_t* car, int32_t id, int32_t b),
                            void (*select)(void* user, int32_t id, int32_t z), void* user);

/* 0x0045c350 [demoWatchNextAiCar]: the attract / spectator camera moves on to the next AI car.
 * Player block (0x005030a8): +0x0C/+0x0E/+0x10 i16 camera angles, +0x14 i32[3] camera position,
 * +0x106 u8 view, +0x6E4 i32[3] car position. AI blocks (0x0050760c, 0x880 each): +0x728 i32[3]
 * position, angles in the low halves of +0x734/+0x738/+0x73C. */
typedef struct {
    uint8_t* player;            /* 0x005030a8 */
    const uint8_t* ai;          /* 0x0050760c */
    int32_t* index;             /* 0x00502f90: watched AI car */
    int32_t ai_count;           /* 0x0050c2b0 */
    int32_t look_mode;          /* 0x00502fd8 */
    int32_t hold;               /* 0x00502fb0 */
    int32_t* cleared;           /* 0x00502fac, set to 0 */
    void (*set_state)(void* user, uint8_t* car, int32_t state);   /* 0x004225a0 */
    void* user;
} WorldDemoWatchEnv;
/* look_mode 0: next index (wraps at ai_count), the camera takes that car's angles and position,
 * set_state(player, 7). Else: view 5; unless hold, next index and the camera angles look from
 * the player car at that AI car (pitch -atan2(dz, |dxy|), roll 0, yaw atan2(dx, dy)). */
void world_demo_watch_next(const WorldDemoWatchEnv* env);

#endif /* TM2_WORLD_MISC_H */
