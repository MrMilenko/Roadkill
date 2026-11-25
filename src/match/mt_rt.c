/*
 * mt_rt.c: frame loop leaves (see mt_rt.h).
 */
#include "mt_rt.h"

#include <string.h>

void mt_rt_schedule_event(const MtRt* r, uint8_t event, uint32_t value) {
    int32_t n = *r->event_count;
    if (n < 4) {
        n++;
        r->events[8 * n + 4] = event;
        memcpy(r->events + 8 * n, &value, 4);
    }
    *r->event_count = n;
}

void mt_rt_events_reset(const MtRt* r) {
    int32_t off = (int32_t)((uint32_t)*r->event_count * 8u + 8u); /* entry count + 1, four times */
    uint32_t m1 = 0xffffffffu;
    for (int k = 0; k < 4; k++) {
        r->events[off + 4] = 0;
        memcpy(r->events + off, &m1, 4);
    }
    *r->flag_940 = 0;
    *r->event_count = 0;
}

void mt_rt_buffers_init(const MtRt* r) {
    r->buf_518e74[0] = 0x00520e9cu;
    r->buf_520e84[0] = 0x00551bdcu;
    *r->buf_510e70 = 0x0058291cu;
    r->buf_518e74[3] = 0x0058691cu;
    r->buf_518e74[2] = 0x00520e9cu + 0x30cf0u;
    *r->cur_buffer = 0x00510e70u;
    r->buf_520e84[2] = 0x00551bdcu + 0x30cf0u;
}

uint32_t mt_rt_get_520e94(const MtRt* r) { return *r->v_520e94; }
uint32_t mt_rt_get_520e90(const MtRt* r) { return *r->v_520e90; }
uint32_t mt_rt_cur_buffer(const MtRt* r) { return *r->cur_buffer; }
uint32_t mt_rt_get_510dc4(const MtRt* r) { return *r->v_510dc4; }
uint32_t mt_rt_screen_buffer(void) { return 0x00510e70u; }
uint32_t mt_rt_buffer_520e9c(void) { return 0x00520e9cu; }
uint32_t mt_rt_buffer_58291c(void) { return 0x0058291cu; }
