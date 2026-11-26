/*
 * announce_proto.h: the wire format between Roadkill and its announce / rendezvous server
 * (server/announce/announce.c). One UDP port; every datagram starts with a Header. Little endian,
 * packed, sizes fixed: a datagram whose size isn't exactly what its type says is dropped.
 *
 *   host   -> server  REGISTER   every few seconds while a session is open (from the host's game
 *                                socket, so the address the server sees is the one peers can reach)
 *   host   -> server  WITHDRAW   the session closed
 *   server -> host    REGISTERED the host's public address as the server sees it
 *   client -> server  LIST       a page of the sessions (page = Header.arg)
 *   server -> client  SESSIONS   up to AN_PAGE sessions, Header.arg = the total
 *   client -> server  JOIN       I want session guid: introduce us
 *   server -> both    INTRO      the other side's public and local addresses: both send to both at
 *                                once (UDP hole punching); the game's own join follows peer to peer
 *
 * Nothing here is trusted by the game: the provider turns a Session into the DirectPlay session
 * description the join screen shows. Passwords never go through the server (only "required").
 */
#ifndef ROADKILL_ANNOUNCE_PROTO_H
#define ROADKILL_ANNOUNCE_PROTO_H

#include <stdint.h>

#define AN_MAGIC 0x4e414b52u   /* "RKAN" */
#define AN_VERSION 1
#define AN_PORT 27400
#define AN_PAGE 4              /* sessions per SESSIONS datagram */
#define AN_NAME 32             /* session name, NUL padded */
#define AN_PLAYER_NAME 16
#define AN_MAX_PLAYERS 8

enum { AN_REGISTER = 1, AN_WITHDRAW, AN_REGISTERED, AN_LIST, AN_SESSIONS, AN_JOIN, AN_INTRO };

#pragma pack(push, 1)
typedef struct {
    uint32_t magic, version;
    uint32_t type;
    uint32_t arg;
} AnHeader;

typedef struct {
    uint32_t ip;     /* IPv4, network order */
    uint16_t port;   /* network order */
    uint16_t pad;
} AnAddr;

/* what the game's join screen needs (docs/NETWORK.md, "The announce / rendezvous server") */
typedef struct {
    uint8_t guid[16];                 /* the session's guidInstance */
    char name[AN_NAME];
    uint32_t flags;                   /* DPSESSION_* as the host set them */
    uint32_t max_players, cur_players;
    uint32_t user[4];                 /* dwUser1..4: mode, arena, 3, the host's clock */
    uint32_t password;                /* 1: the host asks a password at join */
    uint32_t build;                   /* the Roadkill protocol / gameplay signature */
    char players[AN_MAX_PLAYERS][AN_PLAYER_NAME];
    AnAddr local;                     /* the host's own LAN address (for players behind the same NAT) */
    AnAddr public_addr;               /* set by the server in SESSIONS */
} AnSession;

typedef struct { AnHeader h; AnSession s; } AnRegister;
typedef struct { AnHeader h; uint8_t guid[16]; } AnWithdraw;
typedef struct { AnHeader h; AnAddr public_addr; } AnRegistered;
typedef struct { AnHeader h; uint32_t build; } AnList;
typedef struct { AnHeader h; uint32_t count; AnSession s[AN_PAGE]; } AnSessions;
typedef struct { AnHeader h; uint8_t guid[16]; AnAddr local; } AnJoin;
typedef struct { AnHeader h; uint8_t guid[16]; AnAddr peer_public, peer_local; } AnIntro;
#pragma pack(pop)

#endif
