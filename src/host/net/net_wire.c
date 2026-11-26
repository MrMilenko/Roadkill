/*
 * net_wire.c: the encoders and checked decoders of net_wire.h (port code, not from the original).
 */
#include "host/net/net_wire.h"

#include <string.h>

/* ---- a bounded writer / reader: any overrun sets err and every later step does nothing ---- */

typedef struct { uint8_t* p; int cap, pos, err; } W;
typedef struct { const uint8_t* p; int len, pos, err; } R;

static void w_bytes(W* w, const void* s, int n)
{
    if (w->err || n < 0 || w->pos + n > w->cap) { w->err = 1; return; }
    memcpy(w->p + w->pos, s, (size_t)n);
    w->pos += n;
}
static void w_u8(W* w, uint8_t v) { w_bytes(w, &v, 1); }
static void w_u16(W* w, uint16_t v) { uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; w_bytes(w, b, 2); }
static void w_u32(W* w, uint32_t v)
{
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    w_bytes(w, b, 4);
}
static void w_u64(W* w, uint64_t v) { w_u32(w, (uint32_t)v); w_u32(w, (uint32_t)(v >> 32)); }
static void w_str(W* w, const char* s)
{
    size_t l = 0;
    while (l < NW_MAX_NAME && s[l]) l++;
    w_u8(w, (uint8_t)l);
    w_bytes(w, s, (int)l);
}
static int w_done(W* w) { return w->err ? 0 : w->pos; }

static const uint8_t* r_bytes(R* r, int n)
{
    if (r->err || n < 0 || r->pos + n > r->len) { r->err = 1; return NULL; }
    const uint8_t* s = r->p + r->pos;
    r->pos += n;
    return s;
}
static uint8_t r_u8(R* r) { const uint8_t* s = r_bytes(r, 1); return s ? s[0] : 0; }
static uint16_t r_u16(R* r) { const uint8_t* s = r_bytes(r, 2); return s ? (uint16_t)(s[0] | s[1] << 8) : 0; }
static uint32_t r_u32(R* r)
{
    const uint8_t* s = r_bytes(r, 4);
    return s ? (uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24 : 0;
}
static uint64_t r_u64(R* r) { uint64_t lo = r_u32(r); return lo | (uint64_t)r_u32(r) << 32; }
static void r_copy(R* r, void* d, int n) { const uint8_t* s = r_bytes(r, n); if (s) memcpy(d, s, (size_t)n); }
/* a name: at most NW_MAX_NAME bytes, no NUL inside, stored NUL terminated */
static void r_str(R* r, char* out)
{
    out[0] = 0;
    int l = r_u8(r);
    if (l > NW_MAX_NAME) { r->err = 1; return; }
    const uint8_t* s = r_bytes(r, l);
    if (!s) return;
    if (memchr(s, 0, (size_t)l)) { r->err = 1; return; }
    memcpy(out, s, (size_t)l);
    out[l] = 0;
}
/* the whole payload must be used: trailing bytes are a malformed packet */
static int r_end(R* r) { return r->err || r->pos != r->len ? -1 : 0; }

/* ---- header ---- */

void nw_hdr_write(uint8_t* p, const NwHdr* h)
{
    W w = {p, NW_HDR, 0, 0};
    w_u32(&w, NW_MAGIC);
    w_u8(&w, NW_VERSION);
    w_u8(&w, h->type);
    w_u8(&w, h->from);
    w_u8(&w, h->to);
    w_u64(&w, h->token);
    w_u16(&w, h->seq);
    w_u16(&w, h->ack);
    w_u32(&w, h->ack_bits);
}

const uint8_t NW_TM2_APP[16] = {0x40, 0x01, 0x19, 0xea, 0x70, 0xce, 0xcf, 0x11, 0xa1, 0x36, 0x68, 0xc6, 0xff, 0xc0, 0x00, 0x00};

static int type_known(uint8_t t) { return (t >= NW_ENUM_REQ && t <= NW_PUNCH) || (t >= NW_REL && t <= NW_BYE); }

int nw_hdr_read(const uint8_t* p, int len, NwHdr* h)
{
    if (!p || len < NW_HDR || len > NW_MAX_PACKET) return -1;
    R r = {p, NW_HDR, 0, 0};
    if (r_u32(&r) != NW_MAGIC) return -1;
    if (r_u8(&r) != NW_VERSION) return -1;
    h->type = r_u8(&r);
    h->from = r_u8(&r);
    h->to = r_u8(&r);
    h->token = r_u64(&r);
    h->seq = r_u16(&r);
    h->ack = r_u16(&r);
    h->ack_bits = r_u32(&r);
    if (r.err || !type_known(h->type)) return -1;
    /* session-less packets carry no token and no peers; session packets need all three */
    int sessionless = h->type <= NW_PUNCH;
    if (sessionless && (h->token || h->from || h->to)) return -1;
    if (!sessionless && (!h->token || !h->from || !h->to || h->from >= 0xff || h->to >= 0xff)) return -1;
    return 0;
}

/* ---- session-less payloads ---- */

int nw_enum_req_write(uint8_t* p, int cap, const uint8_t app[16], uint32_t compat)
{
    W w = {p, cap, 0, 0};
    w_bytes(&w, app, 16);
    w_u32(&w, compat);
    while (!w.err && w.pos < NW_ENUM_REQ_SIZE) w_u8(&w, 0);
    return w_done(&w);
}

int nw_enum_req_read(const uint8_t* p, int len, uint8_t app[16], uint32_t* compat)
{
    R r = {p, len, 0, 0};
    if (len != NW_ENUM_REQ_SIZE) return -1;
    r_copy(&r, app, 16);
    *compat = r_u32(&r);
    r.pos = r.len;   /* the padding */
    return r_end(&r);
}

static void w_desc(W* w, const NwSessionDesc* d)
{
    w_bytes(w, d->guid, 16);
    w_bytes(w, d->app, 16);
    w_u32(w, d->flags);
    w_u32(w, d->max_players);
    w_u32(w, d->cur_players);
    for (int i = 0; i < 4; i++) w_u32(w, d->user[i]);
    w_str(w, d->name);
    w_u8(w, d->has_password ? 1 : 0);
}

static void r_desc(R* r, NwSessionDesc* d)
{
    memset(d, 0, sizeof *d);
    r_copy(r, d->guid, 16);
    r_copy(r, d->app, 16);
    d->flags = r_u32(r);
    d->max_players = r_u32(r);
    d->cur_players = r_u32(r);
    for (int i = 0; i < 4; i++) d->user[i] = r_u32(r);
    r_str(r, d->name);
    d->has_password = r_u8(r);
    if (d->has_password > 1 || d->max_players > NW_MAX_PLAYERS || d->cur_players > NW_MAX_PLAYERS) r->err = 1;
}

int nw_session_info_write(uint8_t* p, int cap, const NwSessionInfo* s)
{
    W w = {p, cap, 0, 0};
    w_desc(&w, &s->d);
    w_u32(&w, s->compat);
    int np = s->nplayers < 0 ? 0 : s->nplayers > NW_MAX_PLAYERS ? NW_MAX_PLAYERS : s->nplayers;
    int at = w.pos;
    w_u8(&w, 0);
    if (w.err) return 0;
    int put = 0;
    for (int i = 0; i < np; i++) {
        W t = w;
        w_u32(&t, s->players[i].id);
        w_str(&t, s->players[i].sname);
        w_str(&t, s->players[i].lname);
        if (t.err) break;   /* the rest does not fit: the join screen shows fewer names */
        w = t;
        put++;
    }
    p[at] = (uint8_t)put;
    return w_done(&w);
}

int nw_session_info_read(const uint8_t* p, int len, NwSessionInfo* s)
{
    R r = {p, len, 0, 0};
    memset(s, 0, sizeof *s);
    r_desc(&r, &s->d);
    s->compat = r_u32(&r);
    int np = r_u8(&r);
    if (np > NW_MAX_PLAYERS) return -1;
    for (int i = 0; i < np && !r.err; i++) {
        s->players[i].id = r_u32(&r);
        r_str(&r, s->players[i].sname);
        r_str(&r, s->players[i].lname);
        if (!nw_dpid_ok(s->players[i].id)) r.err = 1;
    }
    s->nplayers = np;
    return r_end(&r);
}

int nw_join_req_write(uint8_t* p, int cap, const NwJoinReq* j)
{
    W w = {p, cap, 0, 0};
    w_u64(&w, j->nonce);
    w_bytes(&w, j->app, 16);
    w_bytes(&w, j->guid, 16);
    w_u32(&w, j->compat);
    w_str(&w, j->password);
    w_u32(&w, j->local_ip);
    w_u16(&w, j->local_port);
    return w_done(&w);
}

int nw_join_req_read(const uint8_t* p, int len, NwJoinReq* j)
{
    R r = {p, len, 0, 0};
    memset(j, 0, sizeof *j);
    j->nonce = r_u64(&r);
    r_copy(&r, j->app, 16);
    r_copy(&r, j->guid, 16);
    j->compat = r_u32(&r);
    r_str(&r, j->password);
    j->local_ip = r_u32(&r);
    j->local_port = r_u16(&r);
    if (!j->nonce || (j->local_ip && !j->local_port) || j->local_ip == 0xffffffffu) return -1;
    return r_end(&r);
}

int nw_join_accept_write(uint8_t* p, int cap, const NwJoinAccept* j)
{
    W w = {p, cap, 0, 0};
    w_u64(&w, j->nonce);
    w_u64(&w, j->token);
    w_u8(&w, j->you);
    w_u8(&w, j->host);
    w_u8(&w, j->next_peer);
    w_desc(&w, &j->d);
    return w_done(&w);
}

int nw_join_accept_read(const uint8_t* p, int len, NwJoinAccept* j)
{
    R r = {p, len, 0, 0};
    memset(j, 0, sizeof *j);
    j->nonce = r_u64(&r);
    j->token = r_u64(&r);
    j->you = r_u8(&r);
    j->host = r_u8(&r);
    j->next_peer = r_u8(&r);
    r_desc(&r, &j->d);
    if (!j->nonce || !j->token || !j->you || !j->host || j->you == j->host || j->you == 0xff || j->host == 0xff)
        return -1;
    return r_end(&r);
}

int nw_join_refuse_write(uint8_t* p, int cap, uint64_t nonce, uint32_t hr)
{
    W w = {p, cap, 0, 0};
    w_u64(&w, nonce);
    w_u32(&w, hr);
    return w_done(&w);
}

int nw_join_refuse_read(const uint8_t* p, int len, uint64_t* nonce, uint32_t* hr)
{
    R r = {p, len, 0, 0};
    *nonce = r_u64(&r);
    *hr = r_u32(&r);
    /* a refusal must be a failure code: a forged "success" would not end a join anyway */
    if (!(*hr & 0x80000000u)) return -1;
    return r_end(&r);
}

int nw_u32_write(uint8_t* p, int cap, uint32_t v) { W w = {p, cap, 0, 0}; w_u32(&w, v); return w_done(&w); }
int nw_u32_read(const uint8_t* p, int len, uint32_t* v) { R r = {p, len, 0, 0}; *v = r_u32(&r); return r_end(&r); }

/* ---- session messages ---- */

int nw_msg_write(uint8_t* p, int cap, const NwMsg* m)
{
    W w = {p, cap, 0, 0};
    w_u8(&w, m->kind);
    switch (m->kind) {
    case NM_GAME:
        if (!m->n || m->n > NW_MAX_GAME) return 0;
        w_u32(&w, m->from);
        w_u32(&w, m->to);
        w_u16(&w, m->n);
        w_bytes(&w, m->data, m->n);
        break;
    case NM_PEER:
        w_u8(&w, m->peer);
        w_u32(&w, m->ip);
        w_u16(&w, m->port);
        w_u32(&w, m->lip);
        w_u16(&w, m->lport);
        break;
    case NM_PEER_GONE:
        w_u8(&w, m->peer);
        break;
    case NM_PLAYER:
        if (m->n > NW_MAX_PDATA) return 0;
        w_u32(&w, m->id);
        w_u8(&w, m->owner);
        w_u32(&w, m->flags);
        w_str(&w, m->sname);
        w_str(&w, m->lname);
        w_u16(&w, m->n);
        w_bytes(&w, m->data, m->n);
        break;
    case NM_PLAYER_GONE:
        w_u32(&w, m->id);
        break;
    case NM_PLAYER_DATA:
        if (m->n > NW_MAX_PDATA) return 0;
        w_u32(&w, m->id);
        w_u16(&w, m->n);
        w_bytes(&w, m->data, m->n);
        break;
    case NM_SESSION:
        w_desc(&w, &m->d);
        break;
    case NM_READY:
        break;
    default:
        return 0;
    }
    return w_done(&w);
}

int nw_msg_read(const uint8_t* p, int len, NwMsg* m)
{
    R r = {p, len, 0, 0};
    memset(m, 0, sizeof *m);
    m->kind = r_u8(&r);
    switch (m->kind) {
    case NM_GAME:
        m->from = r_u32(&r);
        m->to = r_u32(&r);
        m->n = r_u16(&r);
        if (!nw_dpid_ok(m->from) || m->to == 0xffffffffu || !m->n || m->n > NW_MAX_GAME) return -1;
        m->data = r_bytes(&r, m->n);
        break;
    case NM_PEER:
        m->peer = r_u8(&r);
        m->ip = r_u32(&r);
        m->port = r_u16(&r);
        m->lip = r_u32(&r);
        m->lport = r_u16(&r);
        if (!m->peer || m->peer == 0xff || !m->port || !m->ip || m->ip == 0xffffffffu) return -1;
        if (m->lip == 0xffffffffu || (m->lip && !m->lport)) return -1;
        break;
    case NM_PEER_GONE:
        m->peer = r_u8(&r);
        if (!m->peer || m->peer == 0xff) return -1;
        break;
    case NM_PLAYER:
        m->id = r_u32(&r);
        m->owner = r_u8(&r);
        m->flags = r_u32(&r);
        r_str(&r, m->sname);
        r_str(&r, m->lname);
        m->n = r_u16(&r);
        if (!nw_dpid_ok(m->id) || !m->owner || m->owner == 0xff || m->n > NW_MAX_PDATA) return -1;
        m->data = r_bytes(&r, m->n);
        break;
    case NM_PLAYER_GONE:
        m->id = r_u32(&r);
        if (!nw_dpid_ok(m->id)) return -1;
        break;
    case NM_PLAYER_DATA:
        m->id = r_u32(&r);
        m->n = r_u16(&r);
        if (!nw_dpid_ok(m->id) || m->n > NW_MAX_PDATA) return -1;
        m->data = r_bytes(&r, m->n);
        break;
    case NM_SESSION:
        r_desc(&r, &m->d);
        break;
    case NM_READY:
        break;
    default:
        return -1;
    }
    return r_end(&r);
}
