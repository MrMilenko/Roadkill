/*
 * cb2_arena_setup.c: arenaSetup, TM2.EXE 0x004576a8 (see cb2_arena_setup.h).
 */
#include "cb2_arena_setup.h"

#define FLOAT_1 0x3f800000u

static uint32_t call(const Cb2ArenaEnv* e, uint32_t addr, int n, uint32_t a0, uint32_t a1, uint32_t a2,
                     uint32_t a3) {
    uint32_t args[4] = { a0, a1, a2, a3 };
    return e->call(e->user, addr, args, n);
}

#define CALL0(e, a) call((e), (a), 0, 0, 0, 0, 0)

/* per event: fog colour, the 0x0050c2c4 byte (-1 = left alone), the arena's own setup calls */
typedef struct {
    uint8_t r, g, b;
    int8_t fog_flag;
    uint32_t fn[4];
} ArenaCase;

/* index = event - 1; 6 and 8 (events 7 and 9) are the default branch */
static const ArenaCase cases[12] = {
    { 0x68, 0x58, 0x48, 0, { 0x0043be70, 0x0043b940, 0x0043b4c0, 0 } },
    { 0x64, 0x64, 0x5d, 0, { 0x0043bffc, 0x0043b9a0, 0x0043b528, 0 } },
    { 0x50, 0x69, 0x6e, 1, { 0x0043c470, 0x0043bac0, 0x0043b660, 0x004125b0 } },
    { 0x3c, 0x2d, 0x14, 1, { 0x0043c2d0, 0x0043ba60, 0x0043b5f8, 0 } },
    { 0x00, 0x00, 0x00, 1, { 0x0043c734, 0x0043bb80, 0x0043b730, 0 } },
    { 0x00, 0x3d, 0x48, 0, { 0x0043c1c4, 0x0043ba00, 0x0043b590, 0x00455014 } },
    { 0, 0, 0, 0, { 0 } },
    { 0x32, 0x3c, 0x41, 1, { 0x0043c634, 0x0043bb20, 0x0043b6c8, 0 } },
    { 0, 0, 0, 0, { 0 } },
    { 0x6a, 0xb4, 0xda, -1, { 0x0043c9dc, 0x0043bc40, 0x0043b800, 0 } },
    { 0x6e, 0x6e, 0x64, -1, { 0x0043cb48, 0x0043bc9c, 0x0043b868, 0 } },
    { 0x00, 0x14, 0x00, -1, { 0x0043ccc0, 0x0043bcf8, 0x0043b8d0, 0 } },
};

static const ArenaCase default_case = { 0x8f, 0x6f, 0x50, 1, { 0x0043c8f8, 0x0043bbe0, 0x0043b798, 0 } };

void cb2_arena_setup(const Cb2ArenaEnv* e, uint8_t event_byte) {
    for (int i = 0; i < 8; i++) { e->clear_b[i] = 0; e->clear_a[i] = 0; }
    CALL0(e, 0x004367f4);                                       /* radar_init */
    for (int i = 0; i < *e->slot_count; i++)                    /* count read again each pass */
        call(e, 0x00470650, 2, e->slot_objs[i], (uint32_t)(int32_t)e->slot_ids[i], 0, 0);
    CALL0(e, 0x0046cb9c);                                       /* rt_screen_buffer, unused */
    {
        /* level_entity_create(0) with six stack arguments (as seen from esp + 4 up) */
        uint32_t args[7] = { 0, 0, 0, FLOAT_1, FLOAT_1, 0x46, 0x23 };
        e->call(e->user, 0x00494610, args, 7);
    }
    uint32_t tex = call(e, 0x0049456c, 1, 0, 0, 0, 0);          /* level_textures_setup(0) */
    if (*e->root_entity == 0) {
        *e->root_entity = call(e, 0x00492d90, 1, tex, 0, 0, 0); /* entity_alloc */
        call(e, 0x00494bfc, 2, *e->root_entity, 0, 0, 0);       /* view_set_target(entity, 0) */
    }
    for (int i = 0; i < 9; i++) e->view_marks[i] = -1;

    int32_t event = (int8_t)event_byte;
    if (event == *e->current_event) {
        CALL0(e, 0x0043bd60);                                   /* startNodesClearUsed */
    } else {
        *e->current_event = event;
        call(e, 0x0046fc90, 1, (uint32_t)event, 0, 0, 0);
        call(e, 0x004952f4, 4, 0, 0xaf, 0xaf, 0xaf);            /* model_set_position */
        static const uint8_t az[3] = { 0x3b, 0x12, 0x15 };
        for (uint32_t j = 0; j < 3; j++) call(e, 0x0049517c, 4, 0, j, 0, az[j]);
        for (uint32_t j = 0; j < 3; j++) {
            uint32_t args[5] = { 0, j, 0x4b, 0x4b, 0x4b };      /* light_set_pos, z on the stack */
            e->call(e->user, 0x0049527c, args, 5);
        }
        uint8_t c = (uint8_t)(event_byte - 1);                  /* dec al; cmp al, 0xb; ja default */
        const ArenaCase* k = c <= 11 && cases[c].fn[0] ? &cases[c] : &default_case;
        if (k->fog_flag >= 0) *e->fog_flag = (uint8_t)k->fog_flag;
        call(e, 0x00494d80, 3, k->r, k->g, k->b, 0);            /* fog_set_color */
        for (int i = 0; i < 4 && k->fn[i]; i++) CALL0(e, k->fn[i]);
        if (k == &default_case) *e->flag_502f7c = 1;
        CALL0(e, 0x004107c0);                                   /* event_objects_find_zones */
        CALL0(e, 0x00411270);                                   /* landmark_b_mark */
        *e->flag_502f7c = 0;
        call(e, 0x00453fac, 1, (uint32_t)event, 0, 0, 0);       /* worldWarpsSetup */
    }
    *e->flag_50c2c0 = 0;
    uint32_t t = CALL0(e, 0x00493038);                          /* timer_get_elapsed */
    call(e, 0x004899e6, 1, t, 0, 0, 0);                         /* crt_srand */
}
