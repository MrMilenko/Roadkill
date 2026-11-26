/*
 * test_net.c: the network provider's transport and packet checks on their own (src/host/net
 * net_wire.c, net_link.c), no sockets, no game: ./build/test_net (exit 0 = every check passed).
 *
 *   1. headers and payloads round trip; malformed ones are refused (bad magic, version, type,
 *      lengths, tokens, strings with a NUL or longer than the game's fields, game messages of 0 or
 *      more than 0x400 bytes, DPIDs 0 / -1, trailing bytes), and random / mutated datagrams never
 *      get through as something else
 *   2. two links over a simulated network that loses, duplicates and reorders packets: every
 *      reliable message arrives exactly once and in order, both ways
 *   3. a reliable message the receiver refuses is not acked (it comes again), acks for something
 *      never sent are refused, and a silent peer times out
 *   4. the announce server's datagrams (net_anwire.c): what the host registers comes back as the
 *      join screen's session exactly; wrong sizes, versions, counts, builds, strings without a
 *      NUL, player counts and addresses are refused
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host/net/net_anwire.h"
#include "host/net/net_link.h"
#include "host/net/net_wire.h"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t rng = 12345;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* ---- 1. wire ---- */

static int mkhdr(uint8_t* p, uint8_t type, uint8_t from, uint8_t to, uint64_t token)
{
    NwHdr h = {type, from, to, token, 7, 3, 0x5};
    nw_hdr_write(p, &h);
    return NW_HDR;
}

static void test_wire(void)
{
    uint8_t p[NW_MAX_PACKET + 16];
    NwHdr h;

    mkhdr(p, NW_REL, 1, 2, 0x1122334455667788ull);
    CHECK(nw_hdr_read(p, NW_HDR, &h) == 0 && h.type == NW_REL && h.from == 1 && h.to == 2 && h.token == 0x1122334455667788ull
          && h.seq == 7 && h.ack == 3 && h.ack_bits == 5, "header round trip");
    CHECK(nw_hdr_read(p, NW_HDR - 1, &h) != 0, "short header refused");
    CHECK(nw_hdr_read(p, NW_MAX_PACKET + 1, &h) != 0, "oversized datagram refused");
    p[0] ^= 1;
    CHECK(nw_hdr_read(p, NW_HDR, &h) != 0, "bad magic refused");
    p[0] ^= 1;
    p[4] = NW_VERSION + 1;
    CHECK(nw_hdr_read(p, NW_HDR, &h) != 0, "other protocol version refused");
    p[4] = NW_VERSION - 1;
    CHECK(nw_hdr_read(p, NW_HDR, &h) != 0, "an older protocol version refused");
    p[4] = NW_VERSION;
    p[5] = 0x55;
    CHECK(nw_hdr_read(p, NW_HDR, &h) != 0, "unknown type refused");
    mkhdr(p, NW_REL, 1, 2, 0);
    CHECK(nw_hdr_read(p, NW_HDR, &h) != 0, "session packet without a token refused");
    mkhdr(p, NW_REL, 0, 2, 9);
    CHECK(nw_hdr_read(p, NW_HDR, &h) != 0, "session packet from peer 0 refused");
    mkhdr(p, NW_ENUM_REQ, 0, 0, 9);
    CHECK(nw_hdr_read(p, NW_HDR, &h) != 0, "session-less packet with a token refused");
    mkhdr(p, NW_ENUM_REQ, 0, 0, 0);
    CHECK(nw_hdr_read(p, NW_HDR, &h) == 0, "session-less header");

    /* messages */
    uint8_t game[NW_MAX_GAME + 1];
    for (int i = 0; i < (int)sizeof game; i++) game[i] = (uint8_t)i;
    NwMsg m, r;
    memset(&m, 0, sizeof m);
    m.kind = NM_GAME;
    m.from = 0x10000101u;
    m.to = 0;
    m.n = NW_MAX_GAME;
    m.data = game;
    int n = nw_msg_write(p, NL_MAX_PAYLOAD, &m);
    CHECK(n > 0 && nw_msg_read(p, n, &r) == 0 && r.n == NW_MAX_GAME && !memcmp(r.data, game, NW_MAX_GAME), "0x400 byte game message");
    CHECK(nw_msg_read(p, n - 1, &r) != 0, "cut game message refused");
    p[n] = 0;
    CHECK(nw_msg_read(p, n + 1, &r) != 0, "trailing byte refused");
    m.n = NW_MAX_GAME + 1;
    CHECK(nw_msg_write(p, sizeof p, &m) == 0, "0x401 byte game message not written");
    /* forge one: size field 0x401 */
    m.n = 4;
    n = nw_msg_write(p, NL_MAX_PAYLOAD, &m);
    p[9] = 0x01; p[10] = 0x04;
    CHECK(nw_msg_read(p, n, &r) != 0, "forged 0x401 size refused");
    m.n = 4;
    n = nw_msg_write(p, NL_MAX_PAYLOAD, &m);
    p[9] = 0; p[10] = 0;
    CHECK(nw_msg_read(p, n - 4, &r) != 0, "empty game message refused");
    m.from = 0;
    n = nw_msg_write(p, NL_MAX_PAYLOAD, &m);
    CHECK(nw_msg_read(p, n, &r) != 0, "game message from DPID 0 refused");
    m.from = 0xffffffffu;
    n = nw_msg_write(p, NL_MAX_PAYLOAD, &m);
    CHECK(nw_msg_read(p, n, &r) != 0, "game message from DPID -1 refused");

    memset(&m, 0, sizeof m);
    m.kind = NM_PLAYER;
    m.id = 0x10000203u;
    m.owner = 2;
    strcpy(m.sname, "Sweet Tooth");
    strcpy(m.lname, "Needles Kane");
    m.n = 0x138;
    m.data = game;
    n = nw_msg_write(p, NL_MAX_PAYLOAD, &m);
    CHECK(n > 0 && nw_msg_read(p, n, &r) == 0 && !strcmp(r.sname, "Sweet Tooth") && !strcmp(r.lname, "Needles Kane") && r.n == 0x138,
          "player round trip");
    /* the short name's length byte is at 1 + 4 + 1 + 4 = 10 */
    uint8_t q[NW_MAX_PACKET];
    memcpy(q, p, (size_t)n);
    q[12] = 0;
    CHECK(nw_msg_read(q, n, &r) != 0, "name with a NUL refused");
    memcpy(q, p, (size_t)n);
    q[10] = 0x80;
    CHECK(nw_msg_read(q, n, &r) != 0, "name longer than 0x7f refused");
    memcpy(q, p, (size_t)n);
    q[10] = 0xff;
    CHECK(nw_msg_read(q, n, &r) != 0, "name past the end refused");
    m.n = NW_MAX_PDATA + 1;
    CHECK(nw_msg_write(p, sizeof p, &m) == 0, "oversized player data not written");
    q[0] = 99;
    CHECK(nw_msg_read(q, n, &r) != 0, "unknown message kind refused");

    /* ENUM_REQ: padded to a fixed size (no amplification), anything else refused */
    uint8_t app[16] = {1, 2, 3}, app2[16];
    uint32_t compat;
    n = nw_enum_req_write(p, NW_MAX_PACKET, app, 7);
    CHECK(n == NW_ENUM_REQ_SIZE && nw_enum_req_read(p, n, app2, &compat) == 0 && compat == 7 && !memcmp(app, app2, 16), "enum request");
    CHECK(nw_enum_req_read(p, 20, app2, &compat) != 0, "short (unpadded) enum request refused");

    /* session info, join */
    NwSessionInfo si, so;
    memset(&si, 0, sizeof si);
    memcpy(si.d.name, "Arena", 6);
    si.d.max_players = 8;
    si.d.cur_players = 2;
    si.nplayers = 2;
    si.players[0].id = 0x10000101u;
    strcpy(si.players[0].sname, "a");
    si.players[1].id = 0x10000201u;
    strcpy(si.players[1].lname, "b");
    n = nw_session_info_write(p, NW_MAX_PACKET - NW_HDR, &si);
    CHECK(n > 0 && nw_session_info_read(p, n, &so) == 0 && so.nplayers == 2 && !strcmp(so.d.name, "Arena"), "session info round trip");
    si.d.max_players = 1000;
    n = nw_session_info_write(p, NW_MAX_PACKET - NW_HDR, &si);
    CHECK(nw_session_info_read(p, n, &so) != 0, "absurd max players refused");
    /* full names for 16 players do not fit one datagram: the names that fit go */
    memset(&si, 0, sizeof si);
    si.d.max_players = 16;
    si.nplayers = 16;
    for (int i = 0; i < 16; i++) {
        si.players[i].id = 0x10000101u + (uint32_t)i;
        memset(si.players[i].sname, 'x', NW_MAX_NAME);
        memset(si.players[i].lname, 'y', NW_MAX_NAME);
    }
    n = nw_session_info_write(p, NW_MAX_PACKET - NW_HDR, &si);
    CHECK(n > 0 && n <= NW_MAX_PACKET - NW_HDR && nw_session_info_read(p, n, &so) == 0 && so.nplayers > 0 && so.nplayers < 16,
          "long name lists cut to the datagram");

    NwJoinAccept ja, jb;
    memset(&ja, 0, sizeof ja);
    ja.nonce = 5;
    ja.token = 6;
    ja.you = 2;
    ja.host = 1;
    n = nw_join_accept_write(p, NW_MAX_PACKET, &ja);
    CHECK(n > 0 && nw_join_accept_read(p, n, &jb) == 0 && jb.you == 2 && jb.token == 6, "join accept round trip");
    ja.you = 1;
    n = nw_join_accept_write(p, NW_MAX_PACKET, &ja);
    CHECK(nw_join_accept_read(p, n, &jb) != 0, "join accept giving the host's own number refused");
    uint64_t nonce;
    uint32_t hr;
    n = nw_join_refuse_write(p, 64, 5, 0);
    CHECK(nw_join_refuse_read(p, n, &nonce, &hr) != 0, "a 'refusal' with a success code refused");

    /* fuzz: random datagrams and single byte mutations of valid ones never crash the decoders;
     * random ones are (practically) never accepted as headers */
    int accepted = 0;
    for (int it = 0; it < 200000; it++) {
        int len = (int)(rnd() % (NW_MAX_PACKET + 8));
        for (int i = 0; i < len; i++) p[i] = (uint8_t)rnd();
        if (nw_hdr_read(p, len, &h) == 0) accepted++;
        nw_msg_read(p, len, &r);
        nw_session_info_read(p, len, &so);
        NwJoinReq jr;
        nw_join_req_read(p, len, &jr);
        nw_join_accept_read(p, len, &jb);
    }
    CHECK(accepted == 0, "random datagrams accepted as headers: %d", accepted);
    memset(&m, 0, sizeof m);
    m.kind = NM_PLAYER;
    m.id = 0x10000203u;
    m.owner = 2;
    strcpy(m.sname, "name");
    m.n = 16;
    m.data = game;
    int base = nw_msg_write(q, NL_MAX_PAYLOAD, &m);
    for (int it = 0; it < 200000; it++) {
        memcpy(p, q, (size_t)base);
        int len = base;
        p[rnd() % (uint32_t)base] = (uint8_t)rnd();
        if (rnd() & 1) len = (int)(rnd() % (uint32_t)(base + 4));
        if (nw_msg_read(p, len, &r) == 0 && r.kind == NM_PLAYER) {
            /* what passes is well formed */
            CHECK(r.n <= NW_MAX_PDATA && strlen(r.sname) <= NW_MAX_NAME && nw_dpid_ok(r.id), "mutated message passed malformed");
        }
    }
}

/* ---- 2. links over a lossy network ---- */

typedef struct Pkt { uint32_t at; int to; int len; uint8_t d[NW_MAX_PACKET]; } Pkt;
static Pkt* net[100000];
static int nnet;
static int loss_pct = 30, dup_pct = 5, jitter = 40;
static int sent_pkts;

typedef struct End { NetLink l; int id; } End;
static End ends[2];
static uint32_t now;

static void enqueue(int to, const uint8_t* pkt, int len)
{
    if (nnet >= 100000) return;
    Pkt* p = malloc(sizeof *p);
    p->at = now + 10 + rnd() % (uint32_t)(jitter + 1);   /* jitter reorders */
    p->to = to;
    p->len = len;
    memcpy(p->d, pkt, (size_t)len);
    net[nnet++] = p;
}

static void send_cb(void* user, const uint8_t* pkt, int len)
{
    End* e = user;
    sent_pkts++;
    if ((int)(rnd() % 100) < loss_pct) return;
    enqueue(1 - e->id, pkt, len);
    if ((int)(rnd() % 100) < dup_pct) enqueue(1 - e->id, pkt, len);
}

static uint32_t got_next[2];       /* the next sequence number each end expects to receive */
static int order_errors, refuse_odd_once, refused;
static uint32_t unrel_got[2];

static int check_cb(void* user, const uint8_t* payload, int len)
{
    End* e = user;
    (void)e;
    if (len < 5) return -1;
    if (refuse_odd_once && payload[0] == 'R') {
        uint32_t v;
        memcpy(&v, payload + 1, 4);
        if ((v & 1) && !(payload[0] == 'R' && rnd() % 2)) { refused++; return -1; }
    }
    return 0;
}

static void deliver_cb(void* user, int reliable, const uint8_t* payload, int len)
{
    End* e = user;
    (void)len;
    uint32_t v;
    memcpy(&v, payload + 1, 4);
    if (!reliable) { unrel_got[e->id]++; return; }
    if (v != got_next[e->id]) order_errors++;
    got_next[e->id] = v + 1;
}

static void pump(void)
{
    for (int i = 0; i < nnet;) {
        Pkt* p = net[i];
        if ((int32_t)(now - p->at) >= 0) {
            End* e = &ends[p->to];
            NwHdr h;
            if (nw_hdr_read(p->d, p->len, &h) == 0) {
                NetLinkOut o = {send_cb, e};
                nl_on_packet(&e->l, &h, p->d + NW_HDR, p->len - NW_HDR, now, &o, check_cb, deliver_cb, e);
            }
            free(p);
            net[i] = net[--nnet];
        } else i++;
    }
}

static void run_lossy(int count, uint32_t max_ms)
{
    uint64_t token = 0xabcdef0123456789ull;
    now = 1000;
    for (int i = 0; i < 2; i++) {
        ends[i].id = i;
        nl_init(&ends[i].l, (uint8_t)(i + 1), (uint8_t)(2 - i), token, 60000, now);
    }
    got_next[0] = got_next[1] = 0;
    order_errors = 0;
    unrel_got[0] = unrel_got[1] = 0;
    uint32_t next_send[2] = {0, 0};
    uint32_t end = now + max_ms;
    while ((int32_t)(end - now) > 0) {
        for (int i = 0; i < 2; i++) {
            NetLinkOut o = {send_cb, &ends[i]};
            /* bursts of up to 8 a tick: more than the window holds, so messages wait too */
            for (int k = 0; k < 8 && next_send[i] < (uint32_t)count; k++) {
                uint8_t msg[64] = {'R'};
                memcpy(msg + 1, &next_send[i], 4);
                CHECK(nl_send_rel(&ends[i].l, msg, 5 + (int)(rnd() % 50), now, &o) == 0, "reliable send queued");
                next_send[i]++;
            }
            uint8_t u[8] = {'U'};
            nl_send_unrel(&ends[i].l, u, 5, now, &o);
            nl_tick(&ends[i].l, now, &o);
        }
        pump();
        now += 5;
        if (got_next[0] == (uint32_t)count && got_next[1] == (uint32_t)count && nl_idle(&ends[0].l) && nl_idle(&ends[1].l)) break;
    }
    while (nnet) free(net[--nnet]);
}

static void test_links(void)
{
    int count = 3000;
    run_lossy(count, 600000);
    CHECK(got_next[0] == (uint32_t)count && got_next[1] == (uint32_t)count, "all reliable messages arrived (%u / %u of %d)", got_next[0],
          got_next[1], count);
    CHECK(order_errors == 0, "reliable messages in order, once each (%d errors)", order_errors);
    CHECK(nl_idle(&ends[0].l) && nl_idle(&ends[1].l), "everything acked");
    printf("  30%% loss, 5%% duplicates, reordering: %d messages each way in %u simulated ms, %u resends, unreliable %u / %u delivered\n",
           count, now - 1000, ends[0].l.n_resent + ends[1].l.n_resent, unrel_got[0], unrel_got[1]);
    for (int i = 0; i < 2; i++) nl_free(&ends[i].l);

    /* a receiver that refuses some messages at first: they are not acked and come again */
    loss_pct = 10;
    refuse_odd_once = 1;
    refused = 0;
    run_lossy(500, 600000);
    CHECK(refused > 0, "some messages were refused");
    CHECK(got_next[0] == 500 && got_next[1] == 500 && order_errors == 0, "refused messages came again and arrived in order");
    refuse_odd_once = 0;
    for (int i = 0; i < 2; i++) nl_free(&ends[i].l);

    /* acks for a sequence never sent, a wrong token, a wrong peer: refused */
    NetLink l;
    nl_init(&l, 1, 2, 77, 3000, 0);
    NetLinkOut o = {send_cb, &ends[0]};
    NwHdr h = {NW_ACK, 2, 1, 77, 0, 5, 0};
    CHECK(nl_on_packet(&l, &h, NULL, 0, 0, &o, check_cb, deliver_cb, &ends[0]) != 0, "ack of an unsent sequence refused");
    h.ack = 0;
    h.token = 78;
    CHECK(nl_on_packet(&l, &h, NULL, 0, 0, &o, check_cb, deliver_cb, &ends[0]) != 0, "wrong token refused");
    h.token = 77;
    h.from = 3;
    CHECK(nl_on_packet(&l, &h, NULL, 0, 0, &o, check_cb, deliver_cb, &ends[0]) != 0, "wrong peer refused");
    h.from = 2;
    h.type = NW_PING;
    CHECK(nl_on_packet(&l, &h, (const uint8_t*)"abc", 3, 0, &o, check_cb, deliver_cb, &ends[0]) != 0, "short ping refused");
    h.type = NW_ACK;
    CHECK(nl_on_packet(&l, &h, NULL, 0, 0, &o, check_cb, deliver_cb, &ends[0]) == 0, "valid ack taken");
    /* silence: dead after the timeout, alive before */
    CHECK(nl_tick(&l, 2900, &o) == 0, "alive before the timeout");
    CHECK(nl_tick(&l, 3100, &o) == 1, "dead after the timeout");
    nl_free(&l);
    while (nnet) free(net[--nnet]);
}

/* ---- 4. the announce server's datagrams ---- */

static void test_announce(void)
{
    const uint32_t build = 0x524b0200u;
    NwSessionInfo si;
    memset(&si, 0, sizeof si);
    for (int i = 0; i < 16; i++) si.d.guid[i] = (uint8_t)(i + 1);
    strcpy(si.d.name, "Roadkill LAN\x01");
    si.d.flags = 0x44;
    si.d.max_players = 8;
    si.d.cur_players = 2;
    si.d.user[0] = 7; si.d.user[1] = 10; si.d.user[2] = 3; si.d.user[3] = 0xdeadbeefu;
    si.d.has_password = 1;
    si.nplayers = 2;
    strcpy(si.players[0].lname, "Alice");
    strcpy(si.players[1].sname, "Grimm");
    NetAddr local = {0xc0a82805u, 27421};   /* 192.168.40.5 */
    uint8_t reg[sizeof(AnRegister)];
    CHECK(na_register_write(reg, sizeof reg, &si, build, &local) == (int)sizeof(AnRegister), "REGISTER size");
    AnRegister r;
    memcpy(&r, reg, sizeof r);
    CHECK(r.h.magic == AN_MAGIC && r.h.type == AN_REGISTER && r.s.build == build && r.s.user[3] == 0xdeadbeefu, "REGISTER fields");
    const uint8_t* ip = (const uint8_t*)&r.s.local.ip;
    const uint8_t* pt = (const uint8_t*)&r.s.local.port;
    CHECK(ip[0] == 192 && ip[1] == 168 && ip[2] == 40 && ip[3] == 5 && pt[0] == (27421 >> 8) && pt[1] == (27421 & 255),
          "addresses in network order");
    CHECK(!strcmp(r.s.name, "Roadkill LAN?"), "names sent printable");

    /* the server's answer as it builds it: the registered session plus the public address */
    AnSessions m;
    memset(&m, 0, sizeof m);
    m.h.magic = AN_MAGIC; m.h.version = AN_VERSION; m.h.type = AN_SESSIONS; m.h.arg = 1;
    m.count = 1;
    m.s[0] = r.s;
    uint8_t* pip = (uint8_t*)&m.s[0].public_addr.ip;
    uint8_t* ppt = (uint8_t*)&m.s[0].public_addr.port;
    pip[0] = 203; pip[1] = 0; pip[2] = 113; pip[3] = 9; ppt[0] = 0x6b; ppt[1] = 0x1d;
    NaSession out[AN_PAGE];
    int n;
    uint32_t total;
    CHECK(na_sessions_read((const uint8_t*)&m, sizeof m, build, &total, out, &n) == 0 && n == 1 && total == 1, "SESSIONS read");
    CHECK(n == 1 && !memcmp(out[0].s.d.guid, si.d.guid, 16) && out[0].s.d.user[0] == 7 && out[0].s.d.user[1] == 10 &&
          out[0].s.d.user[2] == 3 && out[0].s.d.user[3] == 0xdeadbeefu && out[0].s.d.flags == 0x44 && out[0].s.d.has_password &&
          out[0].s.d.max_players == 8 && out[0].s.d.cur_players == 2 && !memcmp(out[0].s.d.app, NW_TM2_APP, 16),
          "the session description exactly (dwUser1..4, flags, players, password)");
    CHECK(n == 1 && out[0].s.nplayers == 2 && !strcmp(out[0].s.players[0].lname, "Alice") && !strcmp(out[0].s.players[1].sname, "Grimm"),
          "player names");
    CHECK(n == 1 && out[0].pub.ip == 0xcb007109u && out[0].pub.port == 0x6b1d && out[0].local.ip == 0xc0a82805u &&
          out[0].local.port == 27421, "public and local addresses");

    AnSessions bad = m;
    CHECK(na_sessions_read((const uint8_t*)&bad, sizeof bad - 1, build, &total, out, &n) != 0, "short SESSIONS refused");
    bad.h.version = 2;
    CHECK(na_sessions_read((const uint8_t*)&bad, sizeof bad, build, &total, out, &n) != 0, "other version refused");
    bad = m; bad.count = AN_PAGE + 1; bad.h.arg = 100;
    CHECK(na_sessions_read((const uint8_t*)&bad, sizeof bad, build, &total, out, &n) != 0, "count over a page refused");
    bad = m; bad.count = 1; bad.h.arg = 0;
    CHECK(na_sessions_read((const uint8_t*)&bad, sizeof bad, build, &total, out, &n) != 0, "count over the total refused");
    bad = m; memset(bad.s[0].name, 'x', AN_NAME);
    CHECK(na_sessions_read((const uint8_t*)&bad, sizeof bad, build, &total, out, &n) == 0 && n == 0, "name without a NUL: left out");
    bad = m; memset(bad.s[0].players[3], 'y', AN_PLAYER_NAME);
    CHECK(na_sessions_read((const uint8_t*)&bad, sizeof bad, build, &total, out, &n) == 0 && n == 0, "player name without a NUL: left out");
    bad = m; bad.s[0].max_players = 200;
    CHECK(na_sessions_read((const uint8_t*)&bad, sizeof bad, build, &total, out, &n) == 0 && n == 0, "absurd max players: left out");
    bad = m; bad.s[0].cur_players = 9;
    CHECK(na_sessions_read((const uint8_t*)&bad, sizeof bad, build, &total, out, &n) == 0 && n == 0, "more players than max: left out");
    CHECK(na_sessions_read((const uint8_t*)&m, sizeof m, build + 1, &total, out, &n) == 0 && n == 0, "another build: left out");
    bad = m; memset(&bad.s[0].public_addr, 0, sizeof bad.s[0].public_addr);
    CHECK(na_sessions_read((const uint8_t*)&bad, sizeof bad, build, &total, out, &n) == 0 && n == 0, "no public address: left out");
    bad = m; memset(&bad.s[0].public_addr.ip, 0xff, 4);
    CHECK(na_sessions_read((const uint8_t*)&bad, sizeof bad, build, &total, out, &n) == 0 && n == 0, "broadcast address: left out");
    bad = m; ((uint8_t*)&bad.s[0].local.ip)[0] = 239;
    CHECK(na_sessions_read((const uint8_t*)&bad, sizeof bad, build, &total, out, &n) == 0 && n == 1 && out[0].local.ip == 0,
          "multicast local address dropped, the session kept");

    AnIntro in;
    memset(&in, 0, sizeof in);
    in.h.magic = AN_MAGIC; in.h.version = AN_VERSION; in.h.type = AN_INTRO; in.h.arg = 1;
    memcpy(in.guid, si.d.guid, 16);
    in.peer_public = m.s[0].public_addr;
    in.peer_local = r.s.local;
    uint32_t arg;
    uint8_t g[16];
    NetAddr pub, loc;
    CHECK(na_intro_read((const uint8_t*)&in, sizeof in, &arg, g, &pub, &loc) == 0 && arg == 1 && pub.ip == 0xcb007109u &&
          loc.port == 27421, "INTRO read");
    in.h.arg = 2;
    CHECK(na_intro_read((const uint8_t*)&in, sizeof in, &arg, g, &pub, &loc) != 0, "INTRO arg 2 refused");
    in.h.arg = 0;
    CHECK(na_intro_read((const uint8_t*)&in, sizeof in + 1, &arg, g, &pub, &loc) != 0, "long INTRO refused");
    uint32_t type;
    CHECK(na_header_read((const uint8_t*)"RKNP", 4, &type, &arg) != 0, "short header refused");

    /* random datagrams never pass as SESSIONS / INTRO with something unusable inside */
    uint8_t buf[sizeof(AnSessions)];
    for (int it = 0; it < 20000; it++) {
        memcpy(buf, &m, sizeof m);
        for (int k = 0; k < 8; k++) buf[rnd() % sizeof buf] = (uint8_t)rnd();
        if (na_sessions_read(buf, sizeof buf, build, &total, out, &n) == 0)
            for (int i = 0; i < n; i++)
                CHECK(out[i].s.d.max_players <= 8 && out[i].s.d.cur_players <= out[i].s.d.max_players && strlen(out[i].s.d.name) < AN_NAME &&
                      na_addr_usable(&out[i].pub) && out[i].s.nplayers <= 8, "mutated SESSIONS passed something unusable");
    }
}

int main(void)
{
    printf("test_net: wire format\n");
    test_wire();
    printf("test_net: links\n");
    test_links();
    printf("test_net: announce server datagrams\n");
    test_announce();
    printf("test_net: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
