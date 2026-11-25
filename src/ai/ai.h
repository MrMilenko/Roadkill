/*
 * ai.h: computer car driving (route following, goals, setup), ported from TM2.EXE.
 *
 * AI car blocks: 0x880 bytes each at 0x0050760c (car i = 0x0050760c + i * 0x880, 0x0045b080),
 * count at 0x0050c2b0 (0x0045b078). Every function here takes the block like the original (EAX).
 * Fields used here (offsets in the AI block; the PS1 decomp's names in brackets where they fit):
 *
 *   +0x001 i8   mood [state]: 1 hunts (routes toward the target, may chase), 2 picks the successor
 *               farthest from the target, 3 a random successor (the PS1 notes say 2 attack, 3 flee)
 *   +0x004 i16  car index (byte +4 bit 0 picks the lane side)                 [index]
 *   +0x008 i16  cleared when the car takes damage on a stuck frame
 *   +0x028 i32  distance to the target car                                    [dist]
 *   +0x030 i16  car type (0x10..0xF0; 0xF0 = Dark Tooth, the boss)            [type]
 *   +0x039 u8   "fresh goal" flag (goal distance history reset)
 *   +0x03B u8   stats need re-deriving (top speed restore)
 *   +0x016..01E i16  range bands (320, 160, 600, 1000, 2000 at reset)
 *   +0x01E i16  chase look ahead limit (compared with +0x028)
 *   +0x024 i16  (i32 >> 16 at +0x22) height difference to the target
 *   +0x068      controls block (car.h), +0x069 u8 car airborne (no snapshot when set)
 *   +0x06D u8   keep the speed level (no top speed change at a new node)
 *   +0x07F u8   stuck (set by ai_update_stuck)
 *   +0x43D u8   passing                         +0x43E u8 diagonal segment (current)
 *   +0x43F u8   segment x aligned (|dx| < 40)   +0x440 u8 segment y aligned
 *   +0x441 u8   diagonal (next)  +0x442 u8 next x aligned  +0x443 u8 next y aligned
 *   +0x444 u8   lane side -    +0x445 u8 lane side +    +0x446 u8 sharp turn (|turn| > 0x638)
 *   +0x448 u8   route changed  +0x449 u8 taking the route to the target's zone
 *   +0x44B u8   aim point valid  +0x44E u8 cleared when not blocked  +0x44F u8
 *   +0x450 u8, +0x451 u8  Dark Tooth jump flags (0x00432f2c)
 *   +0x456 i16  previous node   +0x458 i16 current node   +0x45A i16 next node   (PTS indices)
 *   +0x45C i16  route node (GRP take-route node toward the target's zone, with +0x449)
 *   +0x45E i16  zone (GRP zone index)  +0x460 i16  +0x464 i16 times the previous node was re-picked
 *   +0x466 i16  frames on this segment                    +0x468 i16 blocked frames this update
 *   +0x472 i16, +0x484 i16 (0x800), +0x486 i16 (0x7FFF), +0x43C u8, +0x44A u8, +0x453 u8: route
 *               decision state reset by the route roll / replan
 *   +0x46C i16  side offset   +0x46E i16 lane   +0x470 i16 next lane (PTS lane byte, -2..3)
 *   +0x474 i16  heading copy  +0x476 i16 heading error to the aim point
 *   +0x478 i16  segment speed +0x47A i16 copy
 *   +0x47C i16  heading of the current segment  +0x47E i16 next segment heading
 *   +0x480 i16  turn at the next node (next - current, wrapped)
 *   +0x482 i16  bearing to goal 2    +0x488 i16 bearing to the goal
 *   +0x48A u8   pass side (0, 1 left, 2 right)
 *   +0x48C/490  i32 current segment delta   +0x494/498 i32 next segment delta
 *   +0x49C i32  pass offset (sideways look ahead)  +0x4A0 i32 lateral offset x from the segment
 *   +0x4A4/4A8  i32 current corner point   +0x4AC/4B0 i32 next corner point
 *   +0x4B4 i32  goal distance  +0x4B8 i32 previous goal distance  +0x4BC i32 goal 2 distance
 *   +0x4C0 i32  segment length +0x4C4 i32 next segment length     +0x4C8 i32 heading error copy
 *   +0x4CC/4D0  i32 previous goal  +0x4D4/4D8 i32 goal   +0x4DC/4E0 i32 goal 2 (next node + side)
 *   +0x4E4/4E8  i32 aim point
 *   +0x4EC      stats block (car_stats_derive, src/game/car.c). Read here: +0x514 i16 frames
 *               stuck (stats +0x28), +0x516 i16 (stats +0x2A), +0x53E i16 (stats +0x52),
 *               +0x544 i16 top speed (stats +0x58), +0x562 i16 (stats +0x76), +0x564 i16
 *               (stats +0x78), +0x588 i32, +0x58C i32 health (stats +0xA0), +0x526 i16 maximum
 *               health, +0x52C i16 placement successor, +0x52E i16 start height,
 *               +0x5A0 i32 (stats +0xB4), +0x5AC i32 (stats +0xC0)
 *   +0x5BC i32  placement (spawn) node
 *   +0x5D0      terrain probes (car.h)
 *   +0x834      weapon block (+0x83E u8 skill, +0x842 i16, +0x85A/85C i16, +0x86C/870 profile tables)
 *   +0x708      body (car.h): +0x710 i16.. speed (+0x712), +0x728/72C/730 i32 position,
 *               +0x73C i16 heading, +0x758 matrix
 *
 * PTS nodes (0x004fe1e4, 0x18 bytes each, loaded from the level's .PTS file):
 *   +0x00 i32 x  +0x04 i32 y  +0x08 i16 z  +0x0A i16[4] successors  +0x12 i8[4] lane per successor
 *   +0x16 u8 successor count  +0x17 i8 zone
 * GRP (0x004fca18, the level's .GRP file, see src/physics/grp.h). Read here besides grp.h:
 *   +0x0354, +0x081D, +0x0CE6  35 x 35 i8 route tables [from zone * 35 + to zone]
 *   +0x11AF i8[35]  per zone value (x64 = range)   +0x1218 u8[35] per zone flag
 */
#ifndef TM2_AI_H
#define TM2_AI_H

#include <stdint.h>

#define AI_BLOCK_SIZE 0x880
#define AI_NODE_SIZE 0x18

#define PLAYER_BLOCK_SIZE 0x898

typedef struct {
    const uint8_t* nodes;   /* PTS node table 0x004fe1e4 */
    const uint8_t* grp;     /* GRP file 0x004fca18 */
    uint8_t event_type;     /* 0x00624ec7 (0x00474508) */
    int32_t frame_parity;   /* 0x00a0364c (0x004931f4) */
    int32_t difficulty;     /* 0x00502f98 (0x0045b3dc): 0, 1, 2 */
    /* cars, for the target lookup (0x0045afc8) */
    uint8_t* ai_cars;              /* AI blocks 0x0050760c, 0x880 apart */
    uint8_t* players;              /* player car blocks 0x005030a8, 0x898 apart */
    int32_t player_count;          /* 0x005075e0 */
    const int32_t* target_slot;    /* 0x00502fb4: per AI car, the AI car it chases, -1 = nearest player */
    /* 0x004543f8(kind, ai, 0): Dark Tooth level warp (kind 1 / 2), not ported */
    void (*boss_warp)(void* user, int32_t kind, uint8_t* ai);
    void* user;
    int32_t node_count;            /* 0x004fe1b0 */
    int32_t ai_count;              /* 0x0050c2b0 (0x0045b078) */
    /* effects at a position (the car's +0x728), not ported: 0x00450984(pos, 0) teleport flash,
     * 0x0045109c(pos, 0) smoke puff */
    void (*fx_flash)(void* user, uint8_t* pos);
    void (*fx_smoke)(void* user, uint8_t* pos);
    /* 0x0042d470(ai, 0) car_drive (src/game/car.c, needs the engine's surfaces and effects) */
    void (*drive)(void* user, uint8_t* ai);
    int32_t game_mode;             /* 0x00502fa4 (0x0045c338): < 4 single player kinds */
    uint8_t flag_502f7c;           /* 0x00502f7c (0x0045b2fc) */
    uint8_t flag_50c2c4;           /* 0x0050c2c4 (0x0045b2f4) */
} AiEnv;

/* ---- ai_steps.c ---- */

/* 0x00425674: distance (+0x4B4, previous in +0x4B8) and bearing (+0x488) from the car to the goal
 * (+0x4D4/4D8) moved sideways by the pass offset (+0x49C) along the car's matrix. The history is
 * reset (and +0x44F cleared) when +0x39 is set or the car has been on the segment < 10 frames. */
void ai_goal_bearing(uint8_t* ai);

/* 0x0042577c: distance (+0x4BC) and bearing (+0x482) from the car to goal 2 (+0x4DC/4E0). */
void ai_goal2_bearing(uint8_t* ai);

/* 0x004259ec: stuck bookkeeping, once per route step: counts blocked updates (+0x514), flags the
 * car stuck (+0x7F) after 7, takes health (+0x58C) on some frames after 25, restores the top speed
 * when +0x3B asks for it, clears the per update counters. */
void ai_update_stuck(uint8_t* ai, const AiEnv* env);

/* 0x00430460: the car's offset from the current (which = 1) or next (which = 0) segment, in the
 * segment's frame: out[0] across, out[1] along (one of them 0), out[2] = 0 rotated. */
void ai_route_lateral_offset(const uint8_t* ai, int32_t out[3], int which);

/* 0x004291dc: lateral offset x into +0x4A0, and the pass offset (+0x49C), side (+0x48A) and the
 * passing flag (+0x43D) for side = 0 (none), 1 (left) or 2 (right). */
void ai_set_pass_lane(uint8_t* ai, int side);

/* 0x00427dd8: aim point (+0x4E4/4E8): the current node (for Dark Tooth shifted sideways by the
 * side offset along the goal bearing) plus a look ahead along the next segment growing with speed;
 * heading error to it (+0x476) and a heading copy (+0x474). */
void ai_set_route_aim_point(uint8_t* ai, const AiEnv* env);

/* 0x0042ce2c: in event types 5 and 6, 1 when the car is closer than 480 to an edge of its
 * (unrotated, rectangle or octagon) zone. Returns 0 otherwise. */
int ai_near_zone_edge(const uint8_t* ai, const AiEnv* env);

/* 0x00439588: default top speed of a car type (as an int16 id). */
int32_t car_type_top_speed(uint16_t type);

/* 0x0045b3fc: 1 for car types 0x00, 0x30, 0x70, 0xA0, 0xB0, 0xC0 (the melee special cars). */
int car_has_melee_special(uint16_t type);

/* 0x0043953c: stats +0x52 = top, +0x54 = (+0x52) * 3 / 4, +0x5A = (+0x52) * 2 / 3, then
 * car_stats_derive. */
void car_stats_set_top_speed(uint8_t* stats, int16_t top);

/* 0x00433000: segment speed (+0x478, +0x47A) and top speed (+0x544) for the next node; next
 * segment delta (+0x494/498), its alignment flags (+0x441..443) and corner point (+0x4AC/4B0).
 * turn = 0: plain cruise values; else from the turn at the node, lane, difficulty. */
void ai_set_segment_speed(uint8_t* ai, int turn, const AiEnv* env);

/* ---- ai_route.c ---- */

/* 0x0045afc8: the car AI car `index` chases: AI car target_slot[index], or (slot -1) the player car
 * nearest to it (0x0045acb8: smallest |dx| + |dy| below 0x7FFF among player cars with +0x1D8 > 0,
 * else player 0). *is_player = 1 for a player block (0x898 layout), 0 for an AI block. */
uint8_t* ai_target_car(const AiEnv* env, int32_t index, uint8_t* is_player);

/* 0x0045acb8: index of the player car nearest to pos (i32 x, y) by |dx| + |dy|, among cars with
 * +0x1D8 > 0 other than `skip`, closer than 0x7FFF; 0 if none. */
int32_t ai_nearest_player(const AiEnv* env, const uint8_t* pos, int32_t skip);

/* 0x00432f2c: Dark Tooth only, at nodes 2, 11, 40, 44 inside two boxes of the boss arena: +0x451 = the
 * target is on one of zones 8..11, 14, 15, 17, 18 (0x0045c300); else +0x450 = 1. */
void ai_boss_jump_check(uint8_t* ai, const AiEnv* env);

/* 0x00432ba0: the car reached its next node: node history and segment values move on, top speed from
 * the new lane (unless +0x6D), side offset and side, goals shift, route change flag, segment speed
 * (turn = 0), current segment delta / flags / corner, pass lane reset; next node = previous node
 * until it is chosen. */
void ai_advance_route_node(uint8_t* ai, const AiEnv* env);

/* 0x004334c8: choose the next node (+0x45A) and its lane (+0x470): `node` if not -1; mood 3 a random
 * successor other than the previous node; else the successor nearest to (mood 2: farthest from) a
 * point toward the target car, through the GRP route tables when the target is in another zone. Then
 * next segment length (+0x4C4), heading (+0x47E), goal 2 (+0x4DC/4E0), turn (+0x480), sharp turn. */
void ai_choose_next_node(uint8_t* ai, int32_t node, const AiEnv* env);

/* 0x00427838: route state reset at a new decision; a hunting car (mood 1, < 600 frames on the
 * segment) close enough to its target may decide (randomly, by event type and difficulty) to take the
 * GRP route toward the target's zone: +0x449 and the route node +0x45C. */
void ai_roll_take_route(uint8_t* ai, const AiEnv* env);

/* ---- ai_replan.c ---- */

/* 0x0042c3e0 with the AI layout (kind 0): the node of the car's zone (+0x45E; zones 4..7 count as
 * 0x18 in event type 8) nearest to the car by |dx| + |dy| + |dz| (first of equals, 0 if none). */
int32_t ai_nearest_node(const uint8_t* ai, const AiEnv* env);

/* 0x00432940: puts the car on the route at node +0x5BC: walks the node history in from a random
 * predecessor, picks the next node after the one in +0x52C, moves the car onto the node (zone floor
 * height, heading of the segment, speed 0), resets the probes to the zone and node heights and runs
 * one car_drive step. */
void ai_place_on_route_node(uint8_t* ai, const AiEnv* env);

/* 0x00427710: replans from the nearest node; a stuck car is put back on the route (with a flash and
 * smoke); then the node advance, goal bearing and route state reset. */
void ai_replan_route(uint8_t* ai, const AiEnv* env);

/* 0x0042f6d4: every AI car in `zone` or heading into or out of it re-rolls its route and goes to
 * `node` (zone collapse / obstacle events). */
void ai_reroute_zone(int32_t zone, int32_t node, const AiEnv* env);

/* ---- ai_decide.c ---- */

/* 0x004295fc: 1 when the car should drive straight at its target instead of following the route,
 * with `range` the base chase distance (event type 5: only melee special cars, in the target's zone,
 * via +0x39). Dark Tooth never chases into the closed parts of its arena (boxes, inlined; the PS1
 * has them as aiPointInLevelArea). */
int ai_should_chase_direct(uint8_t* ai, int32_t range, const AiEnv* env);

/* 0x00429f7c: for player car `human`: in game modes < 4 the AI car nearest to it (0x0045ad38: alive =
 * AI +0x58C > 0, |dx| + |dy| < 0x7FFF, else AI car 0), *is_player = 0; else with more than one player
 * the nearest other player car, *is_player = 1; else NULL (*is_player untouched). */
uint8_t* car_nearest_to_human(int32_t human, uint8_t* is_player, const AiEnv* env);

/* ---- ai_setup.c ---- */

/* 0x004325f8: the Dark Tooth level's route fixes, applied to the loaded PTS nodes and GRP route
 * tables: successor counts and links of the boss arena nodes, nodes 84..95 turned into one way links
 * (x = -4800 or 4800, one successor), and the routes out of zone 24 (the warp destination). */
void ai_route_patch_boss_level(uint8_t* nodes, uint8_t* grp);

/* 0x004102f4: per car type driving profile: +0x86C / +0x870 = TM2.EXE addresses of the type's two
 * profile tables (0x004e64dc + 0x18 * (type / 0x10 - 1), +0x0C), +0x83E skill byte = event type * 3 +
 * difficulty * 12 + a per type base (fixed 0x50 for types 0x10, 0xE0, 0xF0), +45 in event types 5
 * and 6, +15 in game mode 2, 100 in game mode 3 with one AI car, at most 100. Other types keep the
 * old values (the skill adjustments still apply). The port stores the original addresses as table
 * ids; the engine resolves them in the player's TM2.EXE. */
void ai_car_setup_profile(uint8_t* ai, const AiEnv* env);

/* ---- ai_misc.c ---- */

/* 0x0044a40c: pos jittered by rand() / div per axis (+ on an odd rand, - on an even one), written
 * to out (the original's static vector 0x004fef70, which it returns). ORIGINAL: an axis whose div
 * is 0 is not written, so it keeps the previous call's value. */
void math_jitter_pos(const int32_t pos[3], int32_t dx, int32_t dy, int32_t dz, int32_t out[3]);

/* 0x004107c0: the 5 event object records at 0x004f004c (0xC4 bytes each): for each active one
 * (+0x00 != 0), +0x0C i16 = grp_find_zone(+0x44, +0x48, +0x4C). */
void event_objects_find_zones(uint8_t* records, const uint8_t* grp);

#endif /* TM2_AI_H */
