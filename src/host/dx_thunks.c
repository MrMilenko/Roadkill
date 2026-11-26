/*
 * dx_thunks.c: the EXE's import thunks (`jmp [slot]`, 0x004d18c0 ..) of the DirectX imports this
 * host implements. Ports call some imports through the thunk address (pf3_ddinit.c 0x004d18ea
 * DirectDrawCreate, pf4_dsinit.c 0x004d18e4 DirectSoundCreate) and others through the IAT slot;
 * a thunk forwards its stdcall words to whatever is bound at the slot. Each is bound only after
 * checking that the bytes at the thunk are `ff 25 <slot>` in the player's EXE.
 *
 * The DirectPlay thunks are also called by pf8_boot.c's call_n, which passes the four registers
 * (eax, edx, ebx, ecx) before the stack words, as its emulator harness recorded them: a thunk
 * whose import takes k stdcall words forwards the last k (the registers mean nothing to a
 * `jmp [slot]`).
 *
 * (If the core comes to bind every `jmp [slot]` thunk generically, this file goes away: binding
 * an address twice stops the run at start up.)
 */
#include "dx_com.h"

#define THUNK(name, thunk, slot) \
    static uint32_t name(const uint32_t* a, int n) { return rt_call(slot, a, n); }

THUNK(t_dsound_create, 0x004d18e4u, 0x00c907b0u)
THUNK(t_ddraw_create, 0x004d18eau, 0x00c907c0u)
#define THUNK_K(name, thunk, slot, k) \
    static uint32_t name(const uint32_t* a, int n) { return n > (k) ? rt_call(slot, a + (n - (k)), (k)) : rt_call(slot, a, n); }

THUNK_K(t_dplay_enumerate, 0x004d1902u, 0x00c907d4u, 2)   /* (callback, context) */
THUNK_K(t_dplay_create, 0x004d1908u, 0x00c907d8u, 3)      /* (GUID*, IDirectPlay**, outer) */
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
