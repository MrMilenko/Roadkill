/*
 * net_log.c: net_log.h (port code, not from the original).
 */
#include "host/net/net_log.h"

#include <stdarg.h>
#include <stdio.h>

#include "host/net/net_sock.h"
#include "host/settings/settings.h"

int net_log_on(void)
{
    static int on = -1;
    if (on < 0) on = settings_bool("debug.net_log", 0);
    return on;
}

void net_log(const char* fmt, ...)
{
    if (!net_log_on()) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    fprintf(stderr, "roadkill net: %s\n", buf);
}

void net_drop(const char* fmt, ...)
{
    static uint32_t window;
    static int lines, skipped;
    uint32_t now = net_now_ms();
    if ((int32_t)(now - window) >= 1000) {
        if (skipped) fprintf(stderr, "roadkill net: (%d more refused datagrams)\n", skipped);
        window = now;
        lines = skipped = 0;
    }
    if (lines >= 5 && !net_log_on()) { skipped++; return; }
    lines++;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    fprintf(stderr, "roadkill net: refused: %s\n", buf);
}
