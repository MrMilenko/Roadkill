/*
 * qol_cars.c: [gameplay] unlock_hidden_cars and play_dark_tooth (qol_cars.h, docs/QOL.md).
 * Port code, not from the original game.
 *
 * unlock_hidden_cars: the original unlocks Sweet Tooth and Minion on the car select itself. Its
 * cheat handler 0x0044e1cc sets pickup counter 18 / 19 (0x004fff24 / 0x004fff28) for the typed
 * codes ICECREAM / BIGBOY (key table 0x004e6a84); the car select's menu hook (0x00477aa0,
 * buttons 0x12, 0x13 from TM2.EXE 0x004e8064) takes the counter and posts scene event 0x12 /
 * 0x13, whose actions store 1 in 0x009f6c1c (Sweet Tooth) / 0x009f6c1d (Minion) (frame +0x974 /
 * +0x95c, src/shell/sh3_car_select.c). Nothing in the game clears the flags again: they last
 * until the game exits. The switch stores the same 1s when the car select starts.
 *
 * play_dark_tooth: Dark Tooth is car 14 (type 0xF0, CARSDB\BB.DPC / .TPC, his own stats in the
 * stats setup 0x0043846c, his name in the car name list, model 0xF0 in the session setup
 * 0x00475af8). What he lacks is the car select's art and the per car tables that end at 13:
 *
 *  - the wheel: the CARSEL scene (SHELLDB1\CARSEL.DPC) has three 14 way switch nodes (type 9):
 *    9005 the car models, 9006 / 9007 the driver picture's two halves; the scene objects +0x4ac,
 *    +0x40c, +0x3bc of the car select point at them (object +4), the choice commands step
 *    within the node's own count (byte +6) and the picture switches copy the car's index. When
 *    the scene's files are loaded (level_files_load 0x004740a8), each switch is copied into a
 *    15 way one: slot 14 of the models is his BB.DPC (its BB.TPC pages appended to the scene's
 *    texture pages), drawn at a quarter through a scale node; slot 14 of the pictures is Sweet
 *    Tooth's. The wheel's filter (0x0047770c) and the name (menuCarNameText 0x00477b2c, the
 *    car name list has "Dark Tooth") already take car 14.
 *  - the car info page: its TIM comes from the 14 entry table 0x004e8024 (car + 0x0e reads
 *    the level select's bytes 0x004e805c); bg_tim_load 0x0046f784 is handed Sweet Tooth's.
 *  - the ending table 0x004e81d0 ends at 13: Sweet Tooth's ending.
 *  - a save made as Dark Tooth (Slot<n>, +0x80 = 14) loaded with the switch off: Sweet Tooth.
 *
 * As the boss he is about four times a car's size (his collision box is 485 long, 173 wide;
 * cars are 89..165 and 50..116, Minion 252), and the chase camera sits inside him. A human's
 * Dark Tooth is drawn at a quarter (a DPC scale node, type 12, shift + 2, put between each root
 * group and its children), the size values of his stats are quartered to match, and so is his
 * probe step height (0xA0, which let him climb the arena walls), so he drives, collides and fires
 * like a car and the chase camera frames him unchanged. A quarter because the scale node only
 * halves: half (242 long) would still be twice the usual car, a quarter (121 x 43, 29 high) is a
 * car's size. sh5_level_load loads the humans' cars before the opponents', so only the first
 * BB.DPC of a level load is changed, and only when a human drives car 14. With a human Dark
 * Tooth the Dark Tooth stage has no boss fight (qolc_level_load).
 *
 * Network games need nothing more: the car reservation (net_join 0x004637c4, per car tables
 * 0x0050f580 / 0x0050f5bc of 15 / 16 entries) takes car 14, and every machine shrinks a human
 * Dark Tooth (any human, local or not); the switch is part of the compatibility word
 * (host/net/net_session.c ns_compat).
 */
#include "qol_cars.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats/dpc.h"
#include "host/qol/qol.h"
#include "host/win32_host.h"
#include "rt.h"

#define GS_TYPE 0x00624ea8u        /* game type */
#define GS_HUMANS 0x00624ea9u      /* humans */
#define GS_LEVEL 0x00624ec7u       /* level */
#define GS_BOSS_DONE 0x00624ec9u   /* the Dark Tooth stage's boss fight was had */
#define GS_CARS 0x00624eadu        /* the humans' cars */
#define UNLOCK_SWEET 0x009f6c1cu
#define UNLOCK_MINION 0x009f6c1du
#define LOAD_NAME 0x009f6920u      /* the file name sh5_level_load just loaded */
#define ROOT_SHIFT 0x004e8448u     /* the DPC walk's starting scale shift */
#define SCENE 0x0050fad8u          /* the shell's scene block */
#define BG_INDEX 0x004e8021u       /* the car select's remembered car */
#define INFO_TIMS 0x004e8024u      /* the car info pages' TIM names, cars 0..13 */
#define CAR_SWEET 12
#define CAR_DARK 14
#define SHRINK 2                    /* a quarter: shift + 2 */

static int human_is_dark(void)
{
    uint8_t n = rt_r8(GS_HUMANS);
    for (uint8_t i = 0; i < n && i < 8; i++)
        if (rt_r8(GS_CARS + i) == CAR_DARK) return 1;
    return 0;
}

static uint32_t human_car(uint32_t i) { return rt_call(0x0045afbcu, &i, 1); }

void qolc_car_select_enter(void)
{
    if (!qol_unlock_hidden_cars()) return;
    rt_w8(UNLOCK_SWEET, 1);
    rt_w8(UNLOCK_MINION, 1);
}

/* ---- the wheel ---- */

typedef struct { uint8_t* p; size_t n; } Blob;

static int read_data(const char* game, Blob* b)
{
    char path[1024];
    w32_host_path(game, path, sizeof path);
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "roadkill: play_dark_tooth: cannot read %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b->p = n > 0 ? malloc((size_t)n) : NULL;
    b->n = b->p && fread(b->p, 1, (size_t)n, f) == (size_t)n ? (size_t)n : 0;
    fclose(f);
    return b->n ? 0 : -1;
}

/* a type 12 scale node at nd: shift, the children */
static void scale_node(uint32_t nd, uint16_t shift, uint32_t kids, uint32_t from)
{
    rt_w32(nd, 0x0000fe0cu);
    rt_w16(nd + 4, shift);
    rt_w16(nd + 6, (uint16_t)kids);
    for (uint32_t k = 0; k < kids; k++) rt_w32(nd + 8 + 4 * k, rt_r32(from + 4 * k));
}

/* BB.TPC appended to the scene's texture pages and BB.DPC relocated against them (fresh copies
 * each time: registering converts palettes in place, relocating rewrites pointers); returns the
 * model's first root, 0 when not available */
static uint32_t wheel_model(void)
{
    static Blob tpc, dpc;
    static uint32_t gt, gd, gs;
    static int failed;
    if (failed) return 0;
    if (!gt) {
        if (read_data("CARSDB\\BB.TPC", &tpc) || read_data("CARSDB\\BB.DPC", &dpc)
            || !(gt = rt_heap_grow((uint32_t)tpc.n)) || !(gd = rt_heap_grow((uint32_t)dpc.n))
            || !(gs = rt_heap_grow(0x40))) {
            failed = 1;
            return 0;
        }
    }
    memcpy(rt_ptr(gt), tpc.p, tpc.n);
    memcpy(rt_ptr(gd), dpc.p, dpc.n);
    uint32_t reg[2] = {gt, 1};
    int32_t first = (int32_t)rt_call(0x00496fd8u, reg, 2);   /* tpc_register, appended */
    if (first < 0) return 0;
    if (dpc_relocate(rt_ptr(gd), dpc.n, gd, (int16_t)first) < 1) return 0;
    DpcHeader h;
    dpc_read_header(rt_ptr(gd), dpc.n, &h);
    uint32_t list = dpc_reloc(h.root_list, h.base, gd), root = rt_r32(list + 4);
    if (!root || rt_r8(root) != 1) return 0;
    /* the mesh setup the scene's own models get (dbscanHierarchy, with an empty object callback
     * 0x00452d28 instead of worldLoadObject: nothing is created) */
    uint32_t scan[3] = {root, h.alloc_count, 0x00452d28u};
    rt_call(0x004a15d4u, scan, 3);
    /* the model under a scale node: a quarter, as the human's car in a match */
    uint32_t kids = rt_r8(root + 0x14);
    if (kids == 0 || kids > 12) return 0;
    scale_node(gs, (uint16_t)(rt_r32(ROOT_SHIFT) + SHRINK), kids, root + 0x18);
    return gs;
}

/* a 15 way copy of the 14 way switch node old, slot 14 = extra */
static uint32_t switch15(uint32_t old, uint32_t extra, uint32_t* store)
{
    if (!*store && !(*store = rt_heap_grow(0x10 + 4 * 15))) return 0;
    uint32_t nd = *store;
    for (uint32_t k = 0; k < 0x10; k += 4) rt_w32(nd + k, rt_r32(old + k));
    for (uint32_t k = 0; k < 14; k++) rt_w32(nd + 0x10 + 4 * k, rt_r32(old + 0x10 + 4 * k));
    rt_w32(nd + 0x10 + 4 * 14, extra);
    rt_w8(nd + 6, 15);
    return nd;
}

static uint32_t store[3];   /* the 15 way switches: models, the picture's two halves */

/* the scene object showing switch node nd, 0 when none */
static uint32_t scene_object_of(uint32_t nd)
{
    static const uint16_t lists[3][2] = {{0x50, 8}, {0x74, 16}, {0xb8, 32}};
    for (int l = 0; l < 3 && nd; l++) {
        uint32_t cnt = rt_r32(SCENE + lists[l][0]);
        for (uint32_t i = 0; i < cnt && i < lists[l][1]; i++) {
            uint32_t obj = rt_r32(SCENE + lists[l][0] + 4 + 4 * i);
            if (obj && rt_r32(obj + 4) == nd) return obj;
        }
    }
    return 0;
}

void qolc_scene_loaded(uint32_t name)
{
    if (!qol_play_dark_tooth() || strcmp((const char*)rt_ptr(name), "SHELLDB1\\CARSEL") != 0) return;
    static const uint16_t ids[3] = {0x232d, 0x232e, 0x232f};   /* 9005 models, 9006 / 9007 picture */
    uint32_t model = 0;
    /* the scene's objects: aux +0x50 / +0x54 [8], decor +0x74 / +0x78 [16], items +0xb8 / +0xbc [32] */
    static const uint16_t lists[3][2] = {{0x50, 8}, {0x74, 16}, {0xb8, 32}};
    for (int l = 0; l < 3; l++) {
        uint32_t cnt = rt_r32(SCENE + lists[l][0]);
        for (uint32_t i = 0; i < cnt && i < lists[l][1]; i++) {
            uint32_t obj = rt_r32(SCENE + lists[l][0] + 4 + 4 * i), node = obj ? rt_r32(obj + 4) : 0;
            if (!node || rt_r8(node) != 9 || rt_r8(node + 6) != 14) continue;
            for (int k = 0; k < 3; k++) {
                if (rt_r16(obj) != ids[k]) continue;
                uint32_t extra;
                if (k == 0) {
                    if (!model && !(model = wheel_model())) return;
                    extra = model;
                } else {
                    extra = rt_r32(node + 0x10 + 4 * CAR_SWEET);
                }
                uint32_t nd = switch15(node, extra, &store[k]);
                if (nd) rt_w32(obj + 4, nd);
            }
        }
    }
}

uint32_t qolc_info_tim(uint32_t name)
{
    if (!qol_play_dark_tooth() || !scene_object_of(store[0])) return name;
    int info = 0;
    for (uint32_t k = 0; k <= CAR_DARK; k++) info |= name == rt_r32(INFO_TIMS + 4 * k);
    if (!info) return name;
    /* the car select keeps the car shown before the info page as (car + 13) % 14 in 0x004e8021
     * (the wheel steps on by one when it comes back): on the 15 way wheel that is (car + 14) % 15 */
    rt_w8(BG_INDEX, (uint8_t)((rt_r8(store[0] + 7) + 14) % 15));
    /* car 14 reads the word after the 14 entry table: Sweet Tooth's page */
    return name == rt_r32(INFO_TIMS + 4 * CAR_DARK) ? rt_r32(INFO_TIMS + 4 * CAR_SWEET) : name;
}

/* ---- the human's Dark Tooth in a match ---- */

static int level_loading, level_shrunk;

void qolc_level_load(int begin)
{
    level_loading = begin;
    level_shrunk = 0;
    /* the Dark Tooth stage of a tournament (level 8) with a human Dark Tooth: no boss fight, as
     * the original skips the Minion fight of New York for a human Minion (sh4_session.c
     * tournament_next). Cars find their object slot by car type (carBindHumanSlot 0x0045803c,
     * car_bind_ai_by_type 0x00458340), so a boss of the human's own type shares the human's
     * object: tried, the fight ends at once. Marking the boss fight done (0x00624ec9) goes on to
     * the stage's last messages and the ending. */
    if (begin && qol_play_dark_tooth() && rt_r8(GS_TYPE) == 1 && rt_r8(GS_LEVEL) == 8 && human_is_dark())
        rt_w8(GS_BOSS_DONE, 1);
}

void qolc_model_loaded(uint32_t buf)
{
    static uint32_t nodes[8];
    const char* name = (const char*)rt_ptr(LOAD_NAME);
    /* once per level load: the humans' cars are loaded before the opponents' */
    if (!level_loading || level_shrunk || !qol_play_dark_tooth() || !human_is_dark()) return;
    size_t len = strlen(name);
    if (len < 6 || strcmp(name + len - 6, "BB.DPC") != 0) return;
    level_shrunk = 1;
    uint32_t list = (uint32_t)((int32_t)(rt_r32(buf + 0x14) - rt_r32(buf + 0xc)) / 4 * 4) + buf;
    uint32_t count = rt_r32(list);
    uint16_t shift = (uint16_t)(rt_r32(ROOT_SHIFT) + SHRINK);
    for (uint32_t i = 0; i < count && i < 8; i++) {
        uint32_t root = rt_r32(list + 4 + 4 * i);
        if (!root) break;
        /* each root is a type 1 group (the object the car entity draws from, worldLoadObject
         * 0x0044f20c): its children go under one scale node, the group's only child */
        if (rt_r8(root) != 1) continue;
        uint32_t kids = rt_r8(root + 0x14);
        if (kids == 0 || kids > 12) continue;
        if (!nodes[i] && !(nodes[i] = rt_heap_grow(0x40))) return;
        scale_node(nodes[i], shift, kids, root + 0x18);
        rt_w8(root + 0x14, 1);
        rt_w32(root + 0x18, nodes[i]);
    }
}

void qolc_stats_done(uint32_t stats, uint16_t car_id)
{
    if (car_id != 0xf0 || !qol_play_dark_tooth() || !human_is_dark()) return;
    int human = 0;
    for (uint32_t i = 0; i < rt_r8(GS_HUMANS) && i < 8 && !human; i++) human = stats == human_car(i) + 0x138u;
    if (!human) return;
    /* the size values (src/game/car.c, car_hit.c, car_spawn.c, cb2_launch.c): +0x3E / +0x48
     * width, +0x42 / +0x44 / +0x46 heights, +0x4A length, +0x4E front, +0x50 rear, +0xD4 / +0xD6
     * / +0xD8 / +0xDC weapon launch offsets */
    static const uint16_t off[] = {0x3e, 0x42, 0x44, 0x46, 0x48, 0x4a, 0x4e, 0x50, 0xd4, 0xd6, 0xd8, 0xdc};
    for (unsigned k = 0; k < sizeof off / sizeof off[0]; k++) {
        int16_t v = (int16_t)rt_r16(stats + off[k]);
        rt_w16(stats + off[k], (uint16_t)(int16_t)(v / (1 << SHRINK)));
    }
}

void qolc_player_frame(uint32_t car)
{
    if (!qol_play_dark_tooth() || rt_r16(car + 0x90) != 0xf0) return;
    if (rt_r32(car + 0x594) == 0xa0) rt_w32(car + 0x594, 0xa0 >> SHRINK);
}

/* ---- per car tables that end at 13: Sweet Tooth's ---- */

void qolc_ending(int before)
{
    static uint8_t swapped[8];
    if (!qol_play_dark_tooth()) return;
    uint8_t n = rt_r8(GS_HUMANS);
    for (uint8_t i = 0; i < n && i < 8; i++) {
        if (before) {
            swapped[i] = rt_r8(GS_CARS + i) == CAR_DARK;
            if (swapped[i]) rt_w8(GS_CARS + i, CAR_SWEET);
        } else if (swapped[i]) {
            rt_w8(GS_CARS + i, CAR_DARK);
            swapped[i] = 0;
        }
    }
}

void qolc_save_loaded(void)
{
    if (!qol_play_dark_tooth() && rt_r8(GS_CARS) == CAR_DARK) rt_w8(GS_CARS, CAR_SWEET);
}
