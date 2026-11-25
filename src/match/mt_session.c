/*
 * mt_session.c: multiplayer session helpers and screen state flags (see mt_session.h).
 */
#include "mt_session.h"

#include <string.h>

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }

static uint8_t* sess(const MtSession* s) { return s->resolve(s->user, *s->session); }
static int32_t sess_count(const MtSession* s) { return rd32(sess(s) + 0x24); }
static uint8_t* player(const MtSession* s, int32_t i) {
    return s->resolve(s->user, (uint32_t)rd32(sess(s) + 4 + 4 * i));
}

uint32_t mt_net_is_multiplayer(const MtSession* s) { return s->net_18(s->user, s->net); }
int32_t mt_session_count(const MtSession* s) { return s->session_count(s->user, *s->session); }
uint32_t mt_net_flag(const MtSession* s) { return s->net_0c(s->user, s->net); }
uint32_t mt_session_call(const MtSession* s, uint32_t a) { return s->session_498634(s->user, *s->session, a); }

uint32_t mt_net_reset(const MtSession* s) {
    s->obj_4abd38(s->user, *s->obj_9d8);
    return s->net_4aa5b8(s->user, s->net);
}

void mt_session_bind_ids(const MtSession* s) {
    int32_t n = s->session_count(s->user, *s->session);
    for (int32_t i = 0; i < n; i++) {
        uint32_t id = s->session_player_id(s->user, *s->session, i);
        mt_human_set_id(s->cars, i, (int32_t)id);
    }
}

void mt_session_player_bytes(const MtSession* s) {
    int32_t i = 0;
    while (i < s->session_count(s->user, *s->session)) {
        uint32_t v = s->session_4986dc(s->user, *s->session, i);
        i++;
        s->player_bytes[i - 1] = (uint8_t)v;
    }
}

void mt_session_set_current(const MtSession* s, int32_t i) {
    s->player_5fc[i] = 0;
    int32_t* t = (int32_t*)((uint8_t*)s->player_64c + (uint32_t)i * 32u);
    for (int k = 0; k < 8; k++) t[k] = 0;
    if (s->net_0c(s->user, s->net)) t[0] = 1;
    *s->change_time = s->time_now(s->user);
    *s->prev_player = *s->cur_player;
    *s->cur_player = i;
}

int32_t mt_session_slot_value(const MtSession* s, int32_t i) {
    s->net_pump(s->user);
    return s->slot_state[i] == 1 ? s->slot_value[i] : -1;
}

void mt_session_slots_reset(const MtSession* s) {
    if (!(uint8_t)s->net_18(s->user, s->net)) return;
    for (int k = 0; k < 15; k++) {
        s->slot_value[k] = -1;
        s->slot_state[k] = 0;
    }
    for (int32_t i = 0; i < sess_count(s); i++) wr32(player(s, i) + 0x130, 0);
}

int32_t mt_player_slot(const MtSession* s, int32_t i) { return rd32(player(s, i) + 0x114); }
int32_t mt_player_name_slot(const MtSession* s, int32_t i) { return rd32(player(s, i) + 0x11c); }

uint8_t mt_session_renumber(const MtSession* s) {
    for (int32_t i = 1; i < sess_count(s); i++) {
        int32_t old = rd32(player(s, i) + 0x114);
        if (old != i) memmove(s->records + 12 * i, s->records + (uint32_t)old * 12u, 12);
        wr32(player(s, i) + 0x114, i);
    }
    return 1;
}

int32_t mt_player_id_by_slot(const MtSession* s, int32_t slot) {
    for (int32_t i = 0; i < sess_count(s); i++) {
        uint8_t* p = player(s, i);
        if (slot == rd32(p + 0x114)) return rd32(p);
    }
    return -1;
}

void mt_session_copy_names(const MtSession* s) {
    for (int32_t j = 0; j < sess_count(s); j++) {
        int32_t k = 0;
        do {
            uint8_t* p = player(s, j);
            uint8_t c = p[0x14 + k];
            s->names[(uint32_t)rd32(p + 0x11c) * 12u + (uint32_t)k] = c;
            if (player(s, j)[0x14 + k] == 0) k = 0x63;
            k++;
        } while (k < 9);
        if (k < 0x63) {
            for (int e = 8; e < 11; e++) s->names[(uint32_t)rd32(player(s, j) + 0x11c) * 12u + e] = '.';
            s->names[(uint32_t)rd32(player(s, j) + 0x11c) * 12u + 11] = 0;
        }
    }
}

void mt_flags_8e0_reset(const MtSession* s) {
    s->flags_8e0[0] = 1;
    for (int k = 1; k < 8; k++) s->flags_8e0[k] = 0;
}
void mt_flags_8e8_clear(const MtSession* s) {
    for (int k = 8; k < 16; k++) s->flags_8e0[k] = 0;
}

void mt_screen_4618f8(const MtSession* s) {
    if (*s->f_e5b8 == 0) {
        *s->f_e100 = 1;
        return;
    }
    if (*s->obj_e440) s->obj_4a6f80(s->user, *s->obj_e440);
}

void mt_screen_461f6c(const MtSession* s) { s->f_e5bc[1] = 1; s->f_e5bc[0] = 0; }
void mt_screen_461f84(const MtSession* s) { s->f_e5bc[1] = 0; s->f_e5bc[0] = 0; }
void mt_screen_465cbc(const MtSession* s) { s->f_f9e4[1] = 0; s->f_f9e4[0] = 1; }
void mt_screen_4671c8(const MtSession* s) { s->f_fa0c[2] = 0; s->f_fa0c[3] = 1; s->f_fa0c[1] = 1; }
void mt_screen_467224(const MtSession* s, int32_t v) { s->f_fa0c[0] = v; s->f_fa0c[1] = 1; s->f_fa0c[2] = 5; }
void mt_screen_4684c0(const MtSession* s) { s->f_fa68[1] = 0; s->f_fa68[0] = 1; }
void mt_screen_4684d8(const MtSession* s) { s->f_fa68[1] = 1; s->f_fa68[0] = 1; }
void mt_screen_469a60(const MtSession* s) { s->f_faa0[1] = 1; s->f_faa0[0] = 1; }
void mt_screen_469a74(const MtSession* s) { s->f_faa0[1] = 0; s->f_faa0[0] = 1; }
int32_t mt_const_2(void) { return 2; }

int32_t mt_obj_call_c8(const MtSession* s, uint32_t obj) {
    uint8_t* o = s->resolve(s->user, obj);
    uint8_t* t = s->resolve(s->user, (uint32_t)rd32(o + 0x34));
    s->call_c8(s->user, (uint32_t)rd32(t + 0xc8), obj, 0);
    return 0;
}
