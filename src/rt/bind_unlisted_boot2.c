/*
 * bind_unlisted_boot2.c: unlisted 0x00485ebc (boot path, from the static constructor 0x00473da0), ported in
 * src/platform/pf8_mixer.c; the adapter passes the four registers as tools/emu/diff_pf8_mixer.py does.
 */
#include "rt_env.h"
#include "platform/pf8_mixer.h"

static uint32_t b_00485ebc(const uint32_t* a, int n)
{
    Pf2Env e = rt_pf2env();
    uint32_t r = pf8_00485ebc(&e, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();
    return r;
}

void rt_bind_unlisted_boot2(void)
{
    rt_bind(0x00485ebcu, b_00485ebc, "unlisted_audio_screen_ctor");
}

RT_BIND_AREA(unlisted_boot2)
