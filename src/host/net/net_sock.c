/*
 * net_sock.c: net_sock.h on POSIX sockets (macOS, Linux) and Winsock (Windows). Port code, not
 * from the original. The original Xbox's (XNet) is port/xbox/xbox_net.c.
 */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _CRT_RAND_S
#include "host/net/net_sock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(ROADKILL_XBOX)

/* the original Xbox: port/xbox/xbox_net.c (XNet) */

#else

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET sock_t;
#define BAD_SOCK INVALID_SOCKET
#define close_sock closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef int sock_t;
#define BAD_SOCK (-1)
#define close_sock close
#endif

struct NetSock {
    sock_t fd;
};

int net_sock_init(void)
{
#ifdef _WIN32
    static int done, ok;
    if (!done) {
        WSADATA w;
        ok = WSAStartup(MAKEWORD(2, 2), &w) == 0;
        done = 1;
    }
    return ok ? 0 : -1;
#else
    return 0;
#endif
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
    if (net_sock_init() != 0) return NULL;
    if (tries < 1) tries = 1;
    for (int t = 0; t < tries; t++) {
        sock_t fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd == BAD_SOCK) return NULL;
        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (const char*)&one, sizeof one);
#ifndef _WIN32
        fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
        NetAddr any = {0, (uint16_t)(port ? port + t : 0)};
        struct sockaddr_in sa;
        to_sa(&any, &sa);
        if (bind(fd, (struct sockaddr*)&sa, sizeof sa) == 0) {
            socklen_t sl = sizeof sa;
            if (getsockname(fd, (struct sockaddr*)&sa, &sl) == 0 && bound) *bound = ntohs(sa.sin_port);
            NetSock* s = malloc(sizeof *s);
            if (!s) { close_sock(fd); return NULL; }
            s->fd = fd;
            return s;
        }
        close_sock(fd);
        if (!port) break;
    }
    return NULL;
}

void net_sock_close(NetSock* s)
{
    if (!s) return;
    close_sock(s->fd);
    free(s);
}

int net_sock_send(NetSock* s, const NetAddr* to, const void* p, int len)
{
    if (!s || len <= 0) return -1;
    struct sockaddr_in sa;
    to_sa(to, &sa);
    return sendto(s->fd, (const char*)p, len, 0, (struct sockaddr*)&sa, sizeof sa) == len ? 0 : -1;
}

int net_sock_recv(NetSock* s, NetAddr* from, void* p, int cap, int timeout_ms)
{
    if (!s) return -1;
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(s->fd, &rd);
    struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    int r = select((int)s->fd + 1, &rd, NULL, NULL, &tv);
    if (r == 0) return 0;
    if (r < 0) {
#ifndef _WIN32
        if (errno == EINTR) return 0;
#endif
        return -1;
    }
    /* one byte more than the caller takes: a longer datagram shows up as cap + 1 */
    static const int extra = 1;
    char buf[2048];
    int want = cap + extra <= (int)sizeof buf ? cap + extra : (int)sizeof buf;
    struct sockaddr_in sa;
    socklen_t sl = sizeof sa;
    int n = (int)recvfrom(s->fd, buf, want, 0, (struct sockaddr*)&sa, &sl);
    if (n < 0) {
#ifdef _WIN32
        int e = WSAGetLastError();
        /* a port unreachable answer to an earlier send, or a datagram bigger than buf */
        if (e == WSAECONNRESET || e == WSAEMSGSIZE) return 0;
#endif
        return 0;
    }
    if (sa.sin_family != AF_INET) return 0;
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
    if (!text || !*text || net_sock_init() != 0) return -1;
    char host[256];
    snprintf(host, sizeof host, "%s", text);
    uint16_t port = defport;
    char* colon = strrchr(host, ':');
    if (colon) {
        char* end;
        long v = strtol(colon + 1, &end, 10);
        if (*end || v <= 0 || v > 65535) return -1;
        port = (uint16_t)v;
        *colon = 0;
    }
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) return -1;
    const struct sockaddr_in* sa = (const struct sockaddr_in*)res->ai_addr;
    out->ip = ntohl(sa->sin_addr.s_addr);
    out->port = port;
    freeaddrinfo(res);
    return 0;
}

int net_sock_local_ip(const NetAddr* toward, uint32_t* ip)
{
    if (net_sock_init() != 0) return -1;
    sock_t fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == BAD_SOCK) return -1;
    NetAddr t = *toward;
    if (!t.ip || t.ip == NET_ADDR_BROADCAST) { t.ip = 0xc0000201u; t.port = 9; }   /* 192.0.2.1: any routed address */
    struct sockaddr_in sa;
    to_sa(&t, &sa);
    int r = -1;
    if (connect(fd, (struct sockaddr*)&sa, sizeof sa) == 0) {
        socklen_t sl = sizeof sa;
        if (getsockname(fd, (struct sockaddr*)&sa, &sl) == 0 && sa.sin_addr.s_addr) {
            *ip = ntohl(sa.sin_addr.s_addr);
            r = 0;
        }
    }
    close_sock(fd);
    return r;
}

uint32_t net_now_ms(void)
{
#ifdef _WIN32
    return (uint32_t)GetTickCount();
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u);
#endif
}

void net_random(void* p, size_t n)
{
#if defined(_WIN32)
    uint8_t* b = p;
    for (size_t i = 0; i < n; i += 4) {
        unsigned int v = 0;
        rand_s(&v);
        for (size_t k = 0; k < 4 && i + k < n; k++) b[i + k] = (uint8_t)(v >> (8 * k));
    }
#elif defined(__APPLE__)
    arc4random_buf(p, n);
#else
    uint8_t* b = p;
    FILE* f = fopen("/dev/urandom", "rb");
    size_t got = f ? fread(b, 1, n, f) : 0;
    if (f) fclose(f);
    if (got < n) {
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        uint64_t x = (uint64_t)t.tv_nsec * 0x9e3779b97f4a7c15ull ^ (uint64_t)(uintptr_t)p ^ (uint64_t)t.tv_sec;
        for (size_t i = got; i < n; i++) {
            x ^= x << 13; x ^= x >> 7; x ^= x << 17;
            b[i] = (uint8_t)x;
        }
    }
#endif
}

#endif

int net_addr_eq(const NetAddr* a, const NetAddr* b) { return a->ip == b->ip && a->port == b->port; }
int net_addr_is_loopback(const NetAddr* a) { return (a->ip >> 24) == 127; }

const char* net_addr_str(const NetAddr* a, char* out, size_t cap)
{
    snprintf(out, cap, "%u.%u.%u.%u:%u", a->ip >> 24, (a->ip >> 16) & 255, (a->ip >> 8) & 255, a->ip & 255, a->port);
    return out;
}
