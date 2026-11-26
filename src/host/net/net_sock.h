/*
 * net_sock.h: the UDP sockets under Roadkill's network play (port code, not from the original).
 * The one place that knows the operating system's socket API: POSIX sockets on macOS / Linux,
 * Winsock on Windows. Another backend (the Xbox: XNet's Winsock-like sockets) replaces
 * net_sock.c's #if branch and nothing above it. IPv4 only for now.
 *
 * Every function is safe to call from any thread; a socket is used by one receiving thread and
 * any number of sending ones (sendto on a UDP socket needs no lock).
 */
#ifndef ROADKILL_NET_SOCK_H
#define ROADKILL_NET_SOCK_H

#include <stddef.h>
#include <stdint.h>

/* an IPv4 endpoint, host byte order */
typedef struct NetAddr {
    uint32_t ip;
    uint16_t port;
} NetAddr;

typedef struct NetSock NetSock;

/* 0 when the platform has sockets (WSAStartup on Windows); -1 when it has none */
int net_sock_init(void);

/* a UDP socket bound to port (0: any) on every interface, broadcast allowed; tries port,
 * port + 1, ... port + tries - 1 when it is taken. NULL when none could be opened. *bound gets
 * the port it has. */
NetSock* net_sock_open(uint16_t port, int tries, uint16_t* bound);
void net_sock_close(NetSock* s);

/* one datagram; 0 sent, -1 failed */
int net_sock_send(NetSock* s, const NetAddr* to, const void* p, int len);
/* waits up to timeout_ms for one datagram: its length (> 0), 0 on timeout, -1 on an error.
 * A datagram longer than cap is cut (its length is then cap + 1, so the caller can drop it). */
int net_sock_recv(NetSock* s, NetAddr* from, void* p, int cap, int timeout_ms);

/* "host", "host:port", "1.2.3.4:port" (port defaults to defport); 0 resolved, -1 not. May block
 * on a name lookup: call it from the provider's own thread. */
int net_addr_resolve(const char* text, uint16_t defport, NetAddr* out);
int net_addr_eq(const NetAddr* a, const NetAddr* b);
int net_addr_is_loopback(const NetAddr* a);
/* "1.2.3.4:5" into out */
const char* net_addr_str(const NetAddr* a, char* out, size_t cap);

#define NET_ADDR_BROADCAST 0xffffffffu
#define NET_ADDR_LOOPBACK  0x7f000001u

/* milliseconds on a monotonic clock */
uint32_t net_now_ms(void);
/* n bytes for tokens and nonces (the system's random source; a time / address mix where there
 * is none) */
void net_random(void* p, size_t n);

#endif
