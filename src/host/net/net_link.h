/*
 * net_link.h: one link between two peers of a session (port code, not from the original): the
 * reliable ordered channel (DPSEND_GUARANTEED game messages and every provider message), the
 * unreliable one (game messages sent with flags 0), keepalive and the RTT estimate.
 *
 * A link only builds and parses datagrams; the caller owns the socket (out->send), the clock
 * (now, milliseconds) and the lock, so tools/test_net.c drives two links over a lossy pipe.
 *
 * Reliable: every message gets a 16 bit sequence; up to NL_WINDOW are in flight, the rest wait in
 * order. The receiver delivers in sequence order only, keeps up to NL_WINDOW early ones, and
 * acks with (next expected, a bit per early one) on every packet it sends plus an ACK packet
 * when it has nothing else to send. Resend after the RTO (srtt + 4 rttvar, 100 ms .. 2 s,
 * doubled per retry). Keepalive: a PING every second; a link that has heard nothing valid for
 * timeout_ms is dead (the session then destroys that peer's players: the game has no timeouts).
 */
#ifndef ROADKILL_NET_LINK_H
#define ROADKILL_NET_LINK_H

#include <stdint.h>

#include "host/net/net_wire.h"

#define NL_WINDOW 32
#define NL_MAX_QUEUED 4096   /* reliable messages waiting behind the window before the link gives up */
#define NL_MAX_PAYLOAD (NW_MAX_PACKET - NW_HDR)

typedef struct NetLinkOut {
    void (*send)(void* user, const uint8_t* pkt, int len);
    void* user;
} NetLinkOut;

typedef struct NlMsg {
    struct NlMsg* next;
    uint16_t seq;
    uint16_t len;
    uint8_t tries;
    uint32_t sent_at, due;
    uint8_t data[NL_MAX_PAYLOAD];
} NlMsg;

typedef struct NetLink {
    uint8_t me, peer;
    uint64_t token;
    /* reliable, sending */
    uint16_t snd_next;               /* the next sequence to give out */
    uint16_t snd_una;                /* the oldest not acked */
    NlMsg* flight[NL_WINDOW];        /* by seq % NL_WINDOW: sent, not acked */
    NlMsg* wait_head, *wait_tail;    /* not sent yet (window full) */
    int nwait;
    /* reliable, receiving */
    uint16_t rcv_next;
    NlMsg* early[NL_WINDOW];         /* by seq % NL_WINDOW: arrived before rcv_next */
    int ack_due;                     /* something to ack */
    /* unreliable */
    uint16_t u_seq;
    /* timing */
    uint32_t srtt, rttvar, rto;
    uint32_t last_heard, last_ping;
    uint32_t timeout_ms;
    int broken;                      /* the send queue overflowed */
    /* counters (TM2_NET_LOG) */
    uint32_t n_sent, n_resent, n_recv, n_dup, n_dropped, n_unrel_out, n_unrel_in;
} NetLink;

/* the receiver's view of a reliable / unreliable payload: return 0 to accept it, -1 to refuse
 * it (a refused reliable message is not acked, so it is never delivered) */
typedef int (*NetLinkCheck)(void* user, const uint8_t* payload, int len);
typedef void (*NetLinkDeliver)(void* user, int reliable, const uint8_t* payload, int len);

void nl_init(NetLink* l, uint8_t me, uint8_t peer, uint64_t token, uint32_t timeout_ms, uint32_t now);
void nl_free(NetLink* l);   /* the queued messages */

/* queue a reliable message (len <= NL_MAX_PAYLOAD); 0 queued, -1 refused (too long, broken link) */
int nl_send_rel(NetLink* l, const uint8_t* payload, int len, uint32_t now, const NetLinkOut* out);
/* send an unreliable one now */
int nl_send_unrel(NetLink* l, const uint8_t* payload, int len, uint32_t now, const NetLinkOut* out);
/* a BYE (unreliable, the caller sends it a few times) */
void nl_send_bye(NetLink* l, const NetLinkOut* out);

/* one datagram from this peer (header already read and checked against token / peers).
 * Returns 0 when it was valid, -1 when it was refused. Reliable messages come out of deliver in
 * order; a BYE returns 1. */
int nl_on_packet(NetLink* l, const NwHdr* h, const uint8_t* payload, int len, uint32_t now, const NetLinkOut* out,
                 NetLinkCheck check, NetLinkDeliver deliver, void* user);

/* resends, acks, pings; returns 1 when the link is dead (timeout or broken) */
int nl_tick(NetLink* l, uint32_t now, const NetLinkOut* out);

/* nothing in flight or waiting */
int nl_idle(const NetLink* l);

#endif
