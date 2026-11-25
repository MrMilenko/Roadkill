/*
 * sh_names.c: game state accessors, display names, menu text callbacks (see sh_names.h).
 */
#include "sh_names.h"

uint8_t sh_game_type(const ShEnv* e) { return sh_r8(e, SH_GAME_STATE); }
uint8_t sh_game_event_type(const ShEnv* e) { return sh_r8(e, SH_GAME_STATE + 0x1f); }
uint8_t sh_game_mode(const ShEnv* e, int32_t i) { return sh_r8(e, SH_GAME_STATE + 0x17 + (uint32_t)i); }

/* text addresses in TM2.EXE, by level - 1 (jump tables 0x00474b5c / 0x00474bec) */
static const uint32_t level_long[12] = {0x4e2308, 0x4e2328, 0x4e238c, 0x4e2377, 0x4e23a8, 0x4e233f,
                                        0x4e23c0, 0x4e235a, 0x4e2422, 0x4e23db, 0x4e23f3, 0x4e240a};
static const uint32_t level_short[12] = {0x4e2430, 0x4e243c, 0x4e2461, 0x4e2458, 0x4e2467, 0x4e2443,
                                         0x4e2470, 0x4e244e, 0x4e2491, 0x4e2478, 0x4e247e, 0x4e2487};
static const uint32_t car_name[15] = {0x4e249f, 0x4e24aa, 0x4e24b3, 0x4e24bb, 0x4e24c5, 0x4e24d1, 0x4e24d9, 0x4e24e1,
                                      0x4e24ea, 0x4e24f2, 0x4e24f7, 0x4e2500, 0x4e250e, 0x4e2507, 0x4e251a};

uint32_t sh_level_display_name(uint8_t level) {
    uint8_t k = (uint8_t)(level - 1);
    return k < 12 ? level_long[k] : 0x4e2422;
}

uint32_t sh_level_short_name(uint8_t level) {
    uint8_t k = (uint8_t)(level - 1);
    return k < 12 ? level_short[k] : 0x4e2491;
}

uint32_t sh_car_display_name(uint8_t car) { return car < 15 ? car_name[car] : 0x4e2525; }

uint8_t sh_tournament_opponents(uint8_t level) {
    static const uint8_t n[12] = {7, 6, 7, 7, 7, 8, 10, 9, 0, 0, 0, 0};
    uint8_t k = (uint8_t)(level - 1);
    return k < 12 ? n[k] : 0;
}

int32_t sh_car_unlocked(const ShEnv* e, int32_t car) {
    if (car == 0xc && sh_r8(e, 0x009f6c1cu) == 0) return 0;
    if (car == 0xd && sh_r8(e, 0x009f6c1du) == 0) return 0;
    return 1;
}

uint32_t sh_menu_car_name_text(const ShEnv* e, int32_t car) {
    uint8_t t = sh_game_type(e);
    if ((t == 5 || t == 6 || t == 7) && e->car_taken && e->car_taken(e->user, car) != -1) return 0x4e280c;
    return sh_car_display_name((uint8_t)car);
}

uint32_t sh_menu_random_car_text(void) { return 0x4e2819; }

uint32_t sh_menu_level_name_text(uint8_t level) { return sh_level_display_name((uint8_t)(level + 1)); }

const char* sh_exe_text(const PeImage* pe, uint32_t addr) {
    const uint8_t* p = pe_at(pe, addr, 1);
    if (!p) return 0;
    for (size_t n = 0; pe_at(pe, addr + (uint32_t)n, 1); n++)
        if (p[n] == 0) return (const char*)p;
    return 0;
}
