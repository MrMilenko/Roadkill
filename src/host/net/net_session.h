/*
 * net_session.h: the network provider under the DirectPlay host (port code, not from the
 * original): one session at a time, its machines (peers) and players, the socket thread, and
 * the queue of what the game's Receive gets. It never touches game memory: src/host/net/dplay.c
 * turns these calls into the DirectPlay semantics the game sees.
 *
 * Topology: a full mesh, as DirectPlay's peer to peer sessions. The host gives every joiner the
 * other members' addresses (NM_PEER) and a snapshot of the players; every machine sends its own
 * players' messages to every other one directly. Host migration (DPSESSION_MIGRATEHOST): when the
 * host goes, every member makes the same choice, the lowest remaining peer number (the oldest
 * member); that machine gets DPSYS_HOST.
 *
 * Players: a machine makes its own players' DPIDs from its peer number, so CreatePlayer needs
 * no round trip. A peer that is not heard from for [net] timeout seconds, sends a BYE, or is
 * reported gone by the host is removed with DPSYS_DESTROYPLAYERORGROUP for each of its players
 * (the game has no timeouts of its own: barriers would wait forever).
 *
 * Threads: every function locks the provider's own mutex; the socket thread runs host code only.
 */
#ifndef ROADKILL_NET_SESSION_H
#define ROADKILL_NET_SESSION_H

#include <stdint.h>

#include "host/net/net_wire.h"

/* DirectPlay results (dplay.h: MAKE_DPHRESULT(code) = 0x88770000 + code) */
#define DP_OK                  0u
#define DPERR_ACCESSDENIED     0x8877000au
#define DPERR_ALREADYINITIALIZED 0x88770005u
#define DPERR_BUFFERTOOSMALL   0x8877001eu
#define DPERR_CANTADDPLAYER    0x88770028u
#define DPERR_CANTCREATEPLAYER 0x8877003cu
#define DPERR_GENERIC          0x80004005u
#define DPERR_INVALIDFLAGS     0x88770078u
#define DPERR_INVALIDOBJECT    0x88770082u
#define DPERR_INVALIDPARAMS    0x80070057u
#define DPERR_INVALIDPLAYER    0x88770096u
#define DPERR_NOCONNECTION     0x887700aau
#define DPERR_NOMESSAGES       0x887700beu
#define DPERR_NOSESSIONS       0x887700dcu
#define DPERR_SENDTOOBIG       0x887700e6u
#define DPERR_TIMEOUT          0x887700f0u
#define DPERR_UNAVAILABLE      0x887700fau
#define DPERR_UNSUPPORTED      0x80004001u
#define DPERR_BUSY             0x8877010eu
#define DPERR_NONEWPLAYERS     0x8877014au
#define DPERR_INVALIDPASSWORD  0x88770154u

/* DPSESSION_* flags the provider acts on */
#define NS_NEWPLAYERSDISABLED  0x00000001u
#define NS_PASSWORDREQUIRED    0x00000400u

typedef struct NsPlayer {
    uint32_t id;
    uint8_t owner;          /* peer number */
    int local;
    uint32_t flags;
    char sname[NW_MAX_NAME + 1], lname[NW_MAX_NAME + 1];
    uint16_t n;
    uint8_t data[NW_MAX_PDATA];
} NsPlayer;

/* what Receive hands the game */
enum { NS_EV_GAME = 1, NS_EV_CREATE, NS_EV_DESTROY, NS_EV_HOST, NS_EV_DATA };
typedef struct NsEvent {
    struct NsEvent* next;
    int kind;
    int reliable;
    uint32_t from, to;      /* NS_EV_GAME: the players; system messages: from 0, to the local player */
    uint32_t id;            /* the player a system message is about */
    uint32_t cur_players;   /* NS_EV_CREATE: players in the session after it */
    char sname[NW_MAX_NAME + 1], lname[NW_MAX_NAME + 1];
    uint16_t n;
    uint8_t data[NW_MAX_GAME];
} NsEvent;

/* 1 when the platform has sockets (the provider can be offered); opens nothing */
int ns_available(void);
/* opens the socket and starts the socket thread (once); 0 ok */
int ns_start(void);
/* the port in use (0 before ns_start) */
uint16_t ns_port(void);
/* the gameplay settings every machine of a session must share (ENUM / JOIN check it) */
uint32_t ns_compat(void);
/* the protocol and gameplay signature the announce server lists by (its `build`) */
uint32_t ns_build(void);

/* host a session (Open with DPOPEN_CREATE): d's guid is made here */
uint32_t ns_host(NwSessionDesc* d, const char* password);
/* join the session guid found by discovery (Open with DPOPEN_JOIN); blocks until the host's
 * snapshot is in, up to the timeout; the caller releases the runtime's host lock around it */
uint32_t ns_join(const uint8_t guid[16], const char* password, NwSessionDesc* out);
/* leave (Close): our players go with us */
void ns_close(void);
int ns_in_session(void);
int ns_is_host(void);
int ns_session_desc(NwSessionDesc* out);   /* 0 in a session */
void ns_enable_new_players(int on);

uint32_t ns_create_player(const char* sname, const char* lname, const uint8_t* data, int n, uint32_t flags, uint32_t* id);
uint32_t ns_destroy_player(uint32_t id);
uint32_t ns_set_player_data(uint32_t id, const uint8_t* data, int n, uint32_t flags);
uint32_t ns_get_player(uint32_t id, NsPlayer* out);
int ns_players(NsPlayer* out, int max);   /* all players of the session, local ones first */

/* Send: flags & 1 = DPSEND_GUARANTEED. Never DPERR_BUSY (the game would spin). */
uint32_t ns_send(uint32_t from, uint32_t to, uint32_t flags, const uint8_t* data, int n);
/* the oldest event into out; remove = 0 leaves it queued. 0 when there was one. */
int ns_recv(NsEvent* out, int remove);
int ns_queued(void);

#endif
