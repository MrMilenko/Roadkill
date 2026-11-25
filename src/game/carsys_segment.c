/*
 * carsys_segment.c: an AI car's speed and aim for its route segment (see carsys_segment.h).
 */
#include "carsys_segment.h"

#include <string.h>

#include "../math/tm2math.h"

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static int32_t wabs(int32_t v) { return v < 0 ? (int32_t)(0u - (uint32_t)v) : v; }

int32_t carsys_ai_speed_percent(const CarRecords* r) {
    uint32_t d = (uint32_t)*r->difficulty;
    return d == 1 ? 0x78 : d == 2 ? 0x8C : 0x4B;
}

void carsys_ai_segment_speed(uint8_t* a, uint8_t active, const CarsysSegmentEnv* env) {
    int32_t base = rd16(a + 0x516);
    if (!active) {
        wr16(a + 0x478, (int16_t)(base * 2));
        wr16(a + 0x544, rd16(a + 0x53E));
    } else {
        if (a[0x446]) {
            int32_t top = env->top_speed(env->user, rd16(a + 0x30));
            if (rd16(a + 0x470) < 1) {
                wr16(a + 0x478, (int16_t)base);
                wr16(a + 0x544, (int16_t)(top / 3));
            } else {
                wr16(a + 0x478, (int16_t)(base * 2));
                wr16(a + 0x544, (int16_t)(tm2_wmul(top, 2) / 3));
            }
        } else {
            int32_t turn = wabs(rd16(a + 0x480));
            if (turn < 0x1C7) {
                wr16(a + 0x478, (int16_t)(base / 2));
                wr16(a + 0x544, rd16(a + 0x53E));
            } else if (turn < 0x505) {
                wr16(a + 0x544, (int16_t)env->top_speed(env->user, rd16(a + 0x30)));
                int16_t rate = rd16(a + 0x544);
                if (rd16(a + 0x470) < 0) {
                    wr16(a + 0x478, (int16_t)(base / 2));
                    wr16(a + 0x544, (int16_t)(rate / 4));
                } else {
                    int32_t lane = rd16(a + 0x46E);
                    if (lane == 0) {
                        wr16(a + 0x478, (int16_t)(base / 2));
                        wr16(a + 0x544, (int16_t)(rate / 3));
                    } else if (lane == 1) {
                        wr16(a + 0x478, (int16_t)(base * 2 / 3));
                        wr16(a + 0x544, (int16_t)(rate / 2));
                    } else {
                        wr16(a + 0x478, (int16_t)base);
                        wr16(a + 0x544, (int16_t)(rate * 3 / 4));
                    }
                }
            } else {
                int32_t top = env->top_speed(env->user, rd16(a + 0x30));
                wr16(a + 0x544, (int16_t)(top / 3));
                wr16(a + 0x478, (int16_t)(base * 3 / 2));
            }
        }
        if (rd16(a + 0x478) < 0xA0) wr16(a + 0x478, 0xA0);
        int32_t pct = carsys_ai_speed_percent(env->rec);
        if (pct == 0x4B) wr16(a + 0x478, (int16_t)(rd16(a + 0x478) * 2 / 3));
        else if (pct == 0x8C) wr16(a + 0x478, (int16_t)(rd16(a + 0x478) * 6 / 5));
    }
    int32_t q = 2000 / rd16(a + 0x562);
    if (rd16(a + 0x544) < q) wr16(a + 0x544, (int16_t)q);
    int32_t v = tm2_wmul(rd16(a + 0x544) * 19, tm2_frame_ticks60()) / 100;
    wr32(a + 0x5AC, tm2_wmul(rd16(a + 0x562), tm2_wmul(v, 32)) / 100);
    const uint8_t* cur = env->nodes + rd16(a + 0x458) * 24;
    const uint8_t* nxt = env->nodes + rd16(a + 0x45A) * 24;
    int32_t dx = (int32_t)((uint32_t)rd32(nxt) - (uint32_t)rd32(cur));
    int32_t dy = (int32_t)((uint32_t)rd32(nxt + 4) - (uint32_t)rd32(cur + 4));
    wr32(a + 0x494, dx);
    wr32(a + 0x498, dy);
    a[0x442] = wabs(dx) < 0x28;
    a[0x443] = wabs(dy) < 0x28;
    a[0x441] = !a[0x442] && !a[0x443];
    if (a[0x441]) {
        wr32(a + 0x4AC, (int32_t)((uint32_t)rd32(cur) + (uint32_t)(dx / 2)));
        wr32(a + 0x4B0, (int32_t)((uint32_t)rd32(cur + 4) + (uint32_t)(dy / 2)));
    } else if (a[0x442]) {
        wr32(a + 0x4AC, rd32(cur));
    } else {
        wr32(a + 0x4B0, rd32(cur + 4));
    }
    wr16(a + 0x47A, rd16(a + 0x478));
}
