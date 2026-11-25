/*
 * dx_dplay.c: DirectPlay for the game (port code, not from the original): single player only for
 * now. DPLAYX.dll is imported by ordinal (the runtime's import table keeps names only, so the
 * slots are bound by address, after checking that the EXE's thunks point at them):
 *
 *   #2 DirectPlayEnumerateA(callback, context), slot 0x00c907d4 (thunk 0x004d1902): returns DP_OK
 *      without calling the callback: no service providers, the network menus list none.
 *   #1 DirectPlayCreate(GUID*, IDirectPlay** out, outer), slot 0x00c907d8 (thunk 0x004d1908;
 *      en7_a.c calls it "DirectSoundCreate"): DPERR_UNAVAILABLE, *out = 0. With no providers the
 *      game does not get there (0x004aa524 needs a device count > 0).
 */
#include "dx_com.h"

#define DPERR_UNAVAILABLE 0x887703e8u

#define SLOT_ENUMERATE 0x00c907d4u
#define SLOT_CREATE    0x00c907d8u

static uint32_t imp_dplay_enumerate(const uint32_t* a, int n)
{
    (void)a; (void)n;
    dx_log_once("DirectPlayEnumerateA: no service providers (single player)");
    return DX_OK;
}

static uint32_t imp_dplay_create(const uint32_t* a, int n)
{
    if (dx_arg(a, n, 1)) rt_w32(dx_arg(a, n, 1), 0);
    dx_log_once("DirectPlayCreate: unavailable (single player)");
    return DPERR_UNAVAILABLE;
}

static void rt_bind_host_dx_dplay(void)
{
    /* imported by ordinal: the runtime names them "DPLAYX.dll!#n" from the EXE's import table */
    rt_bind_import("DPLAYX.dll!#2", imp_dplay_enumerate);   /* DirectPlayEnumerateA */
    rt_bind_import("DPLAYX.dll!#1", imp_dplay_create);      /* DirectPlayCreate */
}

RT_BIND_AREA(host_dx_dplay)
