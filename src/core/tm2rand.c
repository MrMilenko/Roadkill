/* tm2rand.c: see tm2rand.h */
#include "tm2rand.h"

static uint32_t s_state = 1;     /* C runtime default before any srand */

void tm2_srand(uint32_t seed) { s_state = seed; }

int32_t tm2_rand(void) {
    s_state = s_state * 0x41c64e6du + 0x3039u;
    return (int32_t)((s_state >> 16) & 0x7fff);
}
