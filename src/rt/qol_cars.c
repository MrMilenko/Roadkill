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
 * play_dark_tooth: Dark Tooth is car 14 (type 0xF0, CARSDB\BB.DPC / .TPC, its own stats in the
 * stats setup 0x0043846c, its name in the car name list). The car select cannot show him: the
 * CARSEL scene's car wheel is a 14 way switch node with 14 car models and 14 driver pictures,
 * and there is no BIOS\BBINFO.TIM. So in one player tournament / challenge (game types 1 and
 * 3, where the pick is stored at once and no other machine reserves cars) the Sweet Tooth slot
 * stands for him: it is always selectable, labelled "Dark Tooth", and its pick becomes car 14.
 * As the boss he is about four times a car's size (his collision box is 485 long, 173 wide;
 * cars are 89..165 and 50..116, Minion 252), and the chase camera sits inside him. For the
 * human's Dark Tooth the model is drawn at a quarter (a DPC scale node, type 12, shift + 2, put
 * between each root group and its children), the size values of his stats are quartered to
 * match, and so is his probe step height (0xA0, which let him climb the arena walls), so he
 * drives, collides and fires like a car and the chase camera frames him unchanged. A quarter
 * because the scale node only halves: half (242 long) would still be twice the usual car, a
 * quarter (121 x 43, 29 high) is a car's size. The boss in the Dark Tooth stage keeps his size
 * (only the first BB.DPC of a level load, the human's, is changed).
 */
#include "qol_cars.h"

#include <string.h>

#include "host/qol/qol.h"
#include "rt.h"
#include "shell/sh_names.h"

#define GS_TYPE 0x00624ea8u        /* game type */
#define GS_HUMANS 0x00624ea9u      /* humans */
#define GS_CARS 0x00624eadu        /* the humans' cars */
#define UNLOCK_SWEET 0x009f6c1cu
#define UNLOCK_MINION 0x009f6c1du
#define LOAD_NAME 0x009f6920u      /* the file name sh5_level_load just loaded */
#define ROOT_SHIFT 0x004e8448u     /* the DPC walk's starting scale shift */
#define CAR_SWEET 12
#define CAR_DARK 14
#define SHRINK 2                    /* a quarter: shift + 2 */

static int one_player(void)
{
    uint8_t t = rt_r8(GS_TYPE);
    return t == 1 || t == 3;
}

static int dark(void) { return qol_play_dark_tooth() && one_player(); }

void qolc_car_select_enter(void)
{
    if (!qol_unlock_hidden_cars()) return;
    rt_w8(UNLOCK_SWEET, 1);
    rt_w8(UNLOCK_MINION, 1);
}

void qolc_car_select_leave(void)
{
    if (dark() && rt_r8(GS_CARS) == CAR_SWEET) rt_w8(GS_CARS, CAR_DARK);
}

int qolc_car_unlocked(int32_t car, int original)
{
    return car == CAR_SWEET && dark() ? 1 : original;
}

uint32_t qolc_car_name_text(int32_t car, uint32_t original)
{
    if (car != CAR_SWEET || !dark()) return original;
    return sh_car_display_name(CAR_DARK);
}

void qolc_model_loaded(uint32_t buf)
{
    static int done;     /* once per level load: the human's cars are loaded before the opponents' */
    static uint32_t nodes[8];
    const char* name = (const char*)rt_ptr(LOAD_NAME);
    if (!strstr(name, "CARSDB")) { done = 0; return; }
    if (done || !dark() || rt_r8(GS_CARS) != CAR_DARK) return;
    size_t len = strlen(name);
    if (len < 6 || strcmp(name + len - 6, "BB.DPC") != 0) return;
    done = 1;
    uint32_t list = (uint32_t)((int32_t)(rt_r32(buf + 0x14) - rt_r32(buf + 0xc)) / 4 * 4) + buf;
    uint32_t count = rt_r32(list);
    uint16_t shift = (uint16_t)(rt_r32(ROOT_SHIFT) + SHRINK);
    for (uint32_t i = 0; i < count && i < 8; i++) {
        uint32_t root = rt_r32(list + 4 + 4 * i);
        if (!root) break;
        /* each root is a type 1 group (the object the car entity draws from, worldLoadObject
         * 0x0044f20c): its children go under one type 12 scale node (new shift u16 +4, child
         * count u16 +6, children +8), the group's only child */
        if (rt_r8(root) != 1) continue;
        uint32_t kids = rt_r8(root + 0x14);
        if (kids == 0 || kids > 14) continue;
        if (!nodes[i]) nodes[i] = rt_heap_grow(64);
        if (!nodes[i]) return;
        uint32_t nd = nodes[i];
        rt_w32(nd, 0x0000fe0cu);
        rt_w16(nd + 4, shift);
        rt_w16(nd + 6, (uint16_t)kids);
        for (uint32_t k = 0; k < kids; k++) rt_w32(nd + 8 + 4 * k, rt_r32(root + 0x18 + 4 * k));
        rt_w8(root + 0x14, 1);
        rt_w32(root + 0x18, nd);
    }
}

void qolc_stats_done(uint32_t stats, uint16_t car_id)
{
    if (car_id != 0xf0 || !dark() || rt_r8(GS_CARS) != CAR_DARK) return;
    uint32_t z = 0;
    if (stats != rt_call(0x0045afbcu, &z, 1) + 0x138u) return;   /* the human's car only */
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
    if (!dark() || rt_r16(car + 0x90) != 0xf0) return;
    uint32_t z = 0;
    if (car != rt_call(0x0045afbcu, &z, 1)) return;
    if (rt_r32(car + 0x594) == 0xa0) rt_w32(car + 0x594, 0xa0 >> SHRINK);
}

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
