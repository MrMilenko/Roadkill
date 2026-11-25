/*
 * car_hit.h: turning a car's collision hit records into per part hit summaries, ported from
 * TM2.EXE 0x00419604 (wall branch; the car / object branch is delegated).
 *
 * car_collide (scene/car_collide.h) leaves one 0x20 byte hit record per collision point. This
 * step runs over the first six, after the walk, from the all cars update 0x00458470:
 *   - special volume ids are consumed first (0xBBA, 0x191..0x1E0, 0x29A, 0x2B6..0x2B9, 500,
 *     0x208, 0xE10 for AI cars, and the 0xF0 car's own rules); their effects are callbacks;
 *   - any other hit gets a summary: the impact angle (atan2 of the push normal), the bounce
 *     angle against the car's travel angle, the normal and depth;
 *   - wall hits (no entity, an entity type of 6000 and up, or type 9) also get a bounce speed
 *     from the impact speed and angle, and report the wall damage (callback);
 *   - hits from below (the push normal mostly down) are dropped from the records.
 *
 * Block offsets (player / AI):  hit base +0x21C / +0xCC: records at base +0x18 (0x20 each),
 * summaries at base +0x19C (0x24 each). Summary fields: +0 u8 resolved, +1 u8 wall, +2 u8
 * consumed by a special id, +3 u8 point index, +4 i16 bounce speed, +6 i16, +8 i16, +0x0A i16
 * impact angle, +0x0C i16 bounce angle, +0x0E i16 depth, +0x10 i16, +0x18..0x20 i32 normal
 * (negated for walls).
 * Verified against the original under emulation (tools/emu/check_car_hit.sh).
 */
#ifndef TM2_CAR_HIT_H
#define TM2_CAR_HIT_H

#include <stdint.h>

typedef struct {
    /* 0x00450de4(pos, 1): effect at a position, returns its object (stored at turbo block +0x44) */
    uint32_t (*effect)(void* u, const int32_t pos[3]);
    /* 0x004499ac(car, player, id a, id b): ids 0x191..0x1E0 (first special hit, not param 3) */
    void (*special)(void* u, uint8_t* car, int player, int32_t a, int32_t b);
    /* 0x00470d3c(id a, id b, n) */
    void (*pickup)(void* u, int32_t a, int32_t b, int32_t n);
    /* 0x0045a8bc(id a) */
    int (*check)(void* u, int32_t a);
    /* 0x00421920(car, player, kind, flag) */
    void (*set_state)(void* u, uint8_t* car, int player, int kind, int flag);
    /* 0x004543f8(kind, car, player) */
    void (*event)(void* u, int32_t kind, uint8_t* car, int player);
    /* 0x00415d1c(entity type, id a, id b, n, body + 0x38, 0): wall damage */
    void (*wall_damage)(void* u, int32_t type, int32_t a, int32_t b, int32_t n, uint8_t* body);
    /* entity +0x158 (the hit entity's type) */
    int32_t (*entity_type)(void* u, uint32_t entity);
    /* the car / object branch (not ported): returns nonzero to drop the record */
    int (*hit_entity)(void* u, uint8_t* car, int player, int index);
    void* user;
} CarHitEnv;

/* 0x00419604: car = the car block, player = kind, quiet = param 3 (no special effects). */
void car_hits_resolve(uint8_t* car, int player, int quiet, const CarHitEnv* env);

/* What the merge and push steps need from outside the car block. */
typedef struct {
    /* 0x0041fa98(active, summary): an active car hit meets a wall summary; the original hands the
     * car hit to the other car (looked up by active +0x12) before taking the wall */
    void (*pass_car_hit)(void* u, uint8_t* car, int player, uint8_t* active, uint8_t* summary);
    /* the second loop of 0x0041f40c (car to car contact summaries at hit base +0x274), not
     * ported: called when it would run */
    void (*car_contacts)(void* u, uint8_t* car, int player);
    /* crash rumble at the body position: 0x0046f974 / 0x004b8010 position, then, when controls
     * +0 is clear, display_transition 5 and 3 with these strengths */
    void (*crash)(void* u, const int32_t pos[3], int show, int32_t strength5, int32_t strength3);
    /* 0x0045b154(index, player) then 0x004210e4(that, amount, normal, 0, other): damage; other is
     * on the stack (dropped by the decompiler): -1 for walls, active hit +0x14 for car hits */
    void (*damage)(void* u, int32_t index, int player, int32_t amount, const int32_t* normal, int32_t other);
    /* 0x00420be8(car, kind, 2, 2, player): crash state */
    void (*crash_state)(void* u, uint8_t* car, int kind, int player);
    /* 0x00450718(pos, 0): sparks */
    void (*sparks)(void* u, const int32_t pos[3]);
    void* user;
} CarHitApplyEnv;

/* 0x0041f40c: clears the active hit (hit base +0x34C, 0x24 bytes, summary layout), then merges
 * every resolved wall summary of the first `points` into it (bounce speeds and the +6 / +8 / +0x0C
 * angles averaged, deepest depth, highest point index, normals averaged); plays the crash rumble
 * by body velocity +0x18..0x1C (read at the player offsets for both kinds, original quirk);
 * hit base +2 = hit this frame, +0x10 counts frames since the last hit. points = the model's
 * collision point count (model slot +0xF4). */
void car_hits_gather(uint8_t* car, int player, int32_t points, const CarHitApplyEnv* env);

/* 0x00420738(car, player, car_hit, angle, index): turns the heading by angle, slows the car by
 * how hard the hit was, aims the bounce (hit base +8) and may start a spin out. */
void car_hit_turn(uint8_t* car, int player, int car_hit, int32_t angle, int32_t index, const CarHitApplyEnv* env);

/* 0x0041fc3c: applies the active hit: damage, car_hit_turn, then pushes the body out along the
 * impact angle by the depth and away from the wheel corner that hit, and makes sparks on hard
 * hits. */
void car_hit_push(uint8_t* car, int player, const CarHitApplyEnv* env);

/* 0x00438320: the car's six collision points into its entity (+0xF4 = 6, +0xF8 packed i16 x, y,
 * z, 8 bytes apart) from its stats: four corners at half width (stats +0x48), front (+0x4E) and
 * rear (+0x50), height 0x18 - stats +0x42; then the front centre at that height and at
 * stats +0x46. */
void car_collision_points(const uint8_t* car, int player, uint8_t* entity);

/* The per car step of the all cars update (0x00458470, inline) after the collision walk: while
 * the car has gone more than two frames without a hit (hit base +2 clear, +0x10 > 2), keep its
 * last clear state: body +0x18..0x1C = body +0x08..0x0C, +0xE0 = position, +0xEC = +0x38..0x40,
 * +0xF8 matrix = +0x50 matrix. These are the "last frame" position and matrix car_collide picks
 * faces with. With a hit, car_hit_push runs instead. Ported by reading. */
void car_hit_step(uint8_t* car, int player, const CarHitApplyEnv* env);

/* What the hit recovery needs from outside the car block. */
typedef struct {
    /* the car pair push table at 0x0050d0d8 (0x0045c1c4): 15 x 15 int32 vectors, row = the other
     * car (active hit +0x14), column = this car's index */
    const int32_t* pair_push;
    /* 0x0042c2d4 grp_ground_height(pos) */
    int32_t (*ground_height)(void* u, const int32_t pos[3]);
    void* user;
} CarRecoverEnv;

/* 0x0042026c(car, player): the frames after a hit. While the hit timer (hit base +6) runs, the
 * push left in hit base +0x12 moves the body along the wall normal (or the car pair vector for car
 * hits), clamped to a per frame step, keeps the body on the ground, and eases the travel angle
 * (body +0x34) toward the bounce angle (hit base +8). The hit ends when the timer runs out, or
 * for walls once the car is slow or aimed along the bounce. Hit base +0x0A counts frames up to
 * 0x7D00. */
void car_hit_recover(uint8_t* car, int player, const CarRecoverEnv* env);

#endif /* TM2_CAR_HIT_H */
