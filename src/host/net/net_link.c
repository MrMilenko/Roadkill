/*
 * net_link.c: the per peer channels of net_link.h (port code, not from the original).
 */
#include "host/net/net_link.h"

#include <stdlib.h>
#include <string.h>

#define RTO_MIN 100u
#define RTO_MAX 2000u
#define RTO_INIT 300u
#define PING_EVERY 1000u

static int sdiff(uint16_t a, uint16_t b) { return (int16_t)(uint16_t)(a - b); }
static int tdiff(uint32_t a, uint32_t b) { return (int32_t)(a - b); }

void nl_init(NetLink* l, uint8_t me, uint8_t peer, uint64_t token, uint32_t timeout_ms, uint32_t now)
{
    memset(l, 0, sizeof *l);
    l->me = me;
    l->peer = peer;
    l->token = token;
    l->rto = RTO_INIT;
    l->timeout_ms = timeout_ms;
    l->last_heard = now;
    l->last_ping = now - PING_EVERY;   /* ping at the first tick: opens NAT mappings both ways */
}

void nl_free(NetLink* l)
{
    for (int i = 0; i < NL_WINDOW; i++) {
        free(l->flight[i]);
        free(l->early[i]);
        l->flight[i] = l->early[i] = NULL;
    }
    while (l->wait_head) {
        NlMsg* m = l->wait_head;
        l->wait_head = m->next;
        free(m);
    }
    l->wait_tail = NULL;
    l->nwait = 0;
}

static uint32_t ack_bits(const NetLink* l)
{
    uint32_t b = 0;
    for (int i = 0; i < 32; i++) {
        uint16_t s = (uint16_t)(l->rcv_next + 1 + i);
        const NlMsg* m = l->early[s % NL_WINDOW];
        if (m && m->seq == s) b |= 1u << i;
    }
    return b;
}

static void emit(NetLink* l, uint8_t type, uint16_t seq, const uint8_t* payload, int len, const NetLinkOut* out)
{
    uint8_t pkt[NW_MAX_PACKET];
    if (len < 0 || len > NL_MAX_PAYLOAD) return;
    NwHdr h = {type, l->me, l->peer, l->token, seq, l->rcv_next, ack_bits(l)};
    nw_hdr_write(pkt, &h);
    if (len) memcpy(pkt + NW_HDR, payload, (size_t)len);
    out->send(out->user, pkt, NW_HDR + len);
    l->ack_due = 0;   /* every packet carries the acks */
}

static void transmit(NetLink* l, NlMsg* m, uint32_t now, const NetLinkOut* out)
{
    uint32_t back = l->rto;
    for (int i = 1; i < m->tries && back < RTO_MAX; i++) back *= 2;
    if (back > RTO_MAX) back = RTO_MAX;
    m->tries++;
    m->sent_at = now;
    m->due = now + back;
    emit(l, NW_REL, m->seq, m->data, m->len, out);
}

/* moves waiting messages into the window */
static void fill(NetLink* l, uint32_t now, const NetLinkOut* out)
{
    while (l->wait_head && sdiff(l->snd_next, l->snd_una) < NL_WINDOW) {
        NlMsg* m = l->wait_head;
        l->wait_head = m->next;
        if (!l->wait_head) l->wait_tail = NULL;
        l->nwait--;
        m->next = NULL;
        m->seq = l->snd_next++;
        l->flight[m->seq % NL_WINDOW] = m;
        l->n_sent++;
        transmit(l, m, now, out);
    }
}

int nl_send_rel(NetLink* l, const uint8_t* payload, int len, uint32_t now, const NetLinkOut* out)
{
    if (l->broken || len <= 0 || len > NL_MAX_PAYLOAD) return -1;
    if (l->nwait >= NL_MAX_QUEUED) {
        l->broken = 1;   /* the peer has not acked for thousands of messages: nl_tick reports it dead */
        return -1;
    }
    NlMsg* m = calloc(1, sizeof *m);
    if (!m) return -1;
    m->len = (uint16_t)len;
    memcpy(m->data, payload, (size_t)len);
    if (l->wait_tail) l->wait_tail->next = m; else l->wait_head = m;
    l->wait_tail = m;
    l->nwait++;
    fill(l, now, out);
    return 0;
}

int nl_send_unrel(NetLink* l, const uint8_t* payload, int len, uint32_t now, const NetLinkOut* out)
{
    (void)now;
    if (l->broken || len <= 0 || len > NL_MAX_PAYLOAD) return -1;
    emit(l, NW_UNREL, l->u_seq++, payload, len, out);
    l->n_unrel_out++;
    return 0;
}

void nl_send_bye(NetLink* l, const NetLinkOut* out) { emit(l, NW_BYE, 0, NULL, 0, out); }

static void rtt_sample(NetLink* l, uint32_t sample)
{
    if (sample > 10000u) return;
    if (!l->srtt) {
        l->srtt = sample ? sample : 1;
        l->rttvar = sample / 2;
    } else {
        uint32_t d = l->srtt > sample ? l->srtt - sample : sample - l->srtt;
        l->rttvar = (3 * l->rttvar + d) / 4;
        l->srtt = (7 * l->srtt + sample) / 8;
        if (!l->srtt) l->srtt = 1;
    }
    uint32_t r = l->srtt + 4 * l->rttvar;
    l->rto = r < RTO_MIN ? RTO_MIN : r > RTO_MAX ? RTO_MAX : r;
}

static void acked(NetLink* l, uint16_t seq, uint32_t now)
{
    NlMsg** slot = &l->flight[seq % NL_WINDOW];
    if (!*slot || (*slot)->seq != seq) return;
    if ((*slot)->tries == 1) rtt_sample(l, now - (*slot)->sent_at);   /* Karn: not a resent one */
    free(*slot);
    *slot = NULL;
}

/* the peer's acks; -1 when they ack something never sent */
static int on_acks(NetLink* l, const NwHdr* h, uint32_t now)
{
    if (sdiff(h->ack, l->snd_next) > 0) return -1;
    while (sdiff(h->ack, l->snd_una) > 0) {
        acked(l, l->snd_una, now);
        l->snd_una++;
    }
    for (int i = 0; i < 32; i++) {
        if (!(h->ack_bits & (1u << i))) continue;
        uint16_t s = (uint16_t)(h->ack + 1 + i);
        if (sdiff(s, l->snd_next) >= 0) break;
        acked(l, s, now);
    }
    while (l->snd_una != l->snd_next && !l->flight[l->snd_una % NL_WINDOW]) l->snd_una++;
    return 0;
}

int nl_on_packet(NetLink* l, const NwHdr* h, const uint8_t* payload, int len, uint32_t now, const NetLinkOut* out,
                 NetLinkCheck check, NetLinkDeliver deliver, void* user)
{
    if (h->token != l->token || h->from != l->peer || h->to != l->me) return -1;
    /* the payload's shape first: a packet that does not parse changes nothing */
    switch (h->type) {
    case NW_ACK:
    case NW_BYE:
        if (len != 0) return -1;
        break;
    case NW_PING:
    case NW_PONG:
        if (len != 4) return -1;
        break;
    case NW_REL:
    case NW_UNREL:
        if (len <= 0) return -1;
        break;
    default:
        return -1;
    }
    if (on_acks(l, h, now) != 0) return -1;
    l->last_heard = now;
    l->n_recv++;
    switch (h->type) {
    case NW_BYE:
        return 1;
    case NW_PING: {
        emit(l, NW_PONG, 0, payload, 4, out);
        break;
    }
    case NW_PONG: {
        uint32_t t;
        if (nw_u32_read(payload, len, &t) == 0) rtt_sample(l, now - t);
        break;
    }
    case NW_UNREL:
        if (check && check(user, payload, len) != 0) { l->n_dropped++; return -1; }
        l->n_unrel_in++;
        deliver(user, 0, payload, len);
        break;
    case NW_REL: {
        int d = sdiff(h->seq, l->rcv_next);
        if (d < 0) { l->n_dup++; l->ack_due = 1; break; }    /* already delivered: ack again */
        if (d >= NL_WINDOW) { l->n_dropped++; break; }        /* past the window: it comes again */
        if (check && check(user, payload, len) != 0) { l->n_dropped++; return -1; }
        if (d > 0) {
            NlMsg** slot = &l->early[h->seq % NL_WINDOW];
            if (*slot && (*slot)->seq == h->seq) { l->n_dup++; l->ack_due = 1; break; }
            NlMsg* m = *slot ? *slot : calloc(1, sizeof *m);
            if (!m) break;
            m->seq = h->seq;
            m->len = (uint16_t)len;
            memcpy(m->data, payload, (size_t)len);
            *slot = m;
            l->ack_due = 1;
            break;
        }
        l->rcv_next++;
        l->ack_due = 1;
        deliver(user, 1, payload, len);
        for (;;) {
            NlMsg** slot = &l->early[l->rcv_next % NL_WINDOW];
            if (!*slot || (*slot)->seq != l->rcv_next) break;
            NlMsg* m = *slot;
            *slot = NULL;
            l->rcv_next++;
            deliver(user, 1, m->data, m->len);
            free(m);
        }
        break;
    }
    }
    fill(l, now, out);
    return 0;
}

int nl_tick(NetLink* l, uint32_t now, const NetLinkOut* out)
{
    for (int i = 0; i < NL_WINDOW; i++) {
        NlMsg* m = l->flight[i];
        if (m && tdiff(now, m->due) >= 0) {
            l->n_resent++;
            transmit(l, m, now, out);
        }
    }
    if (tdiff(now, l->last_ping) >= (int)PING_EVERY) {
        uint8_t t[4];
        nw_u32_write(t, 4, now);
        emit(l, NW_PING, 0, t, 4, out);
        l->last_ping = now;
    }
    if (l->ack_due) emit(l, NW_ACK, 0, NULL, 0, out);
    return l->broken || tdiff(now, l->last_heard) > (int)l->timeout_ms;
}

int nl_idle(const NetLink* l) { return l->snd_una == l->snd_next && !l->wait_head; }
