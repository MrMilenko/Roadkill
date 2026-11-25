/*
 * car_stats.c: car stats setup, ported from TM2.EXE 0x0043846c (see car_stats.h).
 */
#include "car_stats.h"

#include "car.h"

static const CarStatsEntry* s_table;
static int s_count;

void car_stats_set_table(const CarStatsEntry* entries, int count) { s_table = entries; s_count = count; }

static int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static void w16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }

/* 0x00439fa8: generic stats for ids without their own table */
void car_stats_generic(uint8_t* p) {
    static const struct { uint8_t off; uint16_t v; } w[] = {
        { 0x5c, 3 }, { 0x52, 0x77 }, { 0x56, 0x4b }, { 0x5e, 2 }, { 0x6a, 0x19 }, { 0x6c, 7 }, { 0x42, 0x10 },
        { 0x44, 0x13 }, { 0x46, 0x20 }, { 0x48, 0x34 }, { 0x4c, 0x50 }, { 0x4e, 0x34 }, { 0x50, 0xffc0 },
        { 0xd4, 0xffe8 }, { 0xd6, 0x28 }, { 0xd8, 9 }, { 0xdc, 0 }, { 0xde, 0xfff2 }, { 0xe0, 0x1f },
        { 0x3a, 100 }, { 0x7a, 0x1c }, { 0x24, 0x19 }, { 0x2a, 600 } };
    for (unsigned i = 0; i < sizeof w / sizeof w[0]; i++) w16(p, w[i].off, w[i].v);
    p[0xe] = 0;
    static const struct { uint8_t off; uint16_t v; } w2[] = {
        { 0x7c, 0x4b }, { 0x7e, 0x1e }, { 0x80, 0xf }, { 0x82, 0x46 }, { 0x8e, 0x50 }, { 0x86, 600 },
        { 0x88, 600 }, { 0x8a, 600 }, { 0x90, 0x32 } };
    for (unsigned i = 0; i < sizeof w2 / sizeof w2[0]; i++) w16(p, w2[i].off, w2[i].v);
    p[0xa8] = 6; p[0xa9] = 0; p[0xaa] = 0; p[0xab] = 0;
    w16(p, 0x3e, i16(p, 0x48));
}

void car_stats_init(uint8_t* p, int16_t slot, uint16_t id, int multiplayer, CarStatsRotateFn rotate, void* user) {
    static const uint8_t defaults[] = { 0x6e, 3, 0x6f, 0, 0x98, 0, 0x99, 0, 0x9a, 0, 0x9b, 0, 0x1a, 0, 0x1b, 0,
                                        0x22, 0, 0x23, 0, 0x16, 0, 0x17, 0, 0x28, 0, 0x29, 0, 0x3c, 0, 0x3d, 0,
                                        3, 0, 5, 4, 0x34, 8, 0x35, 0, 7, 0xff, 8, 0xff, 0x92, 0, 0x93, 0,
                                        0x68, 0x19, 0x69, 0 };
    for (unsigned i = 0; i < sizeof defaults; i += 2) p[defaults[i]] = defaults[i + 1];
    if (!p[0]) {
        p[0xa4] = 1; p[0xa5] = 0; p[0xa6] = 0; p[0xa7] = 0;
    } else if (multiplayer && slot > 0) {
        p[0xa4] = 0; p[0xa5] = 0; p[0xa6] = 0; p[0xa7] = 0;
    }
    p[2] = 0;

    const CarStatsEntry* e = 0;
    for (int i = 0; i < s_count; i++)
        if (s_table[i].id == id) e = &s_table[i];
    if (!e) {
        car_stats_generic(p);
    } else {
        for (int i = 0; i < e->count; i++) p[e->bytes[2 * i]] = e->bytes[2 * i + 1];
        if (id == 0x10) w16(p, 0x3e, i16(p, 0x48) * 2 / 3);
        else if (id == 0x50) w16(p, 0x3e, i16(p, 0x48) * 3);
        else w16(p, 0x3e, i16(p, 0x48));
        if (id == 0xc0 && rotate) {
            rotate(user, 0, 0x2e3);
            rotate(user, 1, (int32_t)0xffffff8f);
        }
    }

    int16_t a = i16(p, 0x4e), b = i16(p, 0x50);
    w16(p, 0x52, i16(p, 0x52));
    w16(p, 0x4a, (uint16_t)((a < 0 ? -a : a) + (b < 0 ? -b : b)));
    w16(p, 0x54, i16(p, 0x52) * 3 / 4);
    w16(p, 0x5a, i16(p, 0x52) * 2 / 3);
    car_stats_derive(p);
    p[0x14] = 0x3c; p[0x15] = 0;
    w16(p, 0x8c, i16(p, 0x3a) * i16(p, 0x8e) / 100);
    w16(p, 0x84, i16(p, 0x7c));
}
