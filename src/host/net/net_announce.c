/*
 * net_announce.c: net_announce.h (port code, not from the original).
 */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "host/net/net_announce.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "host/net/net_anwire.h"
#include "host/net/net_log.h"
#include "host/settings/settings.h"

#define REGISTER_EVERY_MS 5000u
#define REGISTER_SOON_MS 1000u   /* after a change, at most this often */
#define MAX_PAGES 64

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static char target[256];
static NetAddr server;
static int resolved;               /* 1: server is valid */
static uint32_t build_sig;
static NaIntro intro_fn;

/* host registration */
static uint8_t reg_guid[16];
static int registered;             /* a REGISTER went out for reg_guid */
static uint32_t reg_at;
static uint8_t reg_last[sizeof(AnRegister)];
static int reg_logged;
/* listing */
static uint32_t page_pending;

static int tdiff(uint32_t a, uint32_t b) { return (int32_t)(a - b); }

int na_server(NetAddr* out)
{
    pthread_mutex_lock(&mu);
    int r = resolved ? 0 : -1;
    if (resolved) *out = server;
    pthread_mutex_unlock(&mu);
    return r;
}

void na_set_intro(NaIntro fn) { intro_fn = fn; }

int na_local_addr(uint16_t port, NetAddr* out)
{
    NetAddr toward = {0, 0};
    na_server(&toward);
    uint32_t ip;
    if (net_sock_local_ip(&toward, &ip) != 0) return -1;
    out->ip = ip;
    out->port = port;
    return 0;
}

/* the name lookup: at once, again every 10 minutes (30 s after a failure) */
static void* resolver(void* arg)
{
    (void)arg;
    for (;;) {
        NetAddr a;
        int ok = net_addr_resolve(target, AN_PORT, &a) == 0;
        pthread_mutex_lock(&mu);
        int was = resolved;
        if (ok) { server = a; resolved = 1; }
        pthread_mutex_unlock(&mu);
        char t[32];
        if (ok && !was) net_log("announce: %s is %s", target, net_addr_str(&a, t, sizeof t));
        if (!ok && !was) fprintf(stderr, "roadkill net: [net] announce: cannot look up \"%s\" (trying again)\n", target);
        struct timespec ts = {ok ? 600 : 30, 0};
        nanosleep(&ts, NULL);
    }
    return NULL;
}

static void send_list(const NdIo* io, uint32_t page)
{
    NetAddr s;
    if (na_server(&s) != 0) return;
    uint8_t pkt[64];
    int n = na_list_write(pkt, sizeof pkt, page, build_sig);
    if (n > 0) io->send(io->user, &s, pkt, n);
}

static void query(void* ctx, const NdIo* io, uint32_t now)
{
    (void)ctx; (void)now;
    page_pending = 0;
    send_list(io, 0);
}

static int raw(void* ctx, const NdIo* io, const NetAddr* from, const uint8_t* pkt, int len, uint32_t now)
{
    (void)ctx;
    NetAddr s;
    if (na_server(&s) != 0 || !net_addr_eq(from, &s)) return 0;
    uint32_t type, arg;
    if (na_header_read(pkt, len, &type, &arg) != 0) {
        net_drop("announce server: not its protocol (%d bytes)", len);
        return 1;
    }
    switch (type) {
    case AN_SESSIONS: {
        NaSession ss[AN_PAGE];
        int n;
        uint32_t total;
        if (na_sessions_read(pkt, len, build_sig, &total, ss, &n) != 0) {
            net_drop("announce server: malformed SESSIONS (%d bytes)", len);
            return 1;
        }
        for (int i = 0; i < n; i++) nd_offer_announced(&ss[i].s, &ss[i].pub, &ss[i].local, "announce", now);
        if (total > (page_pending + 1) * AN_PAGE && page_pending + 1 < MAX_PAGES) send_list(io, ++page_pending);
        return 1;
    }
    case AN_REGISTERED: {
        NetAddr pub;
        char t[32];
        if (na_registered_read(pkt, len, &pub) != 0) { net_drop("announce server: malformed REGISTERED"); return 1; }
        if (!reg_logged) {
            fprintf(stderr, "roadkill net: the session is announced on %s (public address %s)\n", target,
                    net_addr_str(&pub, t, sizeof t));
            reg_logged = 1;
        }
        return 1;
    }
    case AN_INTRO: {
        uint8_t guid[16];
        NetAddr pub, local;
        if (na_intro_read(pkt, len, &arg, guid, &pub, &local) != 0) { net_drop("announce server: malformed INTRO"); return 1; }
        if (intro_fn) intro_fn(arg, guid, &pub, &local, now);
        return 1;
    }
    default:
        net_drop("announce server: unexpected type %u", type);
        return 1;
    }
}

void na_init(uint32_t build)
{
    static int done;
    if (done) return;
    done = 1;
    build_sig = build;
    const char* t = settings_str("net.announce", "roadkill.horrible.dev:27400");
    if (!t || !*t) return;
    snprintf(target, sizeof target, "%s", t);
    pthread_t th;
    if (pthread_create(&th, NULL, resolver, NULL) == 0) pthread_detach(th);
    /* the join list: LIST while EnumSessions runs (queries from the socket thread) */
    NdSource src = {"announce", query, NULL, NULL, raw};
    nd_add_source(&src);
}

void na_host_tick(const NdIo* io, const NwSessionInfo* si, uint32_t now)
{
    NetAddr s;
    if (!settings_bool("net.public", 1) || na_server(&s) != 0) return;
    if (registered && memcmp(reg_guid, si->d.guid, 16) != 0) registered = 0;
    /* the LAN address, looked at again every 30 s (a laptop changes networks) */
    static NetAddr local;
    static uint32_t local_at;
    static int have_local;
    if (!have_local || tdiff(now, local_at) >= 30000) {
        local.ip = local.port = 0;
        na_local_addr(io->my_port, &local);
        local_at = now;
        have_local = 1;
    }
    uint8_t pkt[sizeof(AnRegister)];
    int n = na_register_write(pkt, sizeof pkt, si, build_sig, &local);
    if (n <= 0) return;
    int changed = !registered || memcmp(pkt, reg_last, sizeof pkt) != 0;
    int due = !registered || tdiff(now, reg_at) >= (int)REGISTER_EVERY_MS || (changed && tdiff(now, reg_at) >= (int)REGISTER_SOON_MS);
    if (!due) return;
    io->send(io->user, &s, pkt, n);
    if (!registered) {
        char t[32];
        net_log("announce: registering \"%s\" (local %s)", si->d.name, net_addr_str(&local, t, sizeof t));
        reg_logged = 0;
    }
    memcpy(reg_guid, si->d.guid, 16);
    memcpy(reg_last, pkt, sizeof pkt);
    registered = 1;
    reg_at = now;
}

void na_withdraw(const NdIo* io)
{
    NetAddr s;
    if (!registered || na_server(&s) != 0) return;
    uint8_t pkt[64];
    int n = na_withdraw_write(pkt, sizeof pkt, reg_guid);
    for (int i = 0; i < 2 && n > 0; i++) io->send(io->user, &s, pkt, n);
    registered = 0;
    net_log("announce: withdrawn");
}

void na_join(const NdIo* io, const uint8_t guid[16])
{
    NetAddr s;
    if (na_server(&s) != 0) return;
    NetAddr local = {0, 0};
    na_local_addr(io->my_port, &local);
    uint8_t pkt[64];
    int n = na_join_write(pkt, sizeof pkt, guid, &local);
    if (n > 0) io->send(io->user, &s, pkt, n);
}
