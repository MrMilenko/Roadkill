/*
 * mt_session.h: multiplayer session helpers and screen state flags in TM2.EXE
 * 0x004618f8..0x00469a74, ported from the PC machine code.
 *
 * The network object (0x0050f9a8, a C++ object of the DirectPlay layer) and the session
 * (pointer at 0x0050f9dc) are outside this code; their methods are callbacks taking the
 * object's handle. Session layout as used here: +0x04 u32[] player records, +0x24 i32 player
 * count. A player record: +0x00 u32 id, +0x14 char[9] name, +0x114 i32 slot, +0x11c i32 name
 * slot, +0x130 i32.
 *
 * Globals (pointers in MtSession):
 *   0x0050f580 i32[15] slot state (1 = has a value), 0x0050f5bc i32[15] slot value
 *   0x0050f5fc i32[] per player, 0x0050f64c i32[8] per player (32 bytes each)
 *   0x0050f8cc i32 current player, 0x0050f8d0 i32 previous, 0x0050f8d4 u32 time of the change
 *   0x0050f8e0 u8[8], 0x0050f8e8 u8[8] flags; 0x0050f8f4 char[12] per name slot
 *   0x00624db8 12 byte records per slot; 0x00624ead u8[] per player
 *   screen flags: 0x0050e5bc / 0x0050e5c0, 0x0050f9e4 / 0x0050f9e8, 0x0050fa0c..0x0050fa18,
 *   0x0050fa68 / 0x0050fa6c, 0x0050faa0 / 0x0050faa4 (i32 each, see the functions)
 *   0x0050e5b8 i32, 0x0050e440 u32 object, 0x0050e100 i32
 *
 * Verified against the original under emulation (tools/emu/check_match_session.sh).
 */
#ifndef MT_SESSION_H
#define MT_SESSION_H

#include <stdint.h>

#include "mt_cars.h"

typedef struct {
    uint8_t* (*resolve)(void* user, uint32_t handle);
    uint32_t net;                   /* 0x0050f9a8 (the object's address / handle) */
    uint32_t* session;              /* 0x0050f9dc */
    uint32_t* obj_9d8;              /* 0x0050f9d8 */
    int32_t* slot_state;            /* 0x0050f580 */
    int32_t* slot_value;            /* 0x0050f5bc */
    int32_t* player_5fc;            /* 0x0050f5fc */
    int32_t* player_64c;            /* 0x0050f64c */
    int32_t* cur_player;            /* 0x0050f8cc */
    int32_t* prev_player;           /* 0x0050f8d0 */
    uint32_t* change_time;          /* 0x0050f8d4 */
    uint8_t* flags_8e0;             /* 0x0050f8e0 (16 bytes) */
    uint8_t* names;                 /* 0x0050f8f4 */
    uint8_t* records;               /* 0x00624db8 */
    uint8_t* player_bytes;          /* 0x00624ead */
    int32_t* f_e5bc;                /* 0x0050e5bc, 0x0050e5c0 */
    int32_t* f_f9e4;                /* 0x0050f9e4, 0x0050f9e8 */
    int32_t* f_fa0c;                /* 0x0050fa0c .. 0x0050fa18 */
    int32_t* f_fa68;                /* 0x0050fa68, 0x0050fa6c */
    int32_t* f_faa0;                /* 0x0050faa0, 0x0050faa4 */
    int32_t* f_e5b8;                /* 0x0050e5b8 */
    uint32_t* obj_e440;             /* 0x0050e440 */
    int32_t* f_e100;                /* 0x0050e100 */
    const MtCars* cars;             /* for 0x0045b978 */
    uint32_t (*net_18)(void* user, uint32_t net);            /* 0x004aaa58: net +0x18 */
    uint32_t (*net_0c)(void* user, uint32_t net);            /* 0x004aaa5c: net +0x0c */
    uint32_t (*net_4aa5b8)(void* user, uint32_t net);        /* 0x004aa5b8 */
    void (*obj_4abd38)(void* user, uint32_t obj);            /* 0x004abd38 */
    int32_t (*session_count)(void* user, uint32_t s);        /* 0x00498968 */
    uint32_t (*session_498634)(void* user, uint32_t s, uint32_t a);  /* 0x00498634 */
    uint32_t (*session_player_id)(void* user, uint32_t s, int32_t i); /* 0x0049864c */
    uint32_t (*session_4986dc)(void* user, uint32_t s, int32_t i);   /* 0x004986dc */
    void (*net_pump)(void* user);                            /* 0x00463f78 */
    uint32_t (*time_now)(void* user);                        /* the function pointer at 0x00c90640 */
    void (*obj_4a6f80)(void* user, uint32_t obj);            /* 0x004a6f80 */
    void (*call_c8)(void* user, uint32_t fn, uint32_t obj, int32_t b); /* [[obj +0x34] +0xc8](obj, 0) */
    void* user;
} MtSession;

/* thunks onto the network object / session */
uint32_t mt_net_is_multiplayer(const MtSession* s);                /* 0x004632b0: net_18 (callers test al) */
int32_t mt_session_count(const MtSession* s);                      /* 0x004632c8 */
uint32_t mt_net_flag(const MtSession* s);                          /* 0x004632d4 */
uint32_t mt_session_call(const MtSession* s, uint32_t a);          /* 0x004632e0 */
uint32_t mt_net_reset(const MtSession* s);                         /* 0x0046329c: 0x004abd38(*0x0050f9d8), 0x004aa5b8(net) */
/* 0x00463268: human car i's player id = session_player_id(i) for every session player */
void mt_session_bind_ids(const MtSession* s);
/* 0x00463bdc: player_bytes[i] = low byte of 0x004986dc(session, i) for every player */
void mt_session_player_bytes(const MtSession* s);
/* 0x00463514: player i becomes current: its 0x5fc / 0x64c entries cleared, 0x64c[0] = 1 when
 * net_0c, change time, previous = current, current = i */
void mt_session_set_current(const MtSession* s, int32_t i);
/* 0x00463c9c: pumps the network, then slot i's value when its state is 1, else -1 */
int32_t mt_session_slot_value(const MtSession* s, int32_t i);
/* 0x00463cc8: when multiplayer: 15 slots state 0 / value -1, every player +0x130 = 0 */
void mt_session_slots_reset(const MtSession* s);
/* 0x00464cdc / 0x00464cf0: player i +0x114 / +0x11c */
int32_t mt_player_slot(const MtSession* s, int32_t i);
int32_t mt_player_name_slot(const MtSession* s, int32_t i);
/* 0x00464d68: players 1.. get slot = their index; a player that had another slot carries its
 * 12 byte record over. Returns 1 (al). */
uint8_t mt_session_renumber(const MtSession* s);
/* 0x00464e84: id (+0) of the player whose slot is slot, else -1 */
int32_t mt_player_id_by_slot(const MtSession* s, int32_t slot);
/* 0x00464ef4: every player's name (up to 9 chars) into its name slot; a name without a zero in
 * 9 chars ends as its first 8 chars + "...". */
void mt_session_copy_names(const MtSession* s);
/* 0x00464dcc / 0x00464ff8: flags 0x0050f8e0 = 1, 0x0050f8e1..e7 = 0 / 0x0050f8e8..ef = 0 */
void mt_flags_8e0_reset(const MtSession* s);
void mt_flags_8e8_clear(const MtSession* s);
/* 0x004618f8: 0x0050e5b8 set: 0x004a6f80(*0x0050e440) if that is not 0; else 0x0050e100 = 1 */
void mt_screen_4618f8(const MtSession* s);
/* screen state flag pairs */
void mt_screen_461f6c(const MtSession* s);   /* 0x0050e5c0 = 1, 0x0050e5bc = 0 */
void mt_screen_461f84(const MtSession* s);   /* both 0 */
void mt_screen_465cbc(const MtSession* s);   /* 0x0050f9e8 = 0, 0x0050f9e4 = 1 */
void mt_screen_4671c8(const MtSession* s);   /* 0x0050fa14 = 0, 0x0050fa18 = 1, 0x0050fa10 = 1 */
void mt_screen_467224(const MtSession* s, int32_t v); /* 0x0050fa0c = v, 0x0050fa10 = 1, 0x0050fa14 = 5 */
void mt_screen_4684c0(const MtSession* s);   /* 0x0050fa6c = 0, 0x0050fa68 = 1 */
void mt_screen_4684d8(const MtSession* s);   /* both 1 */
void mt_screen_469a60(const MtSession* s);   /* 0x0050faa4 = 1, 0x0050faa0 = 1 */
void mt_screen_469a74(const MtSession* s);   /* 0x0050faa4 = 0, 0x0050faa0 = 1 */
int32_t mt_const_2(void);                    /* 0x00462fe0 */
/* 0x00462d80: calls the function pointer at [obj +0x34] +0xc8 with (obj, 0), returns 0 */
int32_t mt_obj_call_c8(const MtSession* s, uint32_t obj);

#endif /* MT_SESSION_H */
