/*
 * net_wire.h: Roadkill's own network protocol on UDP (port code, not from the original; not
 * Microsoft's DirectPlay wire protocol). Pure encode / decode with strict checks: nothing here
 * touches sockets or game memory, so tools/test_net.c runs it on its own. docs/NETWORK.md
 * ("Status") describes the format.
 *
 * Every datagram, little endian:
 *
 *   +0  u32 magic 'RKNP'       +4 u8 protocol version (1)    +5 u8 packet type
 *   +6  u8  from peer          +7 u8 to peer                 (0 = not in a session)
 *   +8  u64 session token (0 for the session-less packets: ENUM_*, JOIN_*)
 *   +16 u16 seq (REL: this message's sequence, UNREL: the unreliable counter)
 *   +18 u16 ack (every REL below it has arrived)   +20 u32 ack bits (bit i: ack + 1 + i arrived)
 *   +24 the payload
 *
 * A REL packet carries one reliable message (ordered, resent until acked); UNREL one game
 * message that may be lost. The decoders reject anything that does not parse exactly: wrong
 * magic / version / type, a length that does not match, a string with a NUL or longer than the
 * game's 0x80 byte fields, a game message of 0 or more than 0x400 bytes, a DPID of 0 or -1.
 */
#ifndef ROADKILL_NET_WIRE_H
#define ROADKILL_NET_WIRE_H

#include <stdint.h>

#define NW_MAGIC       0x504e4b52u   /* "RKNP" */
#define NW_VERSION     1
#define NW_HDR         24
#define NW_MAX_PACKET  1200          /* no IP fragmentation on any common path */
#define NW_MAX_GAME    0x400         /* the game's receive buffer (src/match/mt_net.c) */
#define NW_MAX_NAME    0x7f          /* the game copies names into 0x80 byte fields */
#define NW_MAX_PDATA   0x200         /* player data (the game's records are 0x138 bytes) */
#define NW_MAX_PEERS   16            /* machines in a session (the game allows 8 players) */
#define NW_MAX_PLAYERS 16
#define NW_DEFAULT_PORT 27420
#define NW_ENUM_REQ_SIZE 320         /* an ENUM_REQ is padded to this: an answer is never much bigger
                                      * than the question (no traffic amplification by spoofed asks) */

enum {
    /* session-less (token 0) */
    NW_ENUM_REQ = 1,     /* who hosts a session of this game?  app GUID, compat */
    NW_ENUM_RESP = 2,    /* a session: NwSessionInfo */
    NW_JOIN_REQ = 3,     /* NwJoinReq */
    NW_JOIN_ACCEPT = 4,  /* NwJoinAccept */
    NW_JOIN_REFUSE = 5,  /* nonce, HRESULT */
    /* in a session (token, from, to checked) */
    NW_REL = 16,         /* one reliable message (NwMsg) */
    NW_UNREL = 17,       /* one game message (NwMsg kind NM_GAME) */
    NW_ACK = 18,         /* the header's ack fields only */
    NW_PING = 19,        /* u32 sender's time */
    NW_PONG = 20,        /* u32 the time of the PING answered */
    NW_BYE = 21,         /* the sender leaves the session */
};

/* reliable messages (first payload byte) */
enum {
    NM_GAME = 1,         /* u32 from DPID, u32 to DPID (0 all), u16 n, n bytes */
    NM_PEER = 2,         /* host -> members: u8 peer, u32 ip, u16 port (a member to link to) */
    NM_PEER_GONE = 3,    /* host -> members: u8 peer left / timed out */
    NM_PLAYER = 4,       /* u32 id, u8 owner, u32 flags, name, name, u16 n, data */
    NM_PLAYER_GONE = 5,  /* u32 id */
    NM_PLAYER_DATA = 6,  /* u32 id, u16 n, data */
    NM_SESSION = 7,      /* host -> members: the session description (NwSessionDesc) */
    NM_READY = 8,        /* host -> joiner: the snapshot is complete */
};

typedef struct NwHdr {
    uint8_t type, from, to;
    uint64_t token;
    uint16_t seq, ack;
    uint32_t ack_bits;
} NwHdr;

/* the part of DPSESSIONDESC2 the session carries */
typedef struct NwSessionDesc {
    uint8_t guid[16], app[16];
    uint32_t flags, max_players, cur_players;
    uint32_t user[4];
    char name[NW_MAX_NAME + 1];
    uint8_t has_password;
} NwSessionDesc;

typedef struct NwPlayerName {
    uint32_t id;
    char sname[NW_MAX_NAME + 1], lname[NW_MAX_NAME + 1];
} NwPlayerName;

/* ENUM_RESP: the session and its players' names (for the join screen) */
typedef struct NwSessionInfo {
    NwSessionDesc d;
    uint32_t compat;
    int nplayers;
    NwPlayerName players[NW_MAX_PLAYERS];
} NwSessionInfo;

typedef struct NwJoinReq {
    uint64_t nonce;
    uint8_t app[16], guid[16];
    uint32_t compat;
    char password[NW_MAX_NAME + 1];
} NwJoinReq;

typedef struct NwJoinAccept {
    uint64_t nonce, token;
    uint8_t you, host;       /* peer numbers */
    uint8_t next_peer;       /* the host's next peer number (a migrated host goes on from it) */
    NwSessionDesc d;
} NwJoinAccept;

typedef struct NwMsg {
    uint8_t kind;
    uint32_t from, to, id, flags;       /* NM_GAME from / to; NM_PLAYER* id, flags */
    uint8_t peer, owner;                /* NM_PEER* peer; NM_PLAYER owner */
    uint32_t ip;
    uint16_t port;
    uint16_t n;                         /* data bytes */
    const uint8_t* data;                /* into the decoded packet */
    char sname[NW_MAX_NAME + 1], lname[NW_MAX_NAME + 1];
    NwSessionDesc d;                    /* NM_SESSION */
} NwMsg;

/* header: write the 24 bytes; read checks magic, version, a known type and len >= NW_HDR */
void nw_hdr_write(uint8_t* p, const NwHdr* h);
int nw_hdr_read(const uint8_t* p, int len, NwHdr* h);   /* 0 ok, -1 rejected */

/* payloads: *_write return the bytes written (0 when it does not fit cap), *_read 0 / -1.
 * Payload pointers start after the header. */
int nw_enum_req_write(uint8_t* p, int cap, const uint8_t app[16], uint32_t compat);
int nw_enum_req_read(const uint8_t* p, int len, uint8_t app[16], uint32_t* compat);
int nw_session_info_write(uint8_t* p, int cap, const NwSessionInfo* s);   /* drops names that do not fit */
int nw_session_info_read(const uint8_t* p, int len, NwSessionInfo* s);
int nw_join_req_write(uint8_t* p, int cap, const NwJoinReq* j);
int nw_join_req_read(const uint8_t* p, int len, NwJoinReq* j);
int nw_join_accept_write(uint8_t* p, int cap, const NwJoinAccept* j);
int nw_join_accept_read(const uint8_t* p, int len, NwJoinAccept* j);
int nw_join_refuse_write(uint8_t* p, int cap, uint64_t nonce, uint32_t hr);
int nw_join_refuse_read(const uint8_t* p, int len, uint64_t* nonce, uint32_t* hr);
int nw_msg_write(uint8_t* p, int cap, const NwMsg* m);
int nw_msg_read(const uint8_t* p, int len, NwMsg* m);
int nw_u32_write(uint8_t* p, int cap, uint32_t v);
int nw_u32_read(const uint8_t* p, int len, uint32_t* v);

/* a valid DPID: not 0 (DPID_SYSMSG / all players) and not -1 (the game's "none") */
static inline int nw_dpid_ok(uint32_t id) { return id != 0 && id != 0xffffffffu; }

#endif
