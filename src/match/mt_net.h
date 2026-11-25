/*
 * mt_net.h: network packet sends and the start of match handshake, ported from TM2.EXE
 * 0x004632f4, 0x00463d9c, 0x00463e38 (DirectPlay underneath: the interface's Send method,
 * vtable +0x68, retried while it returns 0x8877010e).
 *
 * Packet buffer 0x0050f150 (0x410 bytes): +0 u8 type (0x11 or 0x22), +1 u8, +2 u16 sequence
 * high part, +8 u32 sequence; type 0x22 carries its data from +0xc. Sequence counters: type 0x11
 * u32 0x004e7ad8 with u16 0x0050f560 bumped when it wraps; type 0x22 u32 0x004e7ad0 with u16
 * 0x0050f550. Session (handle *0x0050f9dc): +0x30 u32 our player id.
 * Handshake globals: 0x0050f5fc i32[] player ready, 0x0050f64c i32[8] per player (32 bytes
 * each), 0x0050f8d4 u32 last send time, 0x0050f8d8 u32 now.
 *
 * Verified against the original under emulation (tools/emu/check_match_net.sh).
 */
#ifndef MT_NET_H
#define MT_NET_H

#include <stdint.h>

typedef struct {
    uint8_t* packet;            /* 0x0050f150 */
    uint32_t* seq11;            /* 0x004e7ad8 */
    uint16_t* seq11_hi;         /* 0x0050f560 */
    uint32_t* seq22;            /* 0x004e7ad0 */
    uint16_t* seq22_hi;         /* 0x0050f550 */
    const uint32_t* session;    /* 0x0050f9dc */
    int32_t* ready;             /* 0x0050f5fc */
    int32_t* acks;              /* 0x0050f64c */
    uint32_t* last_time;        /* 0x0050f8d4 */
    uint32_t* now;              /* 0x0050f8d8 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    uint32_t net;               /* 0x0050f9a8 */
    uint32_t (*net_18)(void* user, uint32_t net);          /* 0x004aaa58 */
    uint32_t (*net_0c)(void* user, uint32_t net);          /* 0x004aaa5c */
    uint32_t (*net_iface)(void* user, uint32_t net);       /* 0x004aa68c: the DirectPlay interface */
    /* iface vtable +0x68: Send(this, from, to, flags, data, size) */
    uint32_t (*send)(void* user, uint32_t fn, uint32_t iface, uint32_t from, uint32_t to, uint32_t flags,
                     uint32_t data, uint32_t size);
    int32_t (*session_count)(void* user, uint32_t s);      /* 0x00498968 */
    void (*input_poll)(void* user, uint32_t v);            /* 0x00493748 */
    void (*net_pump)(void* user);                          /* 0x00463f78 */
    uint32_t (*time_now)(void* user);                      /* the function pointer at 0x00c90640 */
    void* user;
} MtNet;

/* 0x00463e38 (type, data, size): size 0..0x400; type 0x11: data to +0, flags 1; type 0x22: data
 * to +0xc, size + 0xc, flags 0; other types: nothing. Sent to everyone (to 0) from the session
 * player. data is a host pointer. */
void mt_net_send(const MtNet* n, uint32_t type, const uint8_t* data, int32_t size);
/* 0x00463d9c (to, b, data, size): a type 0x11 packet with +1 = b to player to (flags 1). */
void mt_net_send_to(const MtNet* n, uint32_t to, uint8_t b, const uint8_t* data, uint32_t size);
/* 0x004632f4 (player i): not networked: 1. Else input poll and network pump; host (net_0c): i's
 * ack, i ready when the acks equal the session count, ready: three 0x20 byte type 0x11 packets
 * {+1 = host, +0x10 = i}, 1; not ready: 0 (time kept, 300 ms). Client: ready: 1; else every
 * 300 ms one packet, 0. ORIGINAL BUG (not reproduced): the packet's other bytes are uninitialized
 * stack; the port sends zeros there. */
int32_t mt_net_wait_ready(const MtNet* n, int32_t i);

/* 0x00463f78 (old name aiCarUpdate; the PS1 pair aiCarUpdate is wrong): the network receive
 * pump: a receive buffer object on the stack (0x004aab90 / 0x004aabc4), then DirectPlay
 * Receive (vtable +0x64)(iface, &from, &to, 1, 0x0050f150, &size = 0x400) until it fails
 * (0x887700be no more messages: quietly, else 0x004aac00 on the buffer object). Messages from
 * 0 are system messages (0x00464088); type 0x11 goes to 0x0046441c(from, to), type 0x22 to
 * 0x00498660(session, from) and, when that is > 0, 0x00473714(0x0050f15c, size, result).
 * Pointers to the stack locals are host pointers (buffer object 0x84 bytes, then size, from, to). */
typedef uint32_t (*MtNetFn)(void* user, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx);
typedef struct {
    const MtNet* net;
    MtNetFn buf_init;           /* 0x004aab90 (buffer object) */
    MtNetFn buf_error;          /* 0x004aac00 (buffer object) */
    MtNetFn buf_done;           /* 0x004aabc4 (buffer object, 0) */
    /* iface vtable +0x64: Receive(this, &from, &to, flags, data, &size) */
    uint32_t (*receive)(void* user, uint32_t fn, uint32_t iface, uint32_t* from, uint32_t* to, uint32_t flags,
                        uint32_t data, uint32_t* size);
    MtNetFn system_msg;         /* 0x00464088 (0x0050f150) */
    MtNetFn game_msg;           /* 0x0046441c (from, to) */
    MtNetFn player_msg;         /* 0x00498660 (session, from) */
    MtNetFn player_data;        /* 0x00473714 (0x0050f15c, size, result) */
} MtNetPump;
void mt_net_pump(const MtNetPump* p);

/* 0x0046500c humanCarsRestart (PS1 pair humanCarsRestart): network restart handshake. While the
 * server id (0x004e7ae0) is -1: wait a second pumping the network (timer 0x00493038 in
 * microseconds, pump 0x00463f78, input 0x00493748), then send a 0x20 byte type 0x11 packet
 * {+1 = 0x18}. Then for every session player, in order: wait a second; once its flag
 * (0x0050f8e8[i]) is set, reset its car (0x0045ba64) and go to the next player; send the next
 * (or same) player a packet {+1 = 0x11, +0x10 = its id (player record +0), +0x14..+0x1f = 0}
 * through mt_net_send_to (to = the server id). ORIGINAL BUG (not reproduced): the other packet
 * bytes are uninitialized stack; the port sends zeros. ORIGINAL BUG (kept): each one second wait
 * compares unsigned microsecond times, so when start + 1000000 falls just below the 32 bit wrap
 * the timer can step over that window and the wait goes on for another whole timer cycle. */
typedef struct {
    const MtNet* net;
    const int32_t* server;      /* 0x004e7ae0 */
    const uint8_t* got;         /* 0x0050f8e8 */
    uint32_t (*timer)(void* user);           /* 0x00493038 */
    void (*pump)(void* user);                /* 0x00463f78 (mt_net_pump) */
    void (*input_poll)(void* user);          /* 0x00493748 (takes no argument) */
    void (*car_reset)(void* user, int32_t i); /* 0x0045ba64 playerCarReset */
} MtNetRestart;
void mt_net_cars_restart(const MtNetRestart* r);

#endif /* MT_NET_H */
