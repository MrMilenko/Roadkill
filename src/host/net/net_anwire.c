/*
 * net_anwire.c: net_anwire.h (port code, not from the original).
 */
#include "host/net/net_anwire.h"

#include <string.h>

static AnAddr to_an(const NetAddr* a)
{
    AnAddr r;
    uint8_t* ip = (uint8_t*)&r.ip;
    uint8_t* port = (uint8_t*)&r.port;
    ip[0] = (uint8_t)(a->ip >> 24); ip[1] = (uint8_t)(a->ip >> 16); ip[2] = (uint8_t)(a->ip >> 8); ip[3] = (uint8_t)a->ip;
    port[0] = (uint8_t)(a->port >> 8); port[1] = (uint8_t)a->port;
    r.pad = 0;
    return r;
}

static NetAddr from_an(const AnAddr* a)
{
    const uint8_t* ip = (const uint8_t*)&a->ip;
    const uint8_t* port = (const uint8_t*)&a->port;
    NetAddr r = {(uint32_t)ip[0] << 24 | (uint32_t)ip[1] << 16 | (uint32_t)ip[2] << 8 | ip[3], (uint16_t)(port[0] << 8 | port[1])};
    return r;
}

int na_addr_usable(const NetAddr* a)
{
    uint32_t top = a->ip >> 24;
    return a->ip && a->port && a->ip != 0xffffffffu && top != 0 && (top < 224 || top > 239);
}

static void header(AnHeader* h, uint32_t type, uint32_t arg)
{
    h->magic = AN_MAGIC;
    h->version = AN_VERSION;
    h->type = type;
    h->arg = arg;
}

/* printable ASCII, cut to the field (NUL padded) */
static void put_text(char* dst, size_t cap, const char* src)
{
    memset(dst, 0, cap);
    for (size_t i = 0; i + 1 < cap && src[i]; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[i] = c >= 0x20 && c < 0x7f ? (char)c : '?';
    }
}

/* a field from the server: NUL inside it, then forced printable; -1 without a NUL */
static int get_text(char* dst, size_t dcap, const char* src, size_t scap)
{
    const char* z = memchr(src, 0, scap);
    if (!z) return -1;
    size_t l = (size_t)(z - src);
    if (l >= dcap) l = dcap - 1;
    for (size_t i = 0; i < l; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[i] = c >= 0x20 && c < 0x7f ? (char)c : '?';
    }
    dst[l] = 0;
    return 0;
}

int na_register_write(uint8_t* p, int cap, const NwSessionInfo* si, uint32_t build, const NetAddr* local)
{
    AnRegister m;
    if (cap < (int)sizeof m) return 0;
    memset(&m, 0, sizeof m);
    header(&m.h, AN_REGISTER, 0);
    AnSession* s = &m.s;
    memcpy(s->guid, si->d.guid, 16);
    put_text(s->name, sizeof s->name, si->d.name);
    s->flags = si->d.flags;
    s->max_players = si->d.max_players > AN_MAX_PLAYERS ? AN_MAX_PLAYERS : si->d.max_players;
    s->cur_players = si->d.cur_players > s->max_players ? s->max_players : si->d.cur_players;
    for (int i = 0; i < 4; i++) s->user[i] = si->d.user[i];
    s->password = si->d.has_password ? 1 : 0;
    s->build = build;
    for (int i = 0; i < si->nplayers && i < AN_MAX_PLAYERS; i++)
        put_text(s->players[i], AN_PLAYER_NAME, si->players[i].lname[0] ? si->players[i].lname : si->players[i].sname);
    if (local && local->ip) s->local = to_an(local);
    memcpy(p, &m, sizeof m);
    return (int)sizeof m;
}

int na_withdraw_write(uint8_t* p, int cap, const uint8_t guid[16])
{
    AnWithdraw m;
    if (cap < (int)sizeof m) return 0;
    header(&m.h, AN_WITHDRAW, 0);
    memcpy(m.guid, guid, 16);
    memcpy(p, &m, sizeof m);
    return (int)sizeof m;
}

int na_list_write(uint8_t* p, int cap, uint32_t page, uint32_t build)
{
    AnList m;
    if (cap < (int)sizeof m) return 0;
    header(&m.h, AN_LIST, page);
    m.build = build;
    memcpy(p, &m, sizeof m);
    return (int)sizeof m;
}

int na_join_write(uint8_t* p, int cap, const uint8_t guid[16], const NetAddr* local)
{
    AnJoin m;
    if (cap < (int)sizeof m) return 0;
    memset(&m, 0, sizeof m);
    header(&m.h, AN_JOIN, 0);
    memcpy(m.guid, guid, 16);
    if (local && local->ip) m.local = to_an(local);
    memcpy(p, &m, sizeof m);
    return (int)sizeof m;
}

int na_header_read(const uint8_t* p, int len, uint32_t* type, uint32_t* arg)
{
    AnHeader h;
    if (!p || len < (int)sizeof h) return -1;
    memcpy(&h, p, sizeof h);
    if (h.magic != AN_MAGIC || h.version != AN_VERSION) return -1;
    *type = h.type;
    *arg = h.arg;
    return 0;
}

static const uint8_t zero16[16];

static int session_from(const AnSession* a, uint32_t build, NaSession* o)
{
    memset(o, 0, sizeof *o);
    if (a->build != build || !memcmp(a->guid, zero16, 16)) return -1;
    if (a->max_players < 1 || a->max_players > AN_MAX_PLAYERS || a->cur_players > a->max_players || a->password > 1) return -1;
    NwSessionDesc* d = &o->s.d;
    memcpy(d->guid, a->guid, 16);
    memcpy(d->app, NW_TM2_APP, 16);
    if (get_text(d->name, sizeof d->name, a->name, sizeof a->name) != 0) return -1;
    d->flags = a->flags;
    d->max_players = a->max_players;
    d->cur_players = a->cur_players;
    for (int i = 0; i < 4; i++) d->user[i] = a->user[i];
    d->has_password = (uint8_t)a->password;
    for (int i = 0; i < AN_MAX_PLAYERS; i++) {
        char nm[AN_PLAYER_NAME];
        if (get_text(nm, sizeof nm, a->players[i], AN_PLAYER_NAME) != 0) return -1;
        if (!nm[0]) continue;
        NwPlayerName* pn = &o->s.players[o->s.nplayers++];
        pn->id = (uint32_t)o->s.nplayers;   /* the join screen only shows names */
        memcpy(pn->sname, nm, sizeof nm);
        memcpy(pn->lname, nm, sizeof nm);
    }
    o->pub = from_an(&a->public_addr);
    o->local = from_an(&a->local);
    if (!na_addr_usable(&o->pub)) return -1;
    if (!na_addr_usable(&o->local)) o->local.ip = o->local.port = 0;
    return 0;
}

int na_sessions_read(const uint8_t* p, int len, uint32_t build, uint32_t* total, NaSession out[AN_PAGE], int* n)
{
    AnSessions m;
    *n = 0;
    if (len != (int)sizeof m) return -1;
    memcpy(&m, p, sizeof m);
    if (m.h.magic != AN_MAGIC || m.h.version != AN_VERSION || m.h.type != AN_SESSIONS) return -1;
    if (m.count > AN_PAGE || m.count > m.h.arg) return -1;
    *total = m.h.arg;
    for (uint32_t i = 0; i < m.count; i++)
        if (session_from(&m.s[i], build, &out[*n]) == 0) (*n)++;
    return 0;
}

int na_intro_read(const uint8_t* p, int len, uint32_t* arg, uint8_t guid[16], NetAddr* pub, NetAddr* local)
{
    AnIntro m;
    if (len != (int)sizeof m) return -1;
    memcpy(&m, p, sizeof m);
    if (m.h.magic != AN_MAGIC || m.h.version != AN_VERSION || m.h.type != AN_INTRO || m.h.arg > 1) return -1;
    *arg = m.h.arg;
    memcpy(guid, m.guid, 16);
    *pub = from_an(&m.peer_public);
    *local = from_an(&m.peer_local);
    if (!na_addr_usable(pub)) return -1;
    if (!na_addr_usable(local)) local->ip = local->port = 0;
    return 0;
}

int na_registered_read(const uint8_t* p, int len, NetAddr* pub)
{
    AnRegistered m;
    if (len != (int)sizeof m) return -1;
    memcpy(&m, p, sizeof m);
    if (m.h.magic != AN_MAGIC || m.h.version != AN_VERSION || m.h.type != AN_REGISTERED) return -1;
    *pub = from_an(&m.public_addr);
    return na_addr_usable(pub) ? 0 : -1;
}
