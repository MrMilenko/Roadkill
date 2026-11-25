/*
 * mt2_ui_wait.c: scene wait commands 0x0046ace8 / 0x0046b15c. See mt2_ui_wait.h.
 */
#include "mt2_ui_wait.h"

#include <string.h>

static int32_t rd(const uint8_t* p, uint32_t o) { int32_t v; memcpy(&v, p + o, 4); return v; }
static void wr(uint8_t* p, uint32_t o, int32_t v) { memcpy(p + o, &v, 4); }

#define R(h) w->resolve(w->user, (h))
#define CALL(a, x, y, z) w->call(w->user, (a), (x), (y), (z))
#define TIMER() (int32_t)CALL(0x00493038, 0, 0, 0)

/* pending event push (inline in both): the count is read again for each store */
static void push_event(uint8_t* sc, uint8_t kind, uint32_t value)
{
    if (rd(sc, 0xacc) >= 0x96)
        return;
    sc[0xad0 + (uint32_t)rd(sc, 0xacc) * 8] = kind;
    wr(sc, 0xad4 + (uint32_t)rd(sc, 0xacc) * 8, (int32_t)value);
    wr(sc, 0xacc, rd(sc, 0xacc) + 1);
}

/* the command leaves the running list +0xaa0 / +0xaa4 */
static void remove_running(uint8_t* sc, uint32_t cmd)
{
    int32_t i = 0;
    if (rd(sc, 0xaa0) > 0) {
        do {
            if ((uint32_t)rd(sc, 0xaa4 + 4 * (uint32_t)i) == cmd)
                goto found;
            i++;
        } while (i < rd(sc, 0xaa0));
    }
    return;
found:
    if (i < rd(sc, 0xaa0)) {
        do {
            wr(sc, 0xaa4 + 4 * (uint32_t)i, rd(sc, 0xaa8 + 4 * (uint32_t)i));
            i++;
        } while (i < rd(sc, 0xaa0));
    }
    wr(sc, 0xaa0, rd(sc, 0xaa0) - 1);
}

/* the timer test shared by both: start at +start_off, now stored at +now_off */
static void timer_done(const Mt2UiWait* w, uint32_t cmd, uint32_t start_off, uint32_t now_off)
{
    uint8_t* c = R(cmd);
    int32_t now = TIMER();
    wr(c, now_off, now);
    if (rd(c, 0) <= -1)
        return;
    if (!((double)rd(c, 0) < (double)((uint32_t)now - (uint32_t)rd(c, start_off)) * 1e-06))
        return;
    push_event(R((uint32_t)rd(c, 0xc)), 2, cmd);
    remove_running(R((uint32_t)rd(c, 0xc)), cmd);
    wr(c, 4, rd(c, 0));
}

static void mark(const Mt2UiWait* w, uint8_t* sc, uint8_t on)
{
    uint32_t item = (uint32_t)rd(sc, 0xbc + 4 * (uint32_t)rd(sc, 0x48));
    uint32_t rec = (uint32_t)rd(R(item), 4);
    if (rec != 0 && R(rec)[0] == 9)
        R(rec)[7] = on;
}

void mt2_ui_wait_input(const Mt2UiWait* w, uint32_t cmd)
{
    static const struct { uint8_t key, result; } keys[] = {
        {3, 0xa}, {2, 9}, {0, 7}, {1, 8}, {0x11, 3}, {0x13, 0xe}, {0x14, 0x10}, {0x15, 0xf},
        {0x18, 0xd}, {0x19, 0xf}, {0x1a, 0x10}, {0x20, 0xb}, {0x21, 5}, {0x1c, 0xc}, {0x1b, 6}};
    uint8_t* c = R(cmd);
    int32_t x, y;
    unsigned k;

    if (rd(c, 0x18) == 0)
        wr(c, 0x18, TIMER());
    CALL(0x00493748, 0, 0, 0);
    c[0x14] = 0xff;
    w->mouse_read(w->user, &x, &y);
    wr(c, 0x10, 0);
    CALL(0x004b2b00, 0, 0, 0);
    if (x != w->last_xy[0] || y != w->last_xy[1])
        wr(c, 0x18, TIMER());
    w->last_xy[0] = x;
    w->last_xy[1] = y;
    if (CALL(0x004b2ee0, 0, 0, 0) != 0 && *w->mouse_latch == 0) {
        int32_t n, i;
        wr(c, 0x18, TIMER());
        n = rd(R((uint32_t)rd(c, 0xc)), 0x74);
        *w->mouse_latch = 1;
        for (i = 0; i < n; i++) {
            uint32_t obj = (uint32_t)rd(R((uint32_t)rd(c, 0xc)), 0x78 + 4 * (uint32_t)i);
            if (CALL(0x00469e24, obj, (uint32_t)x, (uint32_t)y) != 0) {
                uint8_t* sc = R((uint32_t)rd(c, 0xc));
                push_event(sc, 0x11, (uint32_t)rd(sc, 0x78 + 4 * (uint32_t)i));
                break;
            }
        }
        n = rd(R((uint32_t)rd(c, 0xc)), 0xb8);
        for (i = 0; i < n; i++) {
            uint32_t obj = (uint32_t)rd(R((uint32_t)rd(c, 0xc)), 0xbc + 4 * (uint32_t)i);
            if (CALL(0x00469e24, obj, (uint32_t)x, (uint32_t)y) != 0) {
                mark(w, R((uint32_t)rd(c, 0xc)), 0);
                wr(R((uint32_t)rd(c, 0xc)), 0x48, i);
                mark(w, R((uint32_t)rd(c, 0xc)), 1);
                CALL(0x00493728, 0x13, (uint32_t)rd(c, 0x10), 0);
                break;
            }
        }
    } else if (CALL(0x004b2ee0, 0, 0, 0) == 0) {
        *w->mouse_latch = 0;
    }
    if ((uint8_t)CALL(0x004936ac, 7, (uint32_t)rd(c, 0x10), 0) != 0)
        wr(c, 0x18, TIMER());
    for (k = 0; k < sizeof keys / sizeof keys[0]; k++)
        if ((uint8_t)CALL(0x004936ac, keys[k].key, (uint32_t)rd(c, 0x10), 0) != 0) {
            c[0x14] = keys[k].result;
            break;
        }
    if (c[0x14] != 0xff) {
        uint8_t* sc = R((uint32_t)rd(c, 0xc));
        uint8_t kind = c[0x14];
        push_event(sc, kind, (uint32_t)rd(sc, 0xbc + 4 * (uint32_t)rd(sc, 0x48)));
    }
    timer_done(w, cmd, 0x18, 0x1c);
}

void mt2_ui_wait_timeout(const Mt2UiWait* w, uint32_t cmd)
{
    uint8_t* c = R(cmd);
    if (rd(c, 0x14) == 0)
        wr(c, 0x14, TIMER());
    CALL(0x00493748, 0, 0, 0);
    if ((uint8_t)CALL(0x00493694, (uint32_t)rd(c, 0x10), 0, 0) != 0) {
        uint8_t* sc = R((uint32_t)rd(c, 0xc));
        push_event(sc, 3, (uint32_t)rd(sc, 0xbc + 4 * (uint32_t)rd(sc, 0x48)));
        remove_running(R((uint32_t)rd(c, 0xc)), cmd);
        wr(c, 0x14, TIMER());
        return;
    }
    timer_done(w, cmd, 0x14, 0x18);
}
