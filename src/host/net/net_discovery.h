/*
 * net_discovery.h: where the join screen's session list comes from (port code, not from the
 * original). A cache of the sessions heard of lately, filled by session sources:
 *
 *   - "lan": an ENUM_REQ broadcast on the local network (and to this machine) every second
 *     while the join screen is open, to the default port and the next few (a second Roadkill on
 *     the same machine takes the next free port); hosts answer with an ENUM_RESP.
 *   - "direct": the same request to [net] join = host:port.
 *   - "announce": Roadkill's announce / rendezvous server ([net] announce, net_announce.c): a
 *     source with its own query and datagram handler, registered with nd_add_source; the
 *     DirectPlay layer only ever reads the cache.
 *
 * EnumSessions answers from the cache at once (it never waits, never fails). An entry not heard
 * of for ND_EXPIRE_MS is dropped. All functions lock the cache's own mutex; sources run on the
 * provider's socket thread and never call game code.
 */
#ifndef ROADKILL_NET_DISCOVERY_H
#define ROADKILL_NET_DISCOVERY_H

#include <stdint.h>

#include "host/net/net_sock.h"
#include "host/net/net_wire.h"

#define ND_MAX 32
#define ND_EXPIRE_MS 4000u
#define ND_QUERY_MS 1000u
#define ND_WANTED_MS 5000u   /* queries run this long after the last EnumSessions */
#define ND_LAN_PORTS 4       /* the default port and the next 3 */

typedef struct NdEntry {
    NwSessionInfo s;
    NetAddr host;            /* where to send the JOIN_REQ (announced: the public address) */
    const char* source;
    uint32_t seen;
    int announced;           /* only heard of through the announce server: a join asks it for an
                              * introduction first and picks the address that answers */
    NetAddr pub, local;      /* announced: the host's public and LAN addresses */
} NdEntry;

/* what a source may use: the provider's socket */
typedef struct NdIo {
    int (*send)(void* user, const NetAddr* to, const uint8_t* pkt, int len);
    void* user;
    uint16_t my_port;        /* this provider's own port (it never queries itself) */
    const uint8_t* app;      /* the application GUID being looked for */
    uint32_t compat;
} NdIo;

typedef struct NdSource {
    const char* name;
    /* send this source's queries (socket thread, once per ND_QUERY_MS while wanted) */
    void (*query)(void* ctx, const NdIo* io, uint32_t now);
    /* a session-less datagram the provider did not handle: 1 when the source took it */
    int (*datagram)(void* ctx, const NdIo* io, const NetAddr* from, const NwHdr* h, const uint8_t* payload, int len,
                    uint32_t now);
    void* ctx;
    /* a datagram that is not Roadkill's provider protocol (another protocol, e.g. the announce
     * server's): 1 when the source took it */
    int (*raw)(void* ctx, const NdIo* io, const NetAddr* from, const uint8_t* pkt, int len, uint32_t now);
} NdSource;

void nd_init(void);                      /* registers lan / direct from the settings */
int nd_add_source(const NdSource* s);    /* 0 added, -1 table full */

/* the DirectPlay side: EnumSessions was called (keeps the queries running) */
void nd_want(uint32_t now);
/* the fresh entries (application GUID app, or all when app is NULL), newest data; returns n */
int nd_list(NdEntry* out, int max, const uint8_t* app, uint32_t now);
/* one entry by guidInstance: 0 found */
int nd_find(const uint8_t guid[16], NdEntry* out, uint32_t now);
/* forget a session (it refused us, or we left it) */
void nd_forget(const uint8_t guid[16]);

/* the socket thread: queries while wanted */
void nd_tick(const NdIo* io, uint32_t now);
/* the socket thread: a session-less datagram (ENUM_RESP, or something for a source) */
int nd_datagram(const NdIo* io, const NetAddr* from, const NwHdr* h, const uint8_t* payload, int len, uint32_t now);

/* the socket thread: a datagram that is not the provider's protocol */
int nd_raw(const NdIo* io, const NetAddr* from, const uint8_t* pkt, int len, uint32_t now);

/* a source reports a session it heard of */
void nd_offer(const NwSessionInfo* s, const NetAddr* host, const char* source, uint32_t now);
/* the announce server reports one: kept as announced unless a direct source heard of it lately
 * (then the LAN path is used and this only keeps it fresh) */
void nd_offer_announced(const NwSessionInfo* s, const NetAddr* pub, const NetAddr* local, const char* source, uint32_t now);

#endif
