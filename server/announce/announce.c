/*
 * announce.c: Roadkill's announce / rendezvous server (announce_proto.h). One UDP socket, no
 * state on disk. Sessions live while their host keeps registering (AN_TTL); a client lists them
 * and asks to join one, and the server introduces the two so they can reach each other through
 * their NATs. It never relays game traffic and never sees a password.
 *
 *   announce [port]          (default AN_PORT; ANNOUNCE_LOG=1 logs every request)
 *
 * Limits against abuse: datagrams of the wrong size or version are dropped; per source IP at most
 * AN_PER_IP sessions and AN_RATE requests a second; names are forced to printable ASCII.
 */
#include "announce_proto.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define AN_MAX 256        /* sessions */
#define AN_TTL 15         /* seconds without a REGISTER before a session goes */
#define AN_PER_IP 4       /* sessions per host IP */
#define AN_RATE 20        /* requests a second per source IP */
#define AN_RATE_SLOTS 1024

typedef struct { int used; time_t seen; AnSession s; struct sockaddr_in from; } Entry;
static Entry sessions[AN_MAX];
static struct { uint32_t ip; time_t sec; int n; } rate[AN_RATE_SLOTS];
static int logging;

static void lg(const char* fmt, ...)
{
    if (!logging) return;
    va_list ap;
    va_start(ap, fmt);
    char t[32];
    time_t now = time(NULL);
    strftime(t, sizeof t, "%Y-%m-%d %H:%M:%S", gmtime(&now));
    fprintf(stderr, "%s ", t);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static const char* addr_str(const struct sockaddr_in* a)
{
    static char b[64];
    snprintf(b, sizeof b, "%s:%u", inet_ntoa(a->sin_addr), ntohs(a->sin_port));
    return b;
}

static int rate_ok(uint32_t ip, time_t now)
{
    unsigned k = (ip * 2654435761u) % AN_RATE_SLOTS;
    if (rate[k].ip != ip || rate[k].sec != now) { rate[k].ip = ip; rate[k].sec = now; rate[k].n = 0; }
    return ++rate[k].n <= AN_RATE;
}

static void clean_text(char* s, size_t n)
{
    s[n - 1] = 0;
    for (size_t i = 0; i < n && s[i]; i++)
        if ((unsigned char)s[i] < 0x20 || (unsigned char)s[i] > 0x7e) s[i] = '?';
}

static void sanitize(AnSession* s)
{
    clean_text(s->name, sizeof s->name);
    for (int i = 0; i < AN_MAX_PLAYERS; i++) clean_text(s->players[i], sizeof s->players[i]);
    if (s->max_players > AN_MAX_PLAYERS) s->max_players = AN_MAX_PLAYERS;
    if (s->cur_players > s->max_players) s->cur_players = s->max_players;
    s->password = s->password ? 1 : 0;
}

static Entry* find(const uint8_t guid[16])
{
    for (int i = 0; i < AN_MAX; i++)
        if (sessions[i].used && !memcmp(sessions[i].s.guid, guid, 16)) return &sessions[i];
    return NULL;
}

static void expire(time_t now)
{
    for (int i = 0; i < AN_MAX; i++)
        if (sessions[i].used && now - sessions[i].seen > AN_TTL) {
            lg("expired %s (%s)", sessions[i].s.name, addr_str(&sessions[i].from));
            sessions[i].used = 0;
        }
}

static void header(AnHeader* h, uint32_t type, uint32_t arg)
{
    h->magic = AN_MAGIC;
    h->version = AN_VERSION;
    h->type = type;
    h->arg = arg;
}

static AnAddr addr_of(const struct sockaddr_in* a)
{
    AnAddr r = {a->sin_addr.s_addr, a->sin_port, 0};
    return r;
}

static void on_register(int fd, const AnRegister* m, const struct sockaddr_in* from, time_t now)
{
    AnSession s = m->s;
    sanitize(&s);
    Entry* e = find(s.guid);
    if (e && e->from.sin_addr.s_addr != from->sin_addr.s_addr) return;   /* someone else's guid */
    if (!e) {
        int same = 0;
        for (int i = 0; i < AN_MAX; i++)
            if (sessions[i].used && sessions[i].from.sin_addr.s_addr == from->sin_addr.s_addr) same++;
        if (same >= AN_PER_IP) { lg("refused %s: too many sessions", addr_str(from)); return; }
        for (int i = 0; i < AN_MAX && !e; i++) if (!sessions[i].used) e = &sessions[i];
        if (!e) { lg("refused %s: full", addr_str(from)); return; }
        lg("new session \"%s\" from %s (mode %u, arena %u)", s.name, addr_str(from), s.user[0], s.user[1]);
    }
    e->used = 1;
    e->seen = now;
    e->from = *from;
    s.public_addr = addr_of(from);
    e->s = s;
    AnRegistered r;
    header(&r.h, AN_REGISTERED, 0);
    r.public_addr = addr_of(from);
    sendto(fd, &r, sizeof r, 0, (const struct sockaddr*)from, sizeof *from);
}

static void on_withdraw(const AnWithdraw* m, const struct sockaddr_in* from)
{
    Entry* e = find(m->guid);
    if (e && e->from.sin_addr.s_addr == from->sin_addr.s_addr && e->from.sin_port == from->sin_port) {
        lg("withdrawn \"%s\"", e->s.name);
        e->used = 0;
    }
}

static void on_list(int fd, const AnList* m, const struct sockaddr_in* from)
{
    Entry* match[AN_MAX];
    uint32_t n = 0;
    for (int i = 0; i < AN_MAX; i++)
        if (sessions[i].used && sessions[i].s.build == m->build) match[n++] = &sessions[i];
    AnSessions r;
    memset(&r, 0, sizeof r);
    header(&r.h, AN_SESSIONS, n);
    uint32_t first = m->h.arg * AN_PAGE;
    for (uint32_t k = first; k < n && r.count < AN_PAGE; k++) r.s[r.count++] = match[k]->s;
    sendto(fd, &r, sizeof r, 0, (const struct sockaddr*)from, sizeof *from);
}

static void on_join(int fd, const AnJoin* m, const struct sockaddr_in* from)
{
    Entry* e = find(m->guid);
    if (!e) return;
    AnIntro to_host, to_client;
    header(&to_host.h, AN_INTRO, 0);
    memcpy(to_host.guid, m->guid, 16);
    to_host.peer_public = addr_of(from);
    to_host.peer_local = m->local;
    header(&to_client.h, AN_INTRO, 1);
    memcpy(to_client.guid, m->guid, 16);
    to_client.peer_public = e->s.public_addr;
    to_client.peer_local = e->s.local;
    sendto(fd, &to_host, sizeof to_host, 0, (const struct sockaddr*)&e->from, sizeof e->from);
    sendto(fd, &to_client, sizeof to_client, 0, (const struct sockaddr*)from, sizeof *from);
    lg("intro %s -> \"%s\"", addr_str(from), e->s.name);
}

int main(int argc, char** argv)
{
    int port = argc > 1 ? atoi(argv[1]) : AN_PORT;
    logging = getenv("ANNOUNCE_LOG") && *getenv("ANNOUNCE_LOG") == '1';
    signal(SIGPIPE, SIG_IGN);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket"); return 1; }
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr*)&a, sizeof a) != 0) { perror("bind"); return 1; }
    struct timeval tv = {1, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    fprintf(stderr, "roadkill announce: UDP port %d, protocol %d\n", port, AN_VERSION);
    union { AnHeader h; AnRegister reg; AnWithdraw wd; AnList list; AnJoin join; uint8_t raw[1500]; } m;
    for (;;) {
        struct sockaddr_in from;
        socklen_t fl = sizeof from;
        ssize_t n = recvfrom(fd, &m, sizeof m, 0, (struct sockaddr*)&from, &fl);
        time_t now = time(NULL);
        expire(now);
        if (n < (ssize_t)sizeof(AnHeader)) continue;
        if (m.h.magic != AN_MAGIC || m.h.version != AN_VERSION) continue;
        if (!rate_ok(from.sin_addr.s_addr, now)) continue;
        switch (m.h.type) {
        case AN_REGISTER: if (n == sizeof(AnRegister)) on_register(fd, &m.reg, &from, now); break;
        case AN_WITHDRAW: if (n == sizeof(AnWithdraw)) on_withdraw(&m.wd, &from); break;
        case AN_LIST: if (n == sizeof(AnList)) on_list(fd, &m.list, &from); break;
        case AN_JOIN: if (n == sizeof(AnJoin)) on_join(fd, &m.join, &from); break;
        default: break;
        }
    }
}
