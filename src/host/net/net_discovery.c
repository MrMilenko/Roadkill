/*
 * net_discovery.c: the session cache and the "lan" / "direct" sources of net_discovery.h (port
 * code, not from the original).
 */
#include "host/net/net_discovery.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "host/net/net_log.h"
#include "host/settings/settings.h"

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static NdEntry cache[ND_MAX];
static int ncache;
static NdSource sources[8];
static int nsources;
static uint32_t wanted_at, queried_at;
static int wanted_ever;

static int tdiff(uint32_t a, uint32_t b) { return (int32_t)(a - b); }

int nd_add_source(const NdSource* s)
{
    pthread_mutex_lock(&mu);
    int r = -1;
    if (nsources < (int)(sizeof sources / sizeof sources[0])) {
        sources[nsources++] = *s;
        r = 0;
    }
    pthread_mutex_unlock(&mu);
    return r;
}

void nd_want(uint32_t now)
{
    pthread_mutex_lock(&mu);
    if (!wanted_ever || tdiff(now, wanted_at) > (int)ND_WANTED_MS) queried_at = now - ND_QUERY_MS;   /* query at once */
    wanted_at = now;
    wanted_ever = 1;
    pthread_mutex_unlock(&mu);
}

static void expire(uint32_t now)
{
    for (int i = 0; i < ncache;) {
        if (tdiff(now, cache[i].seen) > (int)ND_EXPIRE_MS) cache[i] = cache[--ncache];
        else i++;
    }
}

int nd_list(NdEntry* out, int max, const uint8_t* app, uint32_t now)
{
    pthread_mutex_lock(&mu);
    expire(now);
    int n = 0;
    for (int i = 0; i < ncache && n < max; i++)
        if (!app || !memcmp(cache[i].s.d.app, app, 16)) out[n++] = cache[i];
    pthread_mutex_unlock(&mu);
    return n;
}

int nd_find(const uint8_t guid[16], NdEntry* out, uint32_t now)
{
    pthread_mutex_lock(&mu);
    expire(now);
    int r = -1;
    for (int i = 0; i < ncache; i++)
        if (!memcmp(cache[i].s.d.guid, guid, 16)) {
            *out = cache[i];
            r = 0;
            break;
        }
    pthread_mutex_unlock(&mu);
    return r;
}

void nd_forget(const uint8_t guid[16])
{
    pthread_mutex_lock(&mu);
    for (int i = 0; i < ncache; i++)
        if (!memcmp(cache[i].s.d.guid, guid, 16)) { cache[i] = cache[--ncache]; break; }
    pthread_mutex_unlock(&mu);
}

static void offer(const NwSessionInfo* s, const NetAddr* host, const char* source, uint32_t now, int announced,
                  const NetAddr* pub, const NetAddr* local);

void nd_offer(const NwSessionInfo* s, const NetAddr* host, const char* source, uint32_t now)
{
    offer(s, host, source, now, 0, NULL, NULL);
}

void nd_offer_announced(const NwSessionInfo* s, const NetAddr* pub, const NetAddr* local, const char* source, uint32_t now)
{
    offer(s, pub, source, now, 1, pub, local);
}

static void offer(const NwSessionInfo* s, const NetAddr* host, const char* source, uint32_t now, int announced,
                  const NetAddr* pub, const NetAddr* local)
{
    pthread_mutex_lock(&mu);
    int i;
    for (i = 0; i < ncache; i++) if (!memcmp(cache[i].s.d.guid, s->d.guid, 16)) break;
    if (i == ncache) {
        if (ncache == ND_MAX) {   /* full: the oldest goes */
            int old = 0;
            for (int k = 1; k < ncache; k++) if (tdiff(cache[old].seen, cache[k].seen) > 0) old = k;
            i = old;
        } else ncache++;
        char a[32];
        net_log("discovery: session \"%s\" at %s (%s)", s->d.name, net_addr_str(host, a, sizeof a), source);
    } else if (announced && !cache[i].announced) {
        /* heard of directly (LAN / [net] join) lately: that path wins, the server only says it lives */
        cache[i].seen = now;
        pthread_mutex_unlock(&mu);
        return;
    }
    cache[i].s = *s;
    cache[i].host = *host;
    cache[i].source = source;
    cache[i].seen = now;
    cache[i].announced = announced;
    if (announced) {
        cache[i].pub = *pub;
        cache[i].local = *local;
    }
    pthread_mutex_unlock(&mu);
}

void nd_tick(const NdIo* io, uint32_t now)
{
    NdSource src[8];
    int n;
    pthread_mutex_lock(&mu);
    if (!wanted_ever || tdiff(now, wanted_at) > (int)ND_WANTED_MS || tdiff(now, queried_at) < (int)ND_QUERY_MS) {
        pthread_mutex_unlock(&mu);
        return;
    }
    queried_at = now;
    n = nsources;
    memcpy(src, sources, sizeof src);
    pthread_mutex_unlock(&mu);
    for (int i = 0; i < n; i++) if (src[i].query) src[i].query(src[i].ctx, io, now);
}

int nd_datagram(const NdIo* io, const NetAddr* from, const NwHdr* h, const uint8_t* payload, int len, uint32_t now)
{
    NdSource src[8];
    int n;
    pthread_mutex_lock(&mu);
    n = nsources;
    memcpy(src, sources, sizeof src);
    pthread_mutex_unlock(&mu);
    for (int i = 0; i < n; i++)
        if (src[i].datagram && src[i].datagram(src[i].ctx, io, from, h, payload, len, now)) return 1;
    return 0;
}

int nd_raw(const NdIo* io, const NetAddr* from, const uint8_t* pkt, int len, uint32_t now)
{
    NdSource src[8];
    int n;
    pthread_mutex_lock(&mu);
    n = nsources;
    memcpy(src, sources, sizeof src);
    pthread_mutex_unlock(&mu);
    for (int i = 0; i < n; i++)
        if (src[i].raw && src[i].raw(src[i].ctx, io, from, pkt, len, now)) return 1;
    return 0;
}

/* ---- the UDP sources: an ENUM_REQ to a list of addresses, ENUM_RESPs into the cache ---- */

typedef struct UdpSource {
    const char* name;
    int lan;                 /* broadcast + this machine, ND_LAN_PORTS ports */
    char target[256];        /* direct: host:port */
    NetAddr addr;
    int resolved;            /* direct: 1 resolved, -1 failed */
    uint32_t resolved_at;
} UdpSource;

static UdpSource lan_src = {"lan", 1, "", {0, 0}, 0, 0};
static UdpSource direct_src = {"direct", 0, "", {0, 0}, 0, 0};

static void send_req(const NdIo* io, const NetAddr* to)
{
    uint8_t pkt[NW_HDR + NW_ENUM_REQ_SIZE];
    NwHdr h = {NW_ENUM_REQ, 0, 0, 0, 0, 0, 0};
    nw_hdr_write(pkt, &h);
    int n = nw_enum_req_write(pkt + NW_HDR, (int)sizeof pkt - NW_HDR, io->app, io->compat);
    if (n > 0) io->send(io->user, to, pkt, NW_HDR + n);
}

static void udp_query(void* ctx, const NdIo* io, uint32_t now)
{
    UdpSource* s = ctx;
    if (s->lan) {
        uint16_t base = (uint16_t)settings_int("net.port", NW_DEFAULT_PORT);
        for (int k = 0; k < ND_LAN_PORTS; k++) {
            uint16_t port = (uint16_t)(base + k);
            NetAddr b = {NET_ADDR_BROADCAST, port};
            send_req(io, &b);
            if (port != io->my_port) {
                NetAddr lo = {NET_ADDR_LOOPBACK, port};
                send_req(io, &lo);
            }
        }
        return;
    }
    /* direct: resolve on this thread (a name lookup may block), again every 30 s */
    if (!s->resolved || (s->resolved < 0 && tdiff(now, s->resolved_at) > 30000)) {
        s->resolved = net_addr_resolve(s->target, NW_DEFAULT_PORT, &s->addr) == 0 ? 1 : -1;
        s->resolved_at = now;
        char a[32];
        if (s->resolved > 0) net_log("discovery: [net] join %s is %s", s->target, net_addr_str(&s->addr, a, sizeof a));
        else fprintf(stderr, "roadkill net: [net] join: cannot resolve \"%s\"\n", s->target);
    }
    if (s->resolved > 0) send_req(io, &s->addr);
}

static int udp_datagram(void* ctx, const NdIo* io, const NetAddr* from, const NwHdr* h, const uint8_t* payload, int len,
                        uint32_t now)
{
    UdpSource* s = ctx;
    if (h->type != NW_ENUM_RESP) return 0;
    /* both UDP sources share the answer: the lan one takes it (the direct one would only differ
     * by name); a direct answer is told apart by its address */
    if (!s->lan) return 0;
    NwSessionInfo si;
    char a[32];
    if (nw_session_info_read(payload, len, &si) != 0) {
        net_drop("ENUM_RESP from %s: malformed", net_addr_str(from, a, sizeof a));
        return 1;
    }
    if (memcmp(si.d.app, io->app, 16) != 0) return 1;
    if (si.compat != io->compat) {
        net_log("discovery: \"%s\" at %s plays with other gameplay settings (%08x, ours %08x): not listed", si.d.name,
                net_addr_str(from, a, sizeof a), si.compat, io->compat);
        return 1;
    }
    int direct = direct_src.resolved > 0 && net_addr_eq(from, &direct_src.addr);
    nd_offer(&si, from, direct ? direct_src.name : lan_src.name, now);
    return 1;
}

void nd_init(void)
{
    static int done;
    if (done) return;
    done = 1;
    if (settings_bool("net.lan", 1)) {
        NdSource s = {lan_src.name, udp_query, udp_datagram, &lan_src, NULL};
        nd_add_source(&s);
    } else {
        /* the answers to a direct query still need a parser */
        NdSource s = {lan_src.name, NULL, udp_datagram, &lan_src, NULL};
        nd_add_source(&s);
    }
    const char* j = settings_str("net.join", "");
    if (j && *j) {
        snprintf(direct_src.target, sizeof direct_src.target, "%s", j);
        NdSource s = {direct_src.name, udp_query, udp_datagram, &direct_src, NULL};
        nd_add_source(&s);
    }
}
