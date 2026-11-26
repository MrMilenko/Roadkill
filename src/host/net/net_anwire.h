/*
 * net_anwire.h: the announce server's datagrams (server/announce/announce_proto.h) on the game's
 * side, as pure encoders and checked decoders (port code, not from the original; no sockets, so
 * tools/test_net.c runs them on their own). The server is not trusted: a datagram must have the
 * exact size of its type, the magic and version, counts within AN_PAGE, every string NUL
 * terminated inside its field, player counts within 8, a build that is ours, and real addresses;
 * names are cut to printable ASCII. A session that breaks a rule is left out of the list.
 *
 * Byte order: the protocol's integers are little endian (read as the host's: every Roadkill
 * target is little endian), its addresses network order.
 */
#ifndef ROADKILL_NET_ANWIRE_H
#define ROADKILL_NET_ANWIRE_H

#include <stdint.h>

#include "../../../server/announce/announce_proto.h"
#include "host/net/net_sock.h"
#include "host/net/net_wire.h"

typedef struct NaSession {
    NwSessionInfo s;         /* as the join screen gets it (players' DPIDs are 1..n) */
    NetAddr pub, local;      /* local.ip == 0: none */
} NaSession;

int na_register_write(uint8_t* p, int cap, const NwSessionInfo* si, uint32_t build, const NetAddr* local);
int na_withdraw_write(uint8_t* p, int cap, const uint8_t guid[16]);
int na_list_write(uint8_t* p, int cap, uint32_t page, uint32_t build);
int na_join_write(uint8_t* p, int cap, const uint8_t guid[16], const NetAddr* local);

/* the header: 0 and the type / arg when magic and version are right */
int na_header_read(const uint8_t* p, int len, uint32_t* type, uint32_t* arg);
/* SESSIONS: -1 malformed; else 0, *total (the server's count), the valid sessions in out[0 .. *n) */
int na_sessions_read(const uint8_t* p, int len, uint32_t build, uint32_t* total, NaSession out[AN_PAGE], int* n);
/* INTRO: arg 0 to the host (peer = the joiner), 1 to the joiner (peer = the host) */
int na_intro_read(const uint8_t* p, int len, uint32_t* arg, uint8_t guid[16], NetAddr* pub, NetAddr* local);
int na_registered_read(const uint8_t* p, int len, NetAddr* pub);

/* an address a packet may be sent to: not 0, broadcast, multicast or 0.x.x.x, a port */
int na_addr_usable(const NetAddr* a);

#endif
