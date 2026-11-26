/*
 * net_announce.h: Roadkill's announce / rendezvous server on the game's side (port code, not from
 * the original; the server is server/announce). [net] announce = host:port (default
 * roadkill.horrible.dev:27400, empty: none).
 *
 *   - a session source for the join screen: LIST (page by page) while EnumSessions is being
 *     called; the sessions go into the cache as "announced" (net_discovery.h)
 *   - the host: REGISTER every 5 s (and soon after a change) while a session is open and
 *     [net] public = 1, from the game's own socket (the address the server sees is the one the
 *     players can reach), with the host's LAN address; WITHDRAW at Close
 *   - a join: JOIN asks the server to introduce us; its INTRO goes to the session layer
 *
 * The name is looked up on a thread of its own (never the game's, never the socket thread's).
 * Every datagram from the server is checked (net_anwire.h) and taken only from the server's
 * address. All functions run on the provider's socket thread or under its lock.
 */
#ifndef ROADKILL_NET_ANNOUNCE_H
#define ROADKILL_NET_ANNOUNCE_H

#include <stdint.h>

#include "host/net/net_discovery.h"

/* reads the settings, starts the lookup, registers the source; build = the signature sessions
 * must share */
void na_init(uint32_t build);
/* the server's address once looked up: 0 */
int na_server(NetAddr* out);

/* INTRO from the server: arg 0 = to the host (peer = a joiner), 1 = to a joiner (peer = the host) */
typedef void (*NaIntro)(uint32_t arg, const uint8_t guid[16], const NetAddr* pub, const NetAddr* local, uint32_t now);
void na_set_intro(NaIntro fn);

/* the host: call every tick while hosting (si current); it sends when due */
void na_host_tick(const NdIo* io, const NwSessionInfo* si, uint32_t now);
void na_withdraw(const NdIo* io);
/* a joiner: ask for an introduction to session guid */
void na_join(const NdIo* io, const uint8_t guid[16]);

/* this machine's LAN address with the provider's port (toward the server, else any route) */
int na_local_addr(uint16_t port, NetAddr* out);

#endif
