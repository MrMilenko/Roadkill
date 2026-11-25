/*
 * carsys_misc.c: small accessors and one-liners around the car systems (see carsys_misc.h).
 */
#include "carsys_misc.h"

#include <string.h>

int32_t carsys_event_byte(const CarsysMisc* m) { return *m->event_byte; }

uint8_t* carsys_flame_marker(const CarsysMisc* m, int32_t i) { return m->flame_markers + (intptr_t)i * 20; }

void carsys_bullets_update(const CarsysMisc* m) {
    *m->bullets_parity = (int32_t)(1u - (uint32_t)*m->bullets_parity);
    m->bullets_move(m->user);
    *m->player_one_object = m->human_object(m->user, 0);
}

void carsys_body_spin_clear(uint8_t* body) { memset(body + 0x4C, 0, 4); }

void carsys_push_clear(uint8_t* c) {
    c[3] = 0;
    c[4] = 0;
    c[6] = c[7] = 0;
    c[2] = 0;
}

void carsys_eiffel_fallen_surfaces(const CarsysMisc* m) {
    m->section_last[13] = 66;
    m->section_first[15] = 54;
    m->section_last[15] = 66;
    m->section_first[13] = 54;
}

int32_t carsys_track_surface_index(const CarsysMisc* m, int32_t i) {
    return (int32_t)((uint32_t)*m->surface_count - 1u - (uint32_t)i);
}

void carsys_fx_451a5c(const CarsysMisc* m, int32_t a, int32_t b, int32_t c, int32_t d) {
    m->fx_args[0] = b;
    m->fx_args[1] = c;
    m->fx_451f54(m->user, a, d);
}

void carsys_body_object_register(const CarsysMisc* m, int32_t id, uint8_t* obj) {
    if (obj[0] != 5) return;
    int32_t k = (id >> 4) - 1;
    if (k < 15 && k >= 0) m->body_objects[k] = obj;
}

int32_t carsys_slam_arm_angle(const CarsysMisc* m) { return *m->slam_arm; }
int32_t carsys_slam_claw_angle(const CarsysMisc* m) { return *m->slam_claw; }

void carsys_world_teardown(const CarsysMisc* m) {
    m->world_reset(m->user);
    m->world_flags_i32[0] = 1;
    m->world_flags_u8[0] = 1;
    m->world_flags_i32[1] = 0;
    m->world_flags_u8[1] = 0;
}

int32_t carsys_game_mode(const CarsysMisc* m) { return *m->game_mode; }

void carsys_scene_choice_set(uint8_t* const* p, uint8_t value) {
    uint8_t* q = p[1];
    if (q && q[0] == 9) q[7] = value;
}

uint8_t carsys_dark_tooth_music(const CarsysMisc* m) { return *m->dark_tooth_music; }
void carsys_set_shared_loop(const CarsysMisc* m, uint8_t v) { *m->shared_loop = v; }

void carsys_freeze_reset_all(const CarsysMisc* m) {
    for (int i = 0; i < 16; i++) {
        m->anim_step(m->user, m->freeze_objects + i * 0x108, 0);
        m->anim_step(m->user, m->turbo_objects[i], 0);
    }
}

void carsys_freeze_set_state(const CarsysMisc* m, int32_t id, uint32_t step) {
    int32_t k = id >> 4;
    if (k <= 15 && k >= 0) m->anim_step(m->user, m->freeze_objects + k * 0x108, step);
}

uint8_t* carsys_target_actors(const CarsysMisc* m) { return m->target_actors; }

uint8_t carsys_session_mode(void) { return 0; }

int32_t carsys_obj_finish_if_8(const CarsysMisc* m, uint8_t* obj) {
    if (obj[0x37] != 8) return 1;
    m->obj_finish(m->user, obj);
    return 0;
}
