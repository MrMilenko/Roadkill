/*
 * mt2_net.h: the network message handlers and the join request, ported from the TM2.EXE machine
 * code: 0x0046441c (game messages, type 0x11 packets), 0x00464088 (DirectPlay system messages),
 * 0x004637c4 (a player asks for / is given a session slot). No PS1 counterpart (PC only).
 *
 * Globals (all i32 unless said):
 *   0x0050f150 packet buffer (the pump receives into it): +1 u8 message kind, +8 u32 (system
 *              message: the player id), +0x10 / +0x14 / +0x18 u32 arguments, +0x20 text
 *   0x0050f580 [] slot taken per player, 0x0050f5bc [] slot value per player
 *   0x0050f5fc [20] ready, 0x0050f64c [20][8] acks (32 bytes per player)
 *   0x0050f8cc / 0x0050f8d0 loop index / previous, 0x0050f8d4 u32 last time, 0x0050f8dc flag
 *   0x0050f8e0 u8 [] got (kind 0x10), 0x0050f8e8 u8 [] got (kind 0x12), 0x0050f8f0 u8 flag
 *   0x0050f9a8 the net object (passed by address), 0x0050f9cc DirectPlay interface handle,
 *   0x0050f9dc session handle, 0x0050f9e0 player list handle, 0x00624db8 [][3] stats,
 *   0x004e7ae0 server id.
 * Session (resolve(*session)): +4 u32 [] player record handles, +0x24 i32 count, +0x30 our id,
 *   +0x40 u32, +0x160 i32 join answer. Player record: +0 u32 id, +0x114 i32 key, +0x130 i32 state.
 * Player list node (list_find): +0xc u32 server flag, +0x10 u32, +0x114, +0x134 kept on a copy.
 * Human car (0x0045b988 / 0x0045afbc): +0x1d8 i32, +0x208 u32.
 *
 * Everything outside goes through callbacks (verified ports exist for most of them: table_find_114
 * 0x00498660, table_set_11c 0x004986f8, table_entry_key 0x0049864c, list_find 0x00497c50,
 * table_remove 0x00498820, obj_word_30 0x004988e4 (src/engine/en_misc2.c); net_send / net_send_to
 * / net_receive_pump (src/match/mt_net.c); human_car_by_id (mt_cars.c), human_car_get
 * (src/game/carsys_records.c), human_car_leave (mt_misc.c), screen_4671c8 / net_set_flag /
 * screen_467224 (mt_session.c), rt_get_520e94 (mt_rt.c), gp_hud_msg_post (src/gameplay/gp_misc.c)).
 *
 * Verified against the original under emulation (tools/emu/check_mt2_net.sh).
 */
#ifndef MT2_NET_H
#define MT2_NET_H

#include <stdint.h>

typedef uint32_t (*Mt2NetFn)(void* user, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx);

typedef struct {
    uint8_t* packet;            /* 0x0050f150 */
    int32_t* joined;            /* 0x0050f580 */
    int32_t* slot;              /* 0x0050f5bc */
    int32_t* ready;             /* 0x0050f5fc */
    int32_t* acks;              /* 0x0050f64c */
    int32_t* cur;               /* 0x0050f8cc */
    int32_t* prev;              /* 0x0050f8d0 */
    uint32_t* last_time;        /* 0x0050f8d4 */
    int32_t* flag_8dc;          /* 0x0050f8dc */
    uint8_t* got_8e0;           /* 0x0050f8e0 */
    uint8_t* got_8e8;           /* 0x0050f8e8 */
    const uint8_t* flag_8f0;    /* 0x0050f8f0 */
    const uint32_t* dplay;      /* 0x0050f9cc */
    const uint32_t* session;    /* 0x0050f9dc */
    const uint32_t* players;    /* 0x0050f9e0 */
    int32_t* stats;             /* 0x00624db8 */
    int32_t* server;            /* 0x004e7ae0 */
    /* ORIGINAL BUG: 0x0046441c kind 4 and 0x004637c4 read an uninitialized stack local when no
     * free slot is found (compared with 2; 0x004637c4 also returns it). The port uses this value
     * there; the engine should pass 0 (any value but 2 behaves the same for kind 4). */
    int32_t stale;
    uint8_t* (*resolve)(void* user, uint32_t handle);
    Mt2NetFn net_0c;            /* 0x004aaa5c (net 0x0050f9a8): we host */
    Mt2NetFn net_60;            /* 0x004aaa60 (net) */
    Mt2NetFn time_now;          /* the function pointer at 0x00c90640 (timeGetTime) */
    Mt2NetFn find_114;          /* 0x00498660 (session, key) */
    Mt2NetFn set_11c;           /* 0x004986f8 (session, i, v) */
    Mt2NetFn entry_key;         /* 0x0049864c (session, i) */
    Mt2NetFn remove;            /* 0x00498820 (session, key) */
    Mt2NetFn word_30;           /* 0x004988e4 (session) */
    Mt2NetFn list_find;         /* 0x00497c50 (players, key) -> node */
    Mt2NetFn pl_194;            /* 0x00498194 (players, key, v) */
    Mt2NetFn pl_1e0;            /* 0x004981e0 (players, key, v) */
    Mt2NetFn pl_250;            /* 0x00498250 (players, key, v) */
    Mt2NetFn text_0fc;          /* 0x004670fc (node + 0x14, packet + 0x20 host pointer) */
    Mt2NetFn text_160;          /* 0x00467160 (node + 0x14, packet + 0x20 host pointer) */
    Mt2NetFn scr_1c8;           /* 0x004671c8 () */
    Mt2NetFn set_flag;          /* 0x004671e8 (v) */
    Mt2NetFn scr_224;           /* 0x00467224 (v) */
    Mt2NetFn car_by_id;         /* 0x0045b988 (id) -> car */
    Mt2NetFn car_get;           /* 0x0045afbc (i) -> car (edx 3 is set but not read) */
    Mt2NetFn rt_get;            /* 0x0046ca60 () */
    Mt2NetFn hud_post;          /* 0x00436040 (owner, prio) */
    Mt2NetFn car_leave;         /* 0x0045edd0 (i) */
    Mt2NetFn reg_window;        /* 0x00499884 (0x004e1340 "tm2.ico") */
    /* DirectPlay vtable +0x74, stdcall (this, id, &id (session handle + 0x30), 0x138, 2) */
    uint32_t (*dplay_call)(void* user, uint32_t fn, uint32_t iface, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
    Mt2NetFn send;              /* 0x00463e38 (type, data host pointer, size) */
    Mt2NetFn send_to;           /* 0x00463d9c (to, kind, data host pointer, size) */
    Mt2NetFn pump;              /* 0x00463f78 () */
    Mt2NetFn input_poll;        /* 0x00493748 (eax as the original leaves it) */
    Mt2NetFn input_check;       /* 0x004936ac (0x14, 0) -> al */
    Mt2NetFn timer;             /* 0x00493038 () microseconds */
    Mt2NetFn input_poll0;       /* 0x00493748 as called by 0x00464dec (no argument read) */
    void* user;
} Mt2Net;

/* 0x0046441c (from): one game message from player from; kind = packet +1 (0..0x18, 0xb / 0xc
 * and others: nothing). Packets built here are 0x20 bytes with +1 = kind and +0x10..+0x1f set;
 * the original leaves bytes 0, 2..0xf as stack garbage, the port sends zeros there.
 * DECOMPILER TRAP: kind 4 tests "0x004aaa5c + 0x0050f9a8 != 0" (a function address, always
 * true), not a call; Ghidra drops the test. */
void mt2_net_game_msg(const Mt2Net* n, uint32_t from);

/* 0x00464088 (msg): one DirectPlay system message (msg: host pointer, normally the packet):
 * +0 type: 3 (player created: +4 == 1 registers the window, session +0x40, DirectPlay +0x74;
 * then the 20 player handshake reset), 5 (player gone: id from packet +8 (the global, not msg)),
 * 0x101 (host migration: kind 0x17 sent, DirectPlay +0x74, reset), 0x102 (player data: 0x138
 * bytes from the handle at +0xc into the list node of id +8, keeping its +0x114 / +0x134;
 * +0xc set: server = id). Others nothing.
 * DECOMPILER TRAP: Ghidra shows the DirectPlay +0x74 call without its five stack arguments.
 * The 0x101 packet has only +1 defined (the rest is stack garbage; the port sends zeros). */
void mt2_net_system_msg(const Mt2Net* n, const uint8_t* msg);

/* 0x004637c4 (player p): host: give p a free slot (record +0x114 == 0) and tell everyone (kind 8,
 * +0x10 p, +0x14 our id): 2; slot already taken: 1; none free: the stale local. Client: pump;
 * p has a slot: 0; else ask (kind 4, +0x10 p) and wait for session +0x160, asking again every
 * 300 ms; returns +0x160 (as read after the last pump), or -1 when input 0x14 is pressed. */
int32_t mt2_net_join(const Mt2Net* n, int32_t p);

/* 0x00464dec: ask every other session player (records 1..count-1, in order) for its stats:
 * each round waits one second (timer 0x00493038 in microseconds, pumping the network and
 * polling input), moves on to the next player once its flag 0x0050f8e0[i] is set, and sends the
 * current one a kind 0xf packet (send_to(record +0 id, 0xf, ..)). ORIGINAL BUG (kept): the wait
 * compares unsigned times, so an end time just past the 32 bit wrap is reached early or a whole
 * timer cycle late, as in humanCarsRestart. */
void mt2_net_ask_stats(const Mt2Net* n);

#endif /* MT2_NET_H */
