/*
 * xbox_net.c: host/net/net_sock.h on the original Xbox's network stack, XNet (port code).
 *
 * XNet runs "secure" by default: only other Xboxes over its encrypted links. Online play with
 * PCs needs it started with XNET_STARTUP_BYPASS_SECURITY, which the devkit library (xnet.lib,
 * linked here) honours; then its Winsock is plain UDP like the desktop's. It is started at boot
 * (xbox_net_start, from xbox_main.c) so the console's DHCP is done by the time the game opens a
 * socket. Names are looked up with XNetDnsLookup; the console's own address (the "local" one the
 * announce server passes to players on the same network) is XNetGetTitleXnAddr's.
 */
#include <xtl.h>
#include <string.h>
#include <stdlib.h>

#include "host/net/net_sock.h"

/* XNet's sockets don't take a sendto from one thread while another waits in select on the same
 * socket: the send is lost (the provider's socket thread waits in net_sock_recv while the game's
 * thread sends its car state). So a socket's sends go through a queue that its receiving thread
 * sends from, between short waits. */
#define SENDQ 64
typedef struct { NetAddr to; int len; char data[1500]; } Outgoing;
struct NetSock {
    SOCKET fd;
    CRITICAL_SECTION lock;
    Outgoing q[SENDQ];
    int head, count;
    volatile LONG receiving;   /* a thread is (or has been) receiving: sends queue for it */
    unsigned sent, failed, dropped;
};

static volatile LONG started;   /* 0 not yet, 1 up, -1 failed */

/* on its own thread (xbox_net_start): a network that hangs XNet's start can't stall the game */
static DWORD WINAPI net_start_thread(LPVOID unused)
{
    XNetStartupParams p;
    WSADATA w;
    int r;
    DWORD link = XNetGetEthernetLinkStatus();
    memset(&p, 0, sizeof p);
    p.cfgSizeOfStruct = sizeof p;   /* this XDK's own size: XNetStartup ignores any other, the flag too */
    p.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
    /* plain IP (DHCP, DNS, UDP to PCs) needs more than secure system link: the pool and queues
     * warthog-xbox (Chupathingy's console port) uses in this mode, XNet's own key / security
     * tables unused */
    p.cfgPrivatePoolSizeInPages = 64;
    p.cfgEnetReceiveQueueLength = 16;
    p.cfgIpFragMaxSimultaneous = 16;
    p.cfgIpFragMaxPacketDiv256 = 32;
    p.cfgSockMaxSockets = 64;
    p.cfgKeyRegMax = 0;
    p.cfgSecRegMax = 0;
    (void)unused;
    rk_log("network: ethernet link %s; starting XNet", link & XNET_ETHERNET_LINK_ACTIVE ? "up" : "down");
    r = XNetStartup(&p);
    if (r != 0) { rk_log("network: XNetStartup failed (%d)", r); started = -1; return 0; }
    rk_log("network: XNetStartup done; WSAStartup");
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) { rk_log("network: WSAStartup failed"); started = -1; return 0; }
    started = 1;
    rk_log("network: XNet started (security bypassed for PCs)");
    return 0;
}

void xbox_net_start(void)
{
    DWORD id;
    HANDLE h = CreateThread(NULL, 0x8000, net_start_thread, NULL, 0, &id);
    if (h) CloseHandle(h);
    else rk_log("network: no thread to start XNet");
}

/* the console's address: XNet gets it from DHCP (or the dashboard's static settings) */
static int title_ip(uint32_t* ip, int wait_ms)
{
    XNADDR x;
    DWORD st;
    int waited = 0;
    for (;;) {
        st = XNetGetTitleXnAddr(&x);
        if (st != XNET_GET_XNADDR_PENDING) break;
        if (waited >= wait_ms) return -1;
        Sleep(100);
        waited += 100;
    }
    if (st & XNET_GET_XNADDR_NONE || !x.ina.s_addr) return -1;
    *ip = ntohl(x.ina.s_addr);
    return 0;
}

int net_sock_init(void)
{
    uint32_t ip;
    static int told;
    int waited = 0;
    while (started == 0 && waited < 15000) { Sleep(100); waited += 100; }   /* XNet still starting */
    if (started != 1) return -1;
    if (title_ip(&ip, 10000) != 0) {
        if (!told) rk_log("network: the console has no IP address (cable, DHCP or the dashboard's settings)");
        told = 1;
        return -1;
    }
    if (!told) rk_log("network: the console is %u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
    told = 1;
    return 0;
}

static void to_sa(const NetAddr* a, struct sockaddr_in* sa)
{
    memset(sa, 0, sizeof *sa);
    sa->sin_family = AF_INET;
    sa->sin_port = htons(a->port);
    sa->sin_addr.s_addr = htonl(a->ip);
}

NetSock* net_sock_open(uint16_t port, int tries, uint16_t* bound)
{
    int t;
    if (net_sock_init() != 0) return NULL;
    if (tries < 1) tries = 1;
    for (t = 0; t < tries; t++) {
        SOCKET fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        BOOL one = TRUE;
        NetAddr any;
        struct sockaddr_in sa;
        if (fd == INVALID_SOCKET) return NULL;
        setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (const char*)&one, sizeof one);
        any.ip = 0;
        any.port = (uint16_t)(port ? port + t : 0);
        to_sa(&any, &sa);
        if (bind(fd, (struct sockaddr*)&sa, sizeof sa) == 0) {
            int sl = sizeof sa;
            NetSock* s;
            if (getsockname(fd, (struct sockaddr*)&sa, &sl) == 0 && bound) *bound = ntohs(sa.sin_port);
            s = (NetSock*)calloc(1, sizeof *s);
            if (!s) { closesocket(fd); return NULL; }
            s->fd = fd;
            InitializeCriticalSection(&s->lock);
            return s;
        }
        closesocket(fd);
        if (!port) break;
    }
    return NULL;
}

void net_sock_close(NetSock* s)
{
    if (!s) return;
    closesocket(s->fd);
    DeleteCriticalSection(&s->lock);
    free(s);
}

static int send_now(NetSock* s, const NetAddr* to, const void* p, int len)
{
    struct sockaddr_in sa;
    to_sa(to, &sa);
    if (sendto(s->fd, (const char*)p, len, 0, (struct sockaddr*)&sa, sizeof sa) == len) { s->sent++; return 0; }
    if (s->failed++ < 10) rk_log("network: sendto %d bytes failed (%d)", len, WSAGetLastError());
    return -1;
}

/* sends what other threads queued (the receiving thread, outside select) */
static void flush(NetSock* s)
{
    for (;;) {
        Outgoing o;
        EnterCriticalSection(&s->lock);
        if (!s->count) { LeaveCriticalSection(&s->lock); return; }
        o.to = s->q[s->head].to;
        o.len = s->q[s->head].len;
        memcpy(o.data, s->q[s->head].data, (size_t)o.len);
        s->head = (s->head + 1) % SENDQ;
        s->count--;
        LeaveCriticalSection(&s->lock);
        send_now(s, &o.to, o.data, o.len);
    }
}

int net_sock_send(NetSock* s, const NetAddr* to, const void* p, int len)
{
    Outgoing* o;
    if (!s || len <= 0 || len > (int)sizeof o->data) return -1;
    if (!s->receiving) return send_now(s, to, p, len);   /* nobody waits on the socket yet */
    EnterCriticalSection(&s->lock);
    if (s->count == SENDQ) {
        LeaveCriticalSection(&s->lock);
        if (s->dropped++ < 10) rk_log("network: send queue full, a datagram dropped");
        return -1;
    }
    o = &s->q[(s->head + s->count) % SENDQ];
    o->to = *to;
    o->len = len;
    memcpy(o->data, p, (size_t)len);
    s->count++;
    LeaveCriticalSection(&s->lock);
    return 0;
}

int net_sock_recv(NetSock* s, NetAddr* from, void* p, int cap, int timeout_ms)
{
    fd_set rd;
    struct timeval tv;
    char buf[2048];
    struct sockaddr_in sa;
    int sl = sizeof sa, want, n, r;
    if (!s) return -1;
    s->receiving = 1;
    /* wait in short steps, sending what's queued between them: a game send waits 2 ms at most */
    for (;;) {
        int step = timeout_ms < 2 ? timeout_ms : 2;
        flush(s);
        FD_ZERO(&rd);
        FD_SET(s->fd, &rd);
        tv.tv_sec = 0;
        tv.tv_usec = step * 1000;
        r = select(0, &rd, NULL, NULL, &tv);
        if (r != 0) break;
        timeout_ms -= step;
        if (timeout_ms <= 0) { flush(s); return 0; }
    }
    if (r < 0) return -1;
    /* one byte more than the caller takes: a longer datagram shows up as cap + 1 */
    want = cap + 1 <= (int)sizeof buf ? cap + 1 : (int)sizeof buf;
    n = recvfrom(s->fd, buf, want, 0, (struct sockaddr*)&sa, &sl);
    if (n < 0 || sa.sin_family != AF_INET) return 0;
    if (from) {
        from->ip = ntohl(sa.sin_addr.s_addr);
        from->port = ntohs(sa.sin_port);
    }
    if (n > cap) n = cap + 1;
    memcpy(p, buf, (size_t)(n > cap ? cap : n));
    return n;
}

int net_addr_resolve(const char* text, uint16_t defport, NetAddr* out)
{
    char host[256];
    char* colon;
    uint16_t port = defport;
    unsigned a, b, c, d;
    if (!text || !*text || net_sock_init() != 0) return -1;
    strncpy(host, text, sizeof host - 1);
    host[sizeof host - 1] = 0;
    colon = strrchr(host, ':');
    if (colon) {
        char* end;
        long v = strtol(colon + 1, &end, 10);
        if (*end || v <= 0 || v > 65535) return -1;
        port = (uint16_t)v;
        *colon = 0;
    }
    if (sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 && a < 256 && b < 256 && c < 256 && d < 256) {
        out->ip = a << 24 | b << 16 | c << 8 | d;
        out->port = port;
        return 0;
    }
    {
        XNDNS* dns = NULL;
        WSAEVENT ev = WSACreateEvent();
        int r = -1;
        if (ev == WSA_INVALID_EVENT) return -1;
        if (XNetDnsLookup(host, ev, &dns) == 0 && dns) {
            WaitForSingleObject(ev, 10000);
            if (dns->iStatus == 0 && dns->cina > 0) {
                out->ip = ntohl(dns->aina[0].s_addr);
                out->port = port;
                r = 0;
            } else rk_log("network: no address for %s (status %d)", host, dns->iStatus);
            XNetDnsRelease(dns);
        }
        WSACloseEvent(ev);
        return r;
    }
}

int net_sock_local_ip(const NetAddr* toward, uint32_t* ip)
{
    (void)toward;
    return started == 1 ? title_ip(ip, 0) : -1;
}

uint32_t net_now_ms(void) { return (uint32_t)GetTickCount(); }

void net_random(void* p, size_t n) { XNetRandom((BYTE*)p, (UINT)n); }
