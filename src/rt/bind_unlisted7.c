/*
 * bind_unlisted7.c: unlisted 0x00485af0 (play the object sound, a vtable slot), ported in
 * src/platform/pf8_sndplay.c; the four registers (compact: a Watcom method with register arguments).
 */
#include "rt_env.h"
#include "platform/pf8_sndplay.h"

static uint32_t b_sound_play(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_00485af0(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted7(void)
{
    rt_bind(0x00485af0u, b_sound_play, "unlisted_sound_play");
}

RT_BIND_AREA(unlisted7)
