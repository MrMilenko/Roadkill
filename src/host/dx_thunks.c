/*
 * dx_thunks.c: the EXE's import thunks (`jmp [slot]`, 0x004d18c0 ..) of the DirectX imports this
 * host implements. Ports call some imports through the thunk address (pf3_ddinit.c 0x004d18ea
 * DirectDrawCreate, pf4_dsinit.c 0x004d18e4 DirectSoundCreate) and others through the IAT slot;
 * a thunk forwards its stdcall words to whatever is bound at the slot. Each is bound only after
 * checking that the bytes at the thunk are `ff 25 <slot>` in the player's EXE.
 *
 * (If the core comes to bind every `jmp [slot]` thunk generically, this file goes away: binding
 * an address twice stops the run at start up.)
 */
#include "dx_com.h"

#define THUNK(name, thunk, slot) \
    static uint32_t name(const uint32_t* a, int n) { return rt_call(slot, a, n); }

THUNK(t_dsound_create, 0x004d18e4u, 0x00c907b0u)
THUNK(t_ddraw_create, 0x004d18eau, 0x00c907c0u)
THUNK(t_dplay_enumerate, 0x004d1902u, 0x00c907d4u)
THUNK(t_dplay_create, 0x004d1908u, 0x00c907d8u)
THUNK(t_dinput_create, 0x004d1914u, 0x00c90810u)

static void bind_thunk(uint32_t thunk, uint32_t slot, RtFn fn, const char* name)
{
    if (rt_r8(thunk) != 0xff || rt_r8(thunk + 1) != 0x25 || rt_r32(thunk + 2) != slot) return;
    rt_bind(thunk, fn, name);
}

static void rt_bind_host_dx_thunks(void)
{
    bind_thunk(0x004d18e4u, 0x00c907b0u, t_dsound_create, "thunk DirectSoundCreate");
    bind_thunk(0x004d18eau, 0x00c907c0u, t_ddraw_create, "thunk DirectDrawCreate");
    bind_thunk(0x004d1902u, 0x00c907d4u, t_dplay_enumerate, "thunk DirectPlayEnumerateA");
    bind_thunk(0x004d1908u, 0x00c907d8u, t_dplay_create, "thunk DirectPlayCreate");
    bind_thunk(0x004d1914u, 0x00c90810u, t_dinput_create, "thunk DirectInputCreateA");
}

RT_BIND_AREA(host_dx_thunks)
