/*
 * net_log.h: the network provider's log lines on stderr (port code, not from the original).
 * net_log: only with TM2_NET_LOG=1 ([debug] net_log). net_drop: a refused datagram, always
 * logged but at most a few lines a second (a flood of junk cannot flood the log).
 */
#ifndef ROADKILL_NET_LOG_H
#define ROADKILL_NET_LOG_H

int net_log_on(void);
void net_log(const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
void net_drop(const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

#endif
