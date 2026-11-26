/*
 * net_session.c: the provider of net_session.h (port code, not from the original).
 *
 * Every incoming datagram is checked before anything is queued for the game: the header
 * (net_wire.c: magic, version, type, length), the session token, the sender (a known peer at the
 * address it was given), the payload's exact shape, and then what the sender may do: a machine
 * speaks only for its own players (their DPIDs carry its peer number), only the host may send
 * NM_PEER / NM_PEER_GONE / NM_SESSION / NM_READY, a game message must come from a player of the
 * sender and go to all or to one of ours. A malformed packet is dropped and logged (net_drop);
 * system messages are only ever built by dplay.c from these checked events.
 */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "host/net/net_session.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "host/net/net_announce.h"
#include "host/net/net_anwire.h"
#include "host/net/net_discovery.h"
#include "host/net/net_link.h"
#include "host/net/net_log.h"
#include "host/net/net_sock.h"
#include "host/settings/settings.h"

#define JOIN_TIMEOUT_MS 10000u
#define JOIN_TIMEOUT_ANNOUNCED_MS 15000u   /* introduction and punching first */
#define INTRO_RESEND_MS 1000u
#define INTRO_WAIT_MS 4000u                /* no INTRO by then: the listed addresses are tried anyway */
#define PUNCH_EVERY_MS 200u
#define PUNCH_FOR_MS 5000u
#define PUNCH_LOCAL_GRACE_MS 300u          /* the public address answered: the LAN one may still win */
#define PROBE_FOR_MS 5000u                 /* a member with two addresses: both tried this long */
#define JOIN_RESEND_MS 500u
#define CLOSE_FLUSH_MS 300u
#define EV_SOFT_CAP 16384    /* beyond this unreliable game messages are dropped */
#define EV_HARD_CAP 65536    /* beyond this everything is (the game has not pumped for minutes) */

#define TM2_APP NW_TM2_APP

typedef struct Peer {
    int used;
    uint8_t no;
    NetAddr addr;
    NetLink link;
    uint64_t nonce;          /* host side: the JOIN_REQ that made it (a repeated one gets the same answer) */
    NetAddr local;           /* its own LAN address as it reported it (0: unknown) */
    int probing;             /* sending to both addr and local until one answers (local wins) */
    uint32_t probe_until;
    const char* dead;        /* removal pending (done in tick, never inside the link's own callbacks) */
} Peer;

typedef struct Player {
    int used;
    NsPlayer p;
    uint32_t last22;         /* [gameplay] net_drop_stale: the newest 0x22 counter seen */
    int have22;
} Player;

enum { ST_NONE, ST_JOINING, ST_IN };

static struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int started, start_failed;
    NetSock* sock;
    uint16_t port;
    pthread_t thr;

    int state;
    int ready;               /* the snapshot is in (always for the host) */
    int is_host;
    uint64_t token;
    uint8_t me, host, next_peer;
    NwSessionDesc desc;
    char password[NW_MAX_NAME + 1];
    uint32_t next_idx;
    Peer peers[NW_MAX_PEERS];
    Player players[NW_MAX_PLAYERS];

    /* joining */
    uint64_t join_nonce;
    NetAddr join_addr;
    uint32_t join_hr;
    NwJoinReq join_req;
    int an_join;             /* joining a session heard of from the announce server */
    int intro, chosen;       /* INTRO in; an address picked for the JOIN_REQ */
    NetAddr cand_pub, cand_local;
    int ans_pub, ans_local;
    uint32_t ans_at;

    /* host: punching toward joiners the announce server introduced */
    struct { NetAddr a; uint32_t until, last; } punch[8];

    NsEvent* qhead, *qtail;
    int qn;
    int drop_stale;
    uint32_t timeout_ms;
} S = {.mu = PTHREAD_MUTEX_INITIALIZER, .cv = PTHREAD_COND_INITIALIZER};

static int tdiff(uint32_t a, uint32_t b) { return (int32_t)(a - b); }

/* ---- sending ---- */

static void raw_send(const NetAddr* to, const uint8_t* pkt, int len)
{
    if (S.sock) net_sock_send(S.sock, to, pkt, len);
}

static void link_out(void* user, const uint8_t* pkt, int len)
{
    Peer* p = user;
    raw_send(&p->addr, pkt, len);
    if (p->probing && p->local.ip && !net_addr_eq(&p->local, &p->addr)) raw_send(&p->local, pkt, len);
}

static NetLinkOut out_of(Peer* p) { NetLinkOut o = {link_out, p}; return o; }

static int io_send(void* user, const NetAddr* to, const uint8_t* pkt, int len)
{
    (void)user;
    raw_send(to, pkt, len);
    return 0;
}

static void send_rel(Peer* p, const NwMsg* m, uint32_t now)
{
    uint8_t buf[NL_MAX_PAYLOAD];
    int n = nw_msg_write(buf, sizeof buf, m);
    if (n <= 0) return;
    NetLinkOut o = out_of(p);
    if (nl_send_rel(&p->link, buf, n, now, &o) != 0) net_log("peer %u: reliable queue refused a message", p->no);
}

static void send_rel_all(const NwMsg* m, uint32_t now)
{
    for (int i = 0; i < NW_MAX_PEERS; i++)
        if (S.peers[i].used && !S.peers[i].dead) send_rel(&S.peers[i], m, now);
}

static void sessionless(const NetAddr* to, uint8_t type, const uint8_t* payload, int n)
{
    uint8_t pkt[NW_MAX_PACKET];
    if (n < 0 || n > NW_MAX_PACKET - NW_HDR) return;
    NwHdr h = {type, 0, 0, 0, 0, 0, 0};
    nw_hdr_write(pkt, &h);
    memcpy(pkt + NW_HDR, payload, (size_t)n);
    raw_send(to, pkt, NW_HDR + n);
}

/* ---- tables ---- */

static Peer* peer_find(uint8_t no)
{
    for (int i = 0; i < NW_MAX_PEERS; i++) if (S.peers[i].used && S.peers[i].no == no) return &S.peers[i];
    return NULL;
}

static Peer* peer_add(uint8_t no, const NetAddr* a, uint32_t now)
{
    for (int i = 0; i < NW_MAX_PEERS; i++)
        if (!S.peers[i].used) {
            Peer* p = &S.peers[i];
            memset(p, 0, sizeof *p);
            p->used = 1;
            p->no = no;
            p->addr = *a;
            nl_init(&p->link, S.me, no, S.token, S.timeout_ms, now);
            char t[32];
            net_log("peer %u at %s", no, net_addr_str(a, t, sizeof t));
            return p;
        }
    return NULL;
}

static Player* player_find(uint32_t id)
{
    for (int i = 0; i < NW_MAX_PLAYERS; i++) if (S.players[i].used && S.players[i].p.id == id) return &S.players[i];
    return NULL;
}

static int player_count(void)
{
    int n = 0;
    for (int i = 0; i < NW_MAX_PLAYERS; i++) n += S.players[i].used;
    return n;
}

static uint32_t local_player(void)
{
    for (int i = 0; i < NW_MAX_PLAYERS; i++) if (S.players[i].used && S.players[i].p.local) return S.players[i].p.id;
    return 0;
}

static int peer_has_players(uint8_t no)
{
    for (int i = 0; i < NW_MAX_PLAYERS; i++) if (S.players[i].used && S.players[i].p.owner == no) return 1;
    return 0;
}

/* DPIDs: 0x1xxx.... with 12 bits of the session token, the owner's peer number and an index:
 * never 0 or -1, unique per session, and the owner is readable from the id */
static uint32_t make_id(uint8_t owner, uint32_t idx)
{
    return 0x10000000u | (uint32_t)((S.token >> 8) & 0x0fffu) << 16 | (uint32_t)owner << 8 | (idx & 0xffu);
}
static uint8_t id_owner(uint32_t id) { return (uint8_t)(id >> 8); }

/* ---- the game's queue ---- */

static NsEvent* ev_new(int kind)
{
    NsEvent* e = calloc(1, sizeof *e);
    if (!e) return NULL;
    e->kind = kind;
    e->reliable = 1;
    e->to = local_player();
    return e;
}

static void ev_push(NsEvent* e)
{
    if (!e) return;
    if ((S.qn >= EV_SOFT_CAP && !e->reliable) || S.qn >= EV_HARD_CAP) {
        static int warned;
        if (!warned++) fprintf(stderr, "roadkill net: the game has not read its messages for a long time: dropping\n");
        free(e);
        return;
    }
    if (S.qtail) S.qtail->next = e; else S.qhead = e;
    S.qtail = e;
    S.qn++;
}

static void ev_clear(void)
{
    while (S.qhead) {
        NsEvent* e = S.qhead;
        S.qhead = e->next;
        free(e);
    }
    S.qtail = NULL;
    S.qn = 0;
}

static void ev_player(int kind, const NsPlayer* p)
{
    NsEvent* e = ev_new(kind);
    if (!e) return;
    e->id = p->id;
    memcpy(e->sname, p->sname, sizeof e->sname);
    memcpy(e->lname, p->lname, sizeof e->lname);
    e->n = p->n;
    memcpy(e->data, p->data, p->n);
    e->cur_players = (uint32_t)player_count();
    ev_push(e);
}

static void player_remove(Player* pl, int tell_game)
{
    if (tell_game) ev_player(NS_EV_DESTROY, &pl->p);
    net_log("player %08x (%s) gone", pl->p.id, pl->p.sname);
    pl->used = 0;
    S.desc.cur_players = (uint32_t)player_count();
}

/* ---- session state ---- */

static void reset_session(void)
{
    for (int i = 0; i < NW_MAX_PEERS; i++) {
        if (S.peers[i].used) nl_free(&S.peers[i].link);
        S.peers[i].used = 0;
    }
    memset(S.players, 0, sizeof S.players);
    ev_clear();
    S.state = ST_NONE;
    S.ready = S.is_host = 0;
    S.token = 0;
    S.me = S.host = S.next_peer = 0;
    memset(&S.desc, 0, sizeof S.desc);
    S.password[0] = 0;
    S.join_nonce = 0;
    S.join_hr = 0;
    S.an_join = S.intro = S.chosen = 0;
    S.ans_pub = S.ans_local = 0;
    memset(S.punch, 0, sizeof S.punch);
}

/* a peer is gone (BYE, timeout, the host said so): its players go, maybe the host role moves */
static void peer_remove(Peer* p, uint32_t now)
{
    uint8_t no = p->no;
    int was_host = no == S.host;
    net_log("peer %u removed: %s", no, p->dead ? p->dead : "?");
    for (int i = 0; i < NW_MAX_PLAYERS; i++)
        if (S.players[i].used && S.players[i].p.owner == no) player_remove(&S.players[i], S.ready);
    nl_free(&p->link);
    p->used = 0;
    if (S.is_host) {
        NwMsg m = {0};
        m.kind = NM_PEER_GONE;
        m.peer = no;
        send_rel_all(&m, now);
    }
    if (was_host && S.state == ST_IN) {
        if (!S.ready) {
            /* the host went before the snapshot was complete: the join fails */
            S.join_hr = DPERR_NOCONNECTION;
            pthread_cond_broadcast(&S.cv);
            return;
        }
        uint8_t best = S.me;
        for (int i = 0; i < NW_MAX_PEERS; i++) if (S.peers[i].used && S.peers[i].no < best) best = S.peers[i].no;
        S.host = best;
        if (best == S.me) {
            S.is_host = 1;
            fprintf(stderr, "roadkill net: the host left; this machine hosts the session now\n");
            ev_push(ev_new(NS_EV_HOST));
        } else net_log("the host left; peer %u hosts now", best);
    }
}

/* ---- checking and handling session messages ---- */

typedef struct Ctx { Peer* peer; uint32_t now; } Ctx;

/* 0: take it; -1: refuse it (not acked: it comes again). Malformed packets are refused; a
 * message about a player of the sender that is not known yet is refused (it is known once the
 * host's snapshot or the owner's NM_PLAYER arrives); other wrongs are taken and ignored in
 * handle(), so a reliable channel never stalls on them. */
static int check(void* user, const uint8_t* payload, int len)
{
    Ctx* c = user;
    NwMsg m;
    char a[32];
    if (nw_msg_read(payload, len, &m) != 0) {
        net_drop("peer %u (%s): malformed message (kind %u, %d bytes)", c->peer->no,
                 net_addr_str(&c->peer->addr, a, sizeof a), len > 0 ? payload[0] : 0, len);
        return -1;
    }
    if (m.kind == NM_GAME) {
        if (id_owner(m.from) == c->peer->no && !player_find(m.from)) return -1;
    }
    if ((m.kind == NM_PLAYER_DATA || m.kind == NM_PLAYER_GONE) && id_owner(m.id) == c->peer->no && !player_find(m.id))
        return -1;
    return 0;
}

static void check_unrel_kind(Ctx* c, const NwMsg* m, int* ok)
{
    if (m->kind != NM_GAME) {
        net_drop("peer %u: unreliable message of kind %u", c->peer->no, m->kind);
        *ok = 0;
    }
}

static void handle(void* user, int reliable, const uint8_t* payload, int len)
{
    Ctx* c = user;
    Peer* from = c->peer;
    NwMsg m;
    if (nw_msg_read(payload, len, &m) != 0) return;   /* check() refused it already */
    int ok = 1;
    if (!reliable) check_unrel_kind(c, &m, &ok);
    if (!ok || from->dead) return;
    int from_host = from->no == S.host;
    switch (m.kind) {
    case NM_GAME: {
        Player* src = player_find(m.from);
        if (!src || src->p.owner != from->no) { net_log("peer %u: game message from %08x, not its player", from->no, m.from); return; }
        uint32_t me = local_player();
        if (!me) return;   /* no local player yet: DirectPlay delivers to players only */
        if (m.to != 0) {
            Player* dst = player_find(m.to);
            if (!dst || !dst->p.local) return;
        }
        if (S.drop_stale && !reliable && m.n >= 12 && m.data[0] == 0x22) {
            /* [gameplay] net_drop_stale: the game's own car state counter (packet +8) */
            uint32_t seq = (uint32_t)m.data[8] | (uint32_t)m.data[9] << 8 | (uint32_t)m.data[10] << 16 | (uint32_t)m.data[11] << 24;
            if (src->have22 && (int32_t)(seq - src->last22) <= 0) return;
            src->last22 = seq;
            src->have22 = 1;
        }
        NsEvent* e = ev_new(NS_EV_GAME);
        if (!e) return;
        e->reliable = reliable;
        e->from = m.from;
        e->to = m.to ? m.to : me;
        e->n = m.n;
        memcpy(e->data, m.data, m.n);
        ev_push(e);
        return;
    }
    case NM_PEER: {
        if (!from_host) { net_drop("peer %u: NM_PEER from a member", from->no); return; }
        if (m.peer == S.me || peer_find(m.peer)) return;
        NetAddr a = {m.ip, m.port};
        /* a member on the host's own machine: reach it at the host's address */
        if (net_addr_is_loopback(&a) && !net_addr_is_loopback(&from->addr)) a.ip = from->addr.ip;
        if (m.peer >= S.next_peer) S.next_peer = (uint8_t)(m.peer + 1);
        Peer* np = peer_add(m.peer, &a, c->now);
        if (!np) { net_log("peer table full: peer %u ignored", m.peer); return; }
        NetAddr l = {m.lip, m.lport};
        if (m.lip && na_addr_usable(&l) && !net_addr_eq(&l, &a)) {
            /* both its addresses until one answers: the LAN one when both do (two players behind
             * one router), else the public one the host knows (through their NATs) */
            np->local = l;
            np->probing = 1;
            np->probe_until = c->now + PROBE_FOR_MS;
        }
        return;
    }
    case NM_PEER_GONE: {
        if (!from_host) { net_drop("peer %u: NM_PEER_GONE from a member", from->no); return; }
        Peer* p = peer_find(m.peer);
        if (p && !p->dead) p->dead = "the host reports it gone";
        return;
    }
    case NM_PLAYER: {
        /* a machine announces its own players; the host's snapshot announces everybody's */
        int snapshot = from_host && !S.ready;
        if (id_owner(m.id) != m.owner || (m.owner != from->no && !snapshot)) {
            net_drop("peer %u: player %08x of peer %u", from->no, m.id, m.owner);
            return;
        }
        if (m.owner == S.me || (m.owner != from->no && !peer_find(m.owner))) return;
        Player* pl = player_find(m.id);
        int added = 0;
        if (!pl) {
            for (int i = 0; i < NW_MAX_PLAYERS; i++) if (!S.players[i].used) { pl = &S.players[i]; break; }
            if (!pl) { net_log("player table full: %08x ignored", m.id); return; }
            memset(pl, 0, sizeof *pl);
            pl->used = 1;
            added = 1;
        }
        pl->p.id = m.id;
        pl->p.owner = m.owner;
        pl->p.local = 0;
        pl->p.flags = m.flags;
        memcpy(pl->p.sname, m.sname, sizeof pl->p.sname);
        memcpy(pl->p.lname, m.lname, sizeof pl->p.lname);
        pl->p.n = m.n;
        memcpy(pl->p.data, m.data, m.n);
        S.desc.cur_players = (uint32_t)player_count();
        if (added) {
            net_log("player %08x (%s / %s) of peer %u", m.id, m.sname, m.lname, m.owner);
            if (S.ready) ev_player(NS_EV_CREATE, &pl->p);
        }
        return;
    }
    case NM_PLAYER_GONE: {
        Player* pl = player_find(m.id);
        if (!pl || pl->p.owner != from->no) return;
        player_remove(pl, S.ready);
        return;
    }
    case NM_PLAYER_DATA: {
        Player* pl = player_find(m.id);
        if (!pl || pl->p.owner != from->no) return;
        pl->p.n = m.n;
        memcpy(pl->p.data, m.data, m.n);
        if (S.ready) ev_player(NS_EV_DATA, &pl->p);
        return;
    }
    case NM_SESSION:
        if (!from_host) { net_drop("peer %u: NM_SESSION from a member", from->no); return; }
        if (memcmp(m.d.guid, S.desc.guid, 16) != 0) return;
        S.desc = m.d;
        S.desc.cur_players = (uint32_t)player_count();
        return;
    case NM_READY:
        if (!from_host) return;
        if (!S.ready) {
            S.ready = 1;
            net_log("joined: the snapshot is in (%d players)", player_count());
            pthread_cond_broadcast(&S.cv);
        }
        return;
    }
}

/* ---- session-less packets ---- */

static void fill_info(NwSessionInfo* si)
{
    memset(si, 0, sizeof *si);
    si->d = S.desc;
    si->d.cur_players = (uint32_t)player_count();
    si->compat = ns_compat();
    for (int i = 0; i < NW_MAX_PLAYERS; i++) {
        if (!S.players[i].used) continue;
        NwPlayerName* n = &si->players[si->nplayers++];
        n->id = S.players[i].p.id;
        memcpy(n->sname, S.players[i].p.sname, sizeof n->sname);
        memcpy(n->lname, S.players[i].p.lname, sizeof n->lname);
    }
}

static void on_enum_req(const NetAddr* from, const uint8_t* payload, int len)
{
    uint8_t app[16];
    uint32_t compat;
    char a[32];
    if (nw_enum_req_read(payload, len, app, &compat) != 0) {
        net_drop("ENUM_REQ from %s: malformed", net_addr_str(from, a, sizeof a));
        return;
    }
    if (S.state != ST_IN || !S.is_host || memcmp(app, S.desc.app, 16) != 0) return;
    NwSessionInfo si;
    fill_info(&si);
    uint8_t buf[NW_MAX_PACKET];
    int n = nw_session_info_write(buf, NW_MAX_PACKET - NW_HDR, &si);
    if (n > 0) sessionless(from, NW_ENUM_RESP, buf, n);
}

static void refuse(const NetAddr* to, uint64_t nonce, uint32_t hr)
{
    uint8_t buf[16];
    int n = nw_join_refuse_write(buf, sizeof buf, nonce, hr);
    if (n > 0) sessionless(to, NW_JOIN_REFUSE, buf, n);
}

static void send_accept(Peer* p)
{
    NwJoinAccept j;
    memset(&j, 0, sizeof j);
    j.nonce = p->nonce;
    j.token = S.token;
    j.you = p->no;
    j.host = S.me;
    j.next_peer = S.next_peer;
    j.d = S.desc;
    j.d.cur_players = (uint32_t)player_count();
    uint8_t buf[NW_MAX_PACKET];
    int n = nw_join_accept_write(buf, NW_MAX_PACKET - NW_HDR, &j);
    if (n > 0) sessionless(&p->addr, NW_JOIN_ACCEPT, buf, n);
}

static void on_join_req(const NetAddr* from, const uint8_t* payload, int len, uint32_t now)
{
    NwJoinReq j;
    char a[32];
    if (nw_join_req_read(payload, len, &j) != 0) {
        net_drop("JOIN_REQ from %s: malformed", net_addr_str(from, a, sizeof a));
        return;
    }
    if (S.state != ST_IN || !S.is_host) return;
    if (memcmp(j.guid, S.desc.guid, 16) != 0 || memcmp(j.app, S.desc.app, 16) != 0) {
        refuse(from, j.nonce, DPERR_NOSESSIONS);
        return;
    }
    for (int i = 0; i < NW_MAX_PEERS; i++) {
        Peer* q = &S.peers[i];
        if (!q->used || !net_addr_eq(&q->addr, from)) continue;
        if (q->nonce == j.nonce) {
            send_accept(q);   /* our answer was lost */
            return;
        }
        /* a new join from a member's address: that machine started over (one peer per
         * address, so repeated asks cannot fill the session) */
        q->dead = "it joined again";
        peer_remove(q, now);
    }
    uint32_t hr = 0;
    int npeers = 0;
    for (int i = 0; i < NW_MAX_PEERS; i++) npeers += S.peers[i].used;
    if (j.compat != ns_compat()) hr = DPERR_INVALIDPARAMS;
    else if (S.desc.has_password && strcmp(j.password, S.password) != 0) hr = DPERR_INVALIDPASSWORD;
    else if (S.desc.flags & NS_NEWPLAYERSDISABLED) hr = DPERR_NONEWPLAYERS;
    else if ((uint32_t)player_count() >= S.desc.max_players) hr = DPERR_NONEWPLAYERS;
    else if (npeers >= NW_MAX_PEERS - 1 || S.next_peer >= 0xfe) hr = DPERR_CANTADDPLAYER;
    if (hr) {
        net_log("join from %s refused: %08x", net_addr_str(from, a, sizeof a), hr);
        refuse(from, j.nonce, hr);
        return;
    }
    Peer* p = peer_add(S.next_peer, from, now);
    if (!p) { refuse(from, j.nonce, DPERR_CANTADDPLAYER); return; }
    S.next_peer++;
    p->nonce = j.nonce;
    p->local.ip = j.local_ip;
    p->local.port = j.local_port;
    fprintf(stderr, "roadkill net: %s joins the session (peer %u)\n", net_addr_str(from, a, sizeof a), p->no);
    send_accept(p);
    /* the joiner's snapshot: the other members, every player, the session; then READY */
    NwMsg m;
    for (int i = 0; i < NW_MAX_PEERS; i++) {
        Peer* q = &S.peers[i];
        if (!q->used || q == p || q->dead) continue;
        memset(&m, 0, sizeof m);
        m.kind = NM_PEER;
        m.peer = q->no;
        m.ip = q->addr.ip;
        m.port = q->addr.port;
        m.lip = q->local.ip;
        m.lport = q->local.port;
        send_rel(p, &m, now);
        /* and the new one to the others */
        NwMsg n2;
        memset(&n2, 0, sizeof n2);
        n2.kind = NM_PEER;
        n2.peer = p->no;
        n2.ip = p->addr.ip;
        n2.port = p->addr.port;
        n2.lip = p->local.ip;
        n2.lport = p->local.port;
        send_rel(q, &n2, now);
    }
    for (int i = 0; i < NW_MAX_PLAYERS; i++) {
        if (!S.players[i].used) continue;
        const NsPlayer* pl = &S.players[i].p;
        memset(&m, 0, sizeof m);
        m.kind = NM_PLAYER;
        m.id = pl->id;
        m.owner = pl->owner;
        m.flags = pl->flags;
        memcpy(m.sname, pl->sname, sizeof m.sname);
        memcpy(m.lname, pl->lname, sizeof m.lname);
        m.n = pl->n;
        m.data = pl->data;
        send_rel(p, &m, now);
    }
    memset(&m, 0, sizeof m);
    m.kind = NM_SESSION;
    m.d = S.desc;
    send_rel(p, &m, now);
    memset(&m, 0, sizeof m);
    m.kind = NM_READY;
    send_rel(p, &m, now);
}

static void on_join_answer(const NetAddr* from, const NwHdr* h, const uint8_t* payload, int len, uint32_t now)
{
    char a[32];
    if (h->type == NW_JOIN_REFUSE) {
        uint64_t nonce;
        uint32_t hr;
        if (nw_join_refuse_read(payload, len, &nonce, &hr) != 0) {
            net_drop("JOIN_REFUSE from %s: malformed", net_addr_str(from, a, sizeof a));
            return;
        }
        if (S.state != ST_JOINING || nonce != S.join_nonce || !net_addr_eq(from, &S.join_addr)) return;
        S.join_hr = hr;
        pthread_cond_broadcast(&S.cv);
        return;
    }
    NwJoinAccept j;
    if (nw_join_accept_read(payload, len, &j) != 0) {
        net_drop("JOIN_ACCEPT from %s: malformed", net_addr_str(from, a, sizeof a));
        return;
    }
    if (S.state != ST_JOINING || j.nonce != S.join_nonce || !net_addr_eq(from, &S.join_addr)) return;
    if (memcmp(j.d.guid, S.join_req.guid, 16) != 0) return;
    S.token = j.token;
    S.me = j.you;
    S.host = j.host;
    S.next_peer = j.next_peer;
    S.desc = j.d;
    S.state = ST_IN;
    S.ready = 0;
    S.is_host = 0;
    if (!peer_add(j.host, from, now)) {
        S.join_hr = DPERR_GENERIC;
        pthread_cond_broadcast(&S.cv);
        return;
    }
    net_log("accepted as peer %u by the host (peer %u)", j.you, j.host);
}

/* ---- the socket thread ---- */

/* ---- reaching each other through NATs (announced sessions) ---- */

static void send_punch(const NetAddr* to, uint32_t kind)
{
    uint8_t b[4];
    nw_u32_write(b, 4, kind);
    sessionless(to, NW_PUNCH, b, 4);
}

/* PUNCH: a host answers an ask (that is what tells a joiner which address works); a joiner
 * notes which of the host's addresses answered. Anything else is ignored: punches exist to open
 * NAT mappings, so most arrive where nobody waits for them. */
static void on_punch(const NetAddr* from, const uint8_t* payload, int len, uint32_t now)
{
    uint32_t kind;
    char a[32];
    if (nw_u32_read(payload, len, &kind) != 0 || kind > 1) {
        net_drop("PUNCH from %s: malformed", net_addr_str(from, a, sizeof a));
        return;
    }
    if (kind == 0) {
        if (S.state == ST_IN && S.is_host) send_punch(from, 1);
        return;
    }
    if (S.state != ST_JOINING || !S.an_join || !S.intro || S.chosen) return;
    int was = S.ans_pub || S.ans_local;
    if (S.cand_local.ip && net_addr_eq(from, &S.cand_local)) S.ans_local = 1;
    else if (net_addr_eq(from, &S.cand_pub)) S.ans_pub = 1;
    else return;
    if (!was) S.ans_at = now;
    net_log("punch answered from %s", net_addr_str(from, a, sizeof a));
    pthread_cond_broadcast(&S.cv);
}

/* INTRO from the announce server (socket thread, S.mu held) */
static void on_intro(uint32_t arg, const uint8_t guid[16], const NetAddr* pub, const NetAddr* local, uint32_t now)
{
    char a[32], b[32];
    if (arg == 0) {
        /* to the host: a joiner is coming; punch toward both its addresses for a while */
        if (S.state != ST_IN || !S.is_host || memcmp(guid, S.desc.guid, 16) != 0) return;
        net_log("introduced to a joiner at %s / %s", net_addr_str(pub, a, sizeof a), net_addr_str(local, b, sizeof b));
        const NetAddr* t[2] = {pub, local};
        for (int k = 0; k < 2; k++) {
            if (!t[k]->ip) continue;
            int slot = 0;
            for (int i = 0; i < 8; i++) if (tdiff(S.punch[i].until, S.punch[slot].until) < 0) slot = i;
            S.punch[slot].a = *t[k];
            S.punch[slot].until = now + PUNCH_FOR_MS;
            S.punch[slot].last = now - PUNCH_EVERY_MS;
        }
        return;
    }
    if (S.state != ST_JOINING || !S.an_join || memcmp(guid, S.join_req.guid, 16) != 0) return;
    S.cand_pub = *pub;
    S.cand_local = *local;
    if (!S.intro) net_log("introduced to the host at %s / %s", net_addr_str(pub, a, sizeof a), net_addr_str(local, b, sizeof b));
    S.intro = 1;
    pthread_cond_broadcast(&S.cv);
}

static void on_datagram(const NetAddr* from, const uint8_t* pkt, int len, uint32_t now)
{
    NwHdr h;
    char a[32];
    if (len > NW_MAX_PACKET || nw_hdr_read(pkt, len, &h) != 0) {
        /* the announce server's protocol (only from its address; checked there) */
        NdIo io = {io_send, NULL, S.port, TM2_APP, ns_compat()};
        if (len <= NW_MAX_PACKET && nd_raw(&io, from, pkt, len, now)) return;
        net_drop("datagram from %s: not ours or malformed (%d bytes)", net_addr_str(from, a, sizeof a), len);
        return;
    }
    const uint8_t* payload = pkt + NW_HDR;
    int plen = len - NW_HDR;
    switch (h.type) {
    case NW_ENUM_REQ: on_enum_req(from, payload, plen); return;
    case NW_JOIN_REQ: on_join_req(from, payload, plen, now); return;
    case NW_PUNCH: on_punch(from, payload, plen, now); return;
    case NW_JOIN_ACCEPT:
    case NW_JOIN_REFUSE: on_join_answer(from, &h, payload, plen, now); return;
    case NW_ENUM_RESP: {
        NdIo io = {io_send, NULL, S.port, TM2_APP, ns_compat()};
        nd_datagram(&io, from, &h, payload, plen, now);
        return;
    }
    default: break;
    }
    if (S.state != ST_IN || h.token != S.token || h.to != S.me) {
        net_log("datagram from %s: not for this session", net_addr_str(from, a, sizeof a));
        return;
    }
    Peer* p = peer_find(h.from);
    if (!p || p->dead) {
        net_log("datagram from %s: unknown peer %u", net_addr_str(from, a, sizeof a), h.from);
        return;
    }
    int via_local = p->probing && p->local.ip && net_addr_eq(from, &p->local);
    if (!net_addr_eq(from, &p->addr) && !via_local) {
        net_drop("datagram for peer %u from %s: not its address", h.from, net_addr_str(from, a, sizeof a));
        return;
    }
    Ctx c = {p, now};
    NetLinkOut o = out_of(p);
    int r = nl_on_packet(&p->link, &h, payload, plen, now, &o, check, handle, &c);
    if (r >= 0 && p->probing) {
        if (via_local) {
            /* its LAN address answers: that one from now on */
            net_log("peer %u answers at its LAN address %s", p->no, net_addr_str(from, a, sizeof a));
            p->addr = p->local;
            p->probing = 0;
        } else if (tdiff(p->probe_until, now + PUNCH_LOCAL_GRACE_MS) > 0) {
            p->probe_until = now + PUNCH_LOCAL_GRACE_MS;   /* the public one answers: a little longer for the LAN one */
        }
    }
    if (r < 0) net_log("peer %u: packet type %u refused", p->no, h.type);
    else if (r == 1 && !p->dead) p->dead = "it left";
}

static void tick(uint32_t now)
{
    if (S.state != ST_IN) return;
    if (S.is_host) {
        NwSessionInfo si;
        fill_info(&si);
        NdIo io = {io_send, NULL, S.port, TM2_APP, ns_compat()};
        na_host_tick(&io, &si, now);
        for (int i = 0; i < 8; i++)
            if (S.punch[i].a.ip && tdiff(S.punch[i].until, now) > 0 && tdiff(now, S.punch[i].last) >= (int)PUNCH_EVERY_MS) {
                send_punch(&S.punch[i].a, 0);
                S.punch[i].last = now;
            }
    }
    for (int i = 0; i < NW_MAX_PEERS; i++)
        if (S.peers[i].used && S.peers[i].probing && tdiff(now, S.peers[i].probe_until) > 0) S.peers[i].probing = 0;
    static uint32_t stats_at;
    if (net_log_on() && tdiff(now, stats_at) >= 5000) {
        stats_at = now;
        for (int i = 0; i < NW_MAX_PEERS; i++) {
            const Peer* p = &S.peers[i];
            if (!p->used) continue;
            const NetLink* l = &p->link;
            net_log("peer %u: rtt %u ms, reliable %u sent / %u resent, unreliable %u out / %u in, %u packets in, %u dup, %u refused, "
                    "%d waiting", p->no, l->srtt, l->n_sent, l->n_resent, l->n_unrel_out, l->n_unrel_in, l->n_recv, l->n_dup,
                    l->n_dropped, l->nwait);
        }
    }
    for (int i = 0; i < NW_MAX_PEERS; i++) {
        Peer* p = &S.peers[i];
        if (!p->used) continue;
        NetLinkOut o = out_of(p);
        if (!p->dead && nl_tick(&p->link, now, &o)) {
            p->dead = p->link.broken ? "its link broke" : "timed out";
            fprintf(stderr, "roadkill net: peer %u %s\n", p->no, p->dead);
        }
    }
    for (int i = 0; i < NW_MAX_PEERS; i++)
        if (S.peers[i].used && S.peers[i].dead) peer_remove(&S.peers[i], now);
}

static void* thread_main(void* arg)
{
    (void)arg;
    uint8_t buf[NW_MAX_PACKET + 1];
    for (;;) {
        NetAddr from;
        int n = net_sock_recv(S.sock, &from, buf, NW_MAX_PACKET, 10);
        uint32_t now = net_now_ms();
        pthread_mutex_lock(&S.mu);
        if (n > 0) on_datagram(&from, buf, n, now);
        tick(now);
        uint16_t port = S.port;
        pthread_mutex_unlock(&S.mu);
        NdIo io = {io_send, NULL, port, TM2_APP, ns_compat()};
        nd_tick(&io, now);
        if (n < 0) {
            struct timespec t = {0, 10 * 1000000L};
            nanosleep(&t, NULL);
        }
    }
    return NULL;
}

/* ---- the API ---- */

int ns_available(void) { return net_sock_init() == 0; }

uint32_t ns_compat(void)
{
    /* gameplay settings every machine of a session shares: a bit each */
    static uint32_t c = 0xffffffffu;
    if (c == 0xffffffffu)
        c = (settings_bool("gameplay.fix_crashes", 0) ? 1u : 0u) | (settings_bool("gameplay.net_drop_stale", 0) ? 2u : 0u);
    return c;
}

uint32_t ns_build(void) { return 0x524b0000u | (uint32_t)NW_VERSION << 8 | (ns_compat() & 0xffu); }

int ns_start(void)
{
    pthread_mutex_lock(&S.mu);
    int r = 0;
    if (!S.started && !S.start_failed) {
        int port = settings_int("net.port", NW_DEFAULT_PORT);
        if (port < 1 || port > 65535) port = NW_DEFAULT_PORT;
        S.timeout_ms = (uint32_t)settings_int("net.timeout", 10) * 1000u;
        if (S.timeout_ms < 2000u) S.timeout_ms = 2000u;
        S.drop_stale = settings_bool("gameplay.net_drop_stale", 0);
        S.sock = net_sock_open((uint16_t)port, 10, &S.port);
        if (!S.sock) {
            fprintf(stderr, "roadkill net: cannot open a UDP port at %d..%d: no network play\n", port, port + 9);
            S.start_failed = 1;
        } else {
            nd_init();
            na_init(ns_build());
            na_set_intro(on_intro);
            if (pthread_create(&S.thr, NULL, thread_main, NULL) != 0) {
                net_sock_close(S.sock);
                S.sock = NULL;
                S.start_failed = 1;
            } else {
                pthread_detach(S.thr);
                S.started = 1;
                fprintf(stderr, "roadkill net: network play on UDP port %u\n", S.port);
            }
        }
    }
    if (!S.started) r = -1;
    pthread_mutex_unlock(&S.mu);
    return r;
}

uint16_t ns_port(void) { return S.port; }

uint32_t ns_host(NwSessionDesc* d, const char* password)
{
    if (ns_start() != 0) return DPERR_UNAVAILABLE;
    pthread_mutex_lock(&S.mu);
    if (S.state != ST_NONE) { pthread_mutex_unlock(&S.mu); return DPERR_ALREADYINITIALIZED; }
    reset_session();
    do net_random(&S.token, sizeof S.token); while (!S.token);
    net_random(d->guid, 16);
    d->guid[7] = (uint8_t)((d->guid[7] & 0x0f) | 0x40);   /* a version 4 GUID */
    if (!d->max_players || d->max_players > NW_MAX_PLAYERS) d->max_players = NW_MAX_PLAYERS;
    snprintf(S.password, sizeof S.password, "%s", password ? password : "");
    d->has_password = S.password[0] != 0;
    d->flags = (d->flags & ~NS_PASSWORDREQUIRED) | (d->has_password ? NS_PASSWORDREQUIRED : 0);
    d->cur_players = 0;
    S.desc = *d;
    S.me = S.host = 1;
    S.next_peer = 2;
    S.is_host = 1;
    S.ready = 1;
    S.state = ST_IN;
    fprintf(stderr, "roadkill net: hosting \"%s\" on UDP port %u\n", S.desc.name, S.port);
    pthread_mutex_unlock(&S.mu);
    return DP_OK;
}

static void wait_ms(uint32_t ms)
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_nsec += (long)ms * 1000000L;
    while (t.tv_nsec >= 1000000000L) { t.tv_sec++; t.tv_nsec -= 1000000000L; }
    pthread_cond_timedwait(&S.cv, &S.mu, &t);
}

static void send_join_req(void)
{
    uint8_t buf[NW_MAX_PACKET];
    int n = nw_join_req_write(buf, NW_MAX_PACKET - NW_HDR, &S.join_req);
    if (n > 0) sessionless(&S.join_addr, NW_JOIN_REQ, buf, n);
}

uint32_t ns_join(const uint8_t guid[16], const char* password, NwSessionDesc* out)
{
    if (ns_start() != 0) return DPERR_UNAVAILABLE;
    NdEntry e;
    if (nd_find(guid, &e, net_now_ms()) != 0) return DPERR_NOSESSIONS;
    pthread_mutex_lock(&S.mu);
    if (S.state != ST_NONE) { pthread_mutex_unlock(&S.mu); return DPERR_ALREADYINITIALIZED; }
    reset_session();
    S.state = ST_JOINING;
    S.join_addr = e.host;
    memset(&S.join_req, 0, sizeof S.join_req);
    do net_random(&S.join_nonce, sizeof S.join_nonce); while (!S.join_nonce);
    S.join_req.nonce = S.join_nonce;
    memcpy(S.join_req.app, e.s.d.app, 16);
    memcpy(S.join_req.guid, guid, 16);
    S.join_req.compat = ns_compat();
    snprintf(S.join_req.password, sizeof S.join_req.password, "%s", password ? password : "");
    NetAddr mine;
    if (na_local_addr(S.port, &mine) == 0) {
        S.join_req.local_ip = mine.ip;
        S.join_req.local_port = mine.port;
    }
    NdIo io = {io_send, NULL, S.port, TM2_APP, ns_compat()};
    char a[32];
    if (e.announced) {
        /* through the announce server: an introduction, then punches to the host's public and
         * LAN addresses; the JOIN_REQ goes where an answer came from (the LAN one first) */
        S.an_join = 1;
        S.cand_pub = e.pub;
        S.cand_local = e.local;
        fprintf(stderr, "roadkill net: joining \"%s\" through the announce server\n", e.s.d.name);
    } else {
        S.chosen = 1;
        fprintf(stderr, "roadkill net: joining \"%s\" at %s\n", e.s.d.name, net_addr_str(&e.host, a, sizeof a));
    }
    uint32_t start = net_now_ms(), last = start - JOIN_RESEND_MS, last_intro = start - INTRO_RESEND_MS;
    uint32_t last_punch = start - PUNCH_EVERY_MS, punch_start = 0;
    uint32_t hr = DPERR_TIMEOUT;
    for (;;) {
        uint32_t now = net_now_ms();
        if (S.join_hr) { hr = S.join_hr; break; }
        if (S.state == ST_IN && S.ready) { hr = DP_OK; break; }
        if (tdiff(now, start) > (int)(S.an_join ? JOIN_TIMEOUT_ANNOUNCED_MS : JOIN_TIMEOUT_MS)) break;
        if (S.state == ST_JOINING && !S.chosen) {
            if (!S.intro && tdiff(now, last_intro) >= (int)INTRO_RESEND_MS) {
                na_join(&io, guid);
                last_intro = now;
            }
            if (!S.intro && tdiff(now, start) > (int)INTRO_WAIT_MS) {
                net_log("no introduction from the announce server: trying the listed addresses");
                S.intro = 1;
            }
            if (S.intro) {
                if (!punch_start) punch_start = now;
                if (tdiff(now, last_punch) >= (int)PUNCH_EVERY_MS) {
                    send_punch(&S.cand_pub, 0);
                    if (S.cand_local.ip) send_punch(&S.cand_local, 0);
                    last_punch = now;
                }
                const NetAddr* pick = NULL;
                if (S.ans_local) pick = &S.cand_local;
                else if (S.ans_pub && tdiff(now, S.ans_at) >= (int)PUNCH_LOCAL_GRACE_MS) pick = &S.cand_pub;
                else if (tdiff(now, punch_start) > (int)PUNCH_FOR_MS) pick = &S.cand_pub;   /* no answer: try anyway */
                if (pick) {
                    S.join_addr = *pick;
                    S.chosen = 1;
                    fprintf(stderr, "roadkill net: joining at %s (%s)\n", net_addr_str(pick, a, sizeof a),
                            pick == &S.cand_local ? "its LAN address" : S.ans_pub ? "its public address" : "no punch answered");
                }
            }
        }
        if (S.state == ST_JOINING && S.chosen && tdiff(now, last) >= (int)JOIN_RESEND_MS) {
            send_join_req();
            last = now;
        }
        wait_ms(S.chosen ? 50 : 20);
    }
    if (hr == DP_OK) {
        if (out) *out = S.desc;
        fprintf(stderr, "roadkill net: joined \"%s\" as peer %u\n", S.desc.name, S.me);
    } else {
        fprintf(stderr, "roadkill net: joining failed (%08x)\n", hr);
        for (int i = 0; i < NW_MAX_PEERS; i++)
            if (S.peers[i].used) {
                NetLinkOut o = out_of(&S.peers[i]);
                nl_send_bye(&S.peers[i].link, &o);
            }
        reset_session();
    }
    pthread_mutex_unlock(&S.mu);
    return hr;
}

void ns_close(void)
{
    pthread_mutex_lock(&S.mu);
    if (S.state == ST_NONE) { pthread_mutex_unlock(&S.mu); return; }
    if (S.state == ST_IN) {
        /* let what is in flight arrive before the BYE that ends the links */
        uint32_t start = net_now_ms();
        for (;;) {
            int busy = 0;
            for (int i = 0; i < NW_MAX_PEERS; i++)
                if (S.peers[i].used && !S.peers[i].dead && !nl_idle(&S.peers[i].link)) busy = 1;
            if (!busy || tdiff(net_now_ms(), start) > (int)CLOSE_FLUSH_MS) break;
            wait_ms(10);
        }
        for (int k = 0; k < 3; k++)
            for (int i = 0; i < NW_MAX_PEERS; i++)
                if (S.peers[i].used) {
                    NetLinkOut o = out_of(&S.peers[i]);
                    nl_send_bye(&S.peers[i].link, &o);
                }
        if (S.is_host) {
            /* after the flush (its waits let the socket thread register again) */
            NdIo io = {io_send, NULL, S.port, TM2_APP, ns_compat()};
            na_withdraw(&io);
        }
        fprintf(stderr, "roadkill net: left the session\n");
    }
    reset_session();
    pthread_mutex_unlock(&S.mu);
}

int ns_in_session(void)
{
    pthread_mutex_lock(&S.mu);
    int r = S.state == ST_IN;
    pthread_mutex_unlock(&S.mu);
    return r;
}

int ns_is_host(void)
{
    pthread_mutex_lock(&S.mu);
    int r = S.state == ST_IN && S.is_host;
    pthread_mutex_unlock(&S.mu);
    return r;
}

int ns_session_desc(NwSessionDesc* out)
{
    pthread_mutex_lock(&S.mu);
    int r = -1;
    if (S.state == ST_IN) {
        *out = S.desc;
        out->cur_players = (uint32_t)player_count();
        r = 0;
    }
    pthread_mutex_unlock(&S.mu);
    return r;
}

void ns_enable_new_players(int on)
{
    pthread_mutex_lock(&S.mu);
    if (S.state == ST_IN && S.is_host) {
        uint32_t f = on ? S.desc.flags & ~NS_NEWPLAYERSDISABLED : S.desc.flags | NS_NEWPLAYERSDISABLED;
        if (f != S.desc.flags) {
            S.desc.flags = f;
            net_log("new players %s", on ? "enabled" : "disabled");
            NwMsg m;
            memset(&m, 0, sizeof m);
            m.kind = NM_SESSION;
            m.d = S.desc;
            send_rel_all(&m, net_now_ms());
        }
    }
    pthread_mutex_unlock(&S.mu);
}

static void copy_name(char* dst, const char* src)
{
    snprintf(dst, NW_MAX_NAME + 1, "%s", src ? src : "");
}

uint32_t ns_create_player(const char* sname, const char* lname, const uint8_t* data, int n, uint32_t flags, uint32_t* id)
{
    if (n < 0 || n > NW_MAX_PDATA) return DPERR_INVALIDPARAMS;
    pthread_mutex_lock(&S.mu);
    uint32_t hr = DP_OK;
    Player* pl = NULL;
    if (S.state != ST_IN) hr = DPERR_INVALIDOBJECT;
    else if ((uint32_t)player_count() >= S.desc.max_players) hr = DPERR_CANTADDPLAYER;
    else {
        for (int i = 0; i < NW_MAX_PLAYERS; i++) if (!S.players[i].used) { pl = &S.players[i]; break; }
        if (!pl) hr = DPERR_CANTADDPLAYER;
    }
    if (hr == DP_OK) {
        uint32_t pid;
        do pid = make_id(S.me, (S.next_idx++ % 255u) + 1u); while (player_find(pid));
        memset(pl, 0, sizeof *pl);
        pl->used = 1;
        pl->p.id = pid;
        pl->p.owner = S.me;
        pl->p.local = 1;
        pl->p.flags = flags;
        copy_name(pl->p.sname, sname);
        copy_name(pl->p.lname, lname);
        pl->p.n = (uint16_t)n;
        if (n) memcpy(pl->p.data, data, (size_t)n);
        S.desc.cur_players = (uint32_t)player_count();
        *id = pid;
        NwMsg m;
        memset(&m, 0, sizeof m);
        m.kind = NM_PLAYER;
        m.id = pid;
        m.owner = S.me;
        m.flags = flags;
        memcpy(m.sname, pl->p.sname, sizeof m.sname);
        memcpy(m.lname, pl->p.lname, sizeof m.lname);
        m.n = pl->p.n;
        m.data = pl->p.data;
        send_rel_all(&m, net_now_ms());
        net_log("local player %08x (%s / %s)", pid, pl->p.sname, pl->p.lname);
    }
    pthread_mutex_unlock(&S.mu);
    return hr;
}

uint32_t ns_destroy_player(uint32_t id)
{
    pthread_mutex_lock(&S.mu);
    uint32_t hr = DP_OK;
    Player* pl = S.state == ST_IN ? player_find(id) : NULL;
    if (!pl || !pl->p.local) hr = DPERR_INVALIDPLAYER;
    else {
        NwMsg m;
        memset(&m, 0, sizeof m);
        m.kind = NM_PLAYER_GONE;
        m.id = id;
        send_rel_all(&m, net_now_ms());
        player_remove(pl, 0);
    }
    pthread_mutex_unlock(&S.mu);
    return hr;
}

uint32_t ns_set_player_data(uint32_t id, const uint8_t* data, int n, uint32_t flags)
{
    if (n < 0 || n > NW_MAX_PDATA) return DPERR_INVALIDPARAMS;
    pthread_mutex_lock(&S.mu);
    uint32_t hr = DP_OK;
    Player* pl = S.state == ST_IN ? player_find(id) : NULL;
    if (!pl) hr = DPERR_INVALIDPLAYER;
    else if (!pl->p.local && !(flags & 1)) hr = DPERR_ACCESSDENIED;   /* DPSET_LOCAL only for others' players */
    else {
        pl->p.n = (uint16_t)n;
        if (n) memcpy(pl->p.data, data, (size_t)n);
        if (pl->p.local && !(flags & 1)) {
            NwMsg m;
            memset(&m, 0, sizeof m);
            m.kind = NM_PLAYER_DATA;
            m.id = id;
            m.n = pl->p.n;
            m.data = pl->p.data;
            send_rel_all(&m, net_now_ms());   /* DPSET_GUARANTEED or not: always reliable */
        }
    }
    pthread_mutex_unlock(&S.mu);
    return hr;
}

uint32_t ns_get_player(uint32_t id, NsPlayer* out)
{
    pthread_mutex_lock(&S.mu);
    Player* pl = S.state == ST_IN ? player_find(id) : NULL;
    if (pl) *out = pl->p;
    pthread_mutex_unlock(&S.mu);
    return pl ? DP_OK : DPERR_INVALIDPLAYER;
}

int ns_players(NsPlayer* out, int max)
{
    pthread_mutex_lock(&S.mu);
    int n = 0;
    if (S.state == ST_IN)
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i < NW_MAX_PLAYERS && n < max; i++)
                if (S.players[i].used && S.players[i].p.local == !pass) out[n++] = S.players[i].p;
    pthread_mutex_unlock(&S.mu);
    return n;
}

uint32_t ns_send(uint32_t from, uint32_t to, uint32_t flags, const uint8_t* data, int n)
{
    if (n <= 0) return DPERR_INVALIDPARAMS;
    if (n > NW_MAX_GAME) return DPERR_SENDTOOBIG;
    pthread_mutex_lock(&S.mu);
    uint32_t hr = DP_OK;
    Player* src = S.state == ST_IN ? player_find(from) : NULL;
    if (S.state != ST_IN) hr = DPERR_INVALIDOBJECT;
    else if (!src || !src->p.local) hr = DPERR_INVALIDPLAYER;
    else {
        uint32_t now = net_now_ms();
        NwMsg m;
        memset(&m, 0, sizeof m);
        m.kind = NM_GAME;
        m.from = from;
        m.to = to;
        m.n = (uint16_t)n;
        m.data = data;
        uint8_t buf[NL_MAX_PAYLOAD];
        int len = nw_msg_write(buf, sizeof buf, &m);
        int reliable = flags & 1;
        Player* dst = to ? player_find(to) : NULL;
        if (to && !dst) hr = DPERR_INVALIDPLAYER;
        else if (len <= 0) hr = DPERR_SENDTOOBIG;
        else {
            for (int i = 0; i < NW_MAX_PEERS; i++) {
                Peer* p = &S.peers[i];
                if (!p->used || p->dead) continue;
                if (to ? dst->p.owner != p->no : !peer_has_players(p->no)) continue;
                NetLinkOut o = out_of(p);
                if (reliable) nl_send_rel(&p->link, buf, len, now, &o);
                else nl_send_unrel(&p->link, buf, len, now, &o);
            }
            /* other local players (the game has one) get it too, as DirectPlay delivers locally */
            for (int i = 0; i < NW_MAX_PLAYERS; i++) {
                Player* q = &S.players[i];
                if (!q->used || !q->p.local || q->p.id == from || (to && q->p.id != to)) continue;
                NsEvent* e = ev_new(NS_EV_GAME);
                if (!e) continue;
                e->reliable = reliable;
                e->from = from;
                e->to = q->p.id;
                e->n = (uint16_t)n;
                memcpy(e->data, data, (size_t)n);
                ev_push(e);
            }
        }
    }
    pthread_mutex_unlock(&S.mu);
    return hr;
}

int ns_recv(NsEvent* out, int remove)
{
    pthread_mutex_lock(&S.mu);
    NsEvent* e = S.qhead;
    if (e) {
        *out = *e;
        out->next = NULL;
        if (remove) {
            S.qhead = e->next;
            if (!S.qhead) S.qtail = NULL;
            S.qn--;
            free(e);
        }
    }
    pthread_mutex_unlock(&S.mu);
    return e ? 0 : -1;
}

int ns_queued(void)
{
    pthread_mutex_lock(&S.mu);
    int n = S.qn;
    pthread_mutex_unlock(&S.mu);
    return n;
}
