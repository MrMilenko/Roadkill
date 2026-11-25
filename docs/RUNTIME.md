# The runtime: running the ported functions as the game

`tm2` (src/app/tm2.c) runs the game on the ported code. Every port was verified on its own
against TM2.EXE under the emulator; the runtime (src/rt) wires them together. Port code, not a
decompiled function: where it differs from the original it says so.

```
cmake --build build --target tm2 && ./build/tm2 [disc/files/TM2.EXE]
TM2_TRACE=1 ./build/tm2      # every call, indented by depth
```

## Memory

One 64 MB host buffer is the game's 32 bit address space (`rt_base`, src/rt/rt.h):

| range | what |
|---|---|
| 0x00400000..0x00e0d000 | TM2.EXE's headers and sections, copied at start up from the player's own file (`.bss` zeroed). Game data never goes in the source. |
| 0x01000000..0x03000000 | the heap the game's allocator grows into (`rt_heap_grow`, the runtime's sbrk) |
| 0x03000000..0x03f00000 | the frame stack, grows down: the locals a port keeps at game addresses (`En2Env.scratch`, a screen's frame base `fb`) |
| 0x03f00000..0x04000000 | fake addresses the host hands out (COM objects and vtables, handles) |

`rt_ptr(a)` is the host pointer of game address `a`; `rt_r32` / `rt_w32` and friends read and
write it. A port that takes host pointers to globals (the older ports' context structs, e.g.
`MtCars.human_count`) gets `rt_ptr(0x005075e0)`; one that takes a `WMem` gets one region
`{0, RT_SIZE, rt_base}`; `En2Env` / `Pf2Env` get `rt_mem_cb`.

## Calls

`rt_call(fn, args, n)` looks `fn` up in the binding table and calls its adapter
`uint32_t adapter(const uint32_t* a, int n)`; the result is the callee's EAX.

- **A Watcom function** (game code, or the runtime above 0x004c0000): `fn` = its address,
  `a` = the callee's register arguments, then its stack words, compactly: a callee that takes k
  register arguments (a prefix of EAX, EDX, EBX, ECX: k = 1 + the last register it reads) gets
  them in `a[0 .. k-1]` and stack word i in `a[k + i]`. A cdecl / stdcall function with no
  register arguments (sprintf, a window or thread procedure, WinMain) gets its stack words from
  `a[0]`. This is how the emulator harnesses recorded every call (`regs[:nregs] + stack`), so it
  is what the ported callers pass. `rt_arg(a, n, i)` reads one, 0 when not passed.
- **A Win32 import**: `fn` = its IAT slot, `a` = the stdcall words in order. Host services bind
  by name: `rt_bind_import("lstrcpyA", fn)` (the slot comes from the player's EXE import table).
- **A COM method**: `fn` = `PF2_COM(interface, vtable offset)` (src/platform/pf2_env.h),
  `a[0]` = the object, then the stdcall words. A port that instead reads a vtable from game
  memory and calls the address it finds gets fake vtables whose entries are bound addresses.

An address with no binding stops the run and prints the chain of calls that reached it: that is
the next thing to bind. Binding the same address twice stops at start up.

## Writing a binding

One file per area, `src/rt/bind_<area>.c` (host services: `src/host/<x>.c`), ending with
`RT_BIND_AREA(<area>)` after `void rt_bind_<area>(void) { rt_bind(addr, adapter, "name"); ... }`.
The build picks the file up by itself (CMake glob, re-run cmake once after adding a file).

The adapter does exactly what the port's emulator harness did when it called the port
(tools/emu/diff_<group>.py: which registers, in which order, which stack words, what context):

```c
/* 0x004a8b04 WinMain(hInstance, hPrev, cmdline, nShow), stdcall */
static uint32_t b_game_main(const uint32_t* a, int n)
{
    En2Env e = rt_en2env();          /* a fresh RT_SCRATCH frame for its game address locals */
    uint32_t r = en8_game_main(&e, 0, rt_arg(a, n, 0), rt_arg(a, n, 1), rt_arg(a, n, 2), rt_arg(a, n, 3));
    rt_en2env_done();                /* pop it */
    return r;
}
```

- Ports tested with the `T(...)` harnesses (diff_en2_common / diff_en3_common / diff_pf2_env and
  their subclasses) are bound by a generated table: `TM2_BINDGEN=<file> sh tools/emu/check_<g>.sh`
  records each test's (address, function, registers, stack words); `tools/rt/gen_bind.py` writes
  `src/rt/bind_gen_t.c`. Do not edit that file; fix the generator.
- A port with a frame base (`fb`): `uint32_t fb = rt_frame_push(size)` with the size its header
  gives, `rt_frame_pop(size)` after.
- A port with a context struct: fill it once (a static, pointers into `rt_ptr`), its callbacks
  call `rt_call` with the original address and the original argument order.
- A port that takes a caller register the original reads without setting (`edi_in`, `esi_in`:
  ORIGINAL BUG paths) gets 0 unless the binding knows better; say so in a comment.
- A harness artefact that only made sense under the emulator (a placeholder address standing for
  a local, a stub's scripted answer) needs a real equivalent here: note it, fix it in the port if
  needed and rerun that port's check.

`tools/rt/unbound.tsv` lists every verified function with no binding yet (address, name, source).

## Known gaps (code the game reaches that is not ported yet)

The verified count (tools/progress.py) only covers the functions in Ghidra's function list.
What the runtime has shown is missing:

- **Code Ghidra never split into functions** ("unlisted"), some reached through vtables and
  function pointers (e.g. 0x004b9380, music vtable slot 0; 0x004ba49c sample init). Inventory and
  ports: tools/rt/unlisted.tsv (in progress).
- **Pieces of verified functions that the harness delegated** instead of porting: the car
  contact loop of car_hits_gather (gp5_contacts.c) and the car / object branch of
  car_hits_resolve (gp6_car_object.c) are now ported and verified as code ranges and wired in.
  Others may surface as runtime stops.
- **The x87 trig functions** (level_setup_viewport 0x00493ed0, viewCreate 0x00494610,
  subsys_menu_options 0x0049437c, 0x004b93cc, 0x004b9674): in progress, with host libm at the
  x87 helper boundary.
- **Host stubs**: dialog boxes (the audio / input config dialogs answer Cancel), GDI drawing,
  menus, movies, DirectPlay, Direct3D (the game uses its software renderer).
- **CD audio** (src/host/snd_mci.c, cd_audio.c, cd_image.c): the MCI "cdaudio" device plays the
  music tracks straight from the player's CD image (TM2_CDIMAGE, default the .cue in TM2_DATA's
  parent folder, e.g. disc/TWISTEDMETAL2.cue; never copied), mixed into the DirectSound output.
  Open, status, TMSF / MSF / ms time formats, play from / to, pause, resume, stop, seek, close;
  a track that ends stops (mode "stopped"). GAP: the game replays it from its monitor thread
  (0x004b902c, polls every 3 s), but that thread never gets the host lock (the main loop never
  blocks), so for now a track plays once, until the next screen picks one. The CD volume is
  the aux device's (auxSetVolume); the WINMM mixer is still a stub (the audio dialog's CD slider does nothing).
  MM_MCINOTIFY (unused by the game) is delivered at the next MCI call, not through the queue.
  No image or TM2_NOSOUND=1: MCI_OPEN fails as before and the game runs without music.
  TM2_CD_LOG=1 logs the MCI calls and the mixed level once a second.
- **Joysticks** (src/host/input_dinput.c on input_joy.c: SDL game controllers as DirectInput
  joysticks, mapping in input_joy.c; TM2_NOJOY=1 none, TM2_VIRTUAL_PAD=1 + TM2_PAD a scripted
  pad; test tools/rt/test_dinput_joy.c): no hot plug, no force feedback. The game only reads the
  pad when its saved InputJoystickStatus is 1 (a registry saved without a pad has 2).
- **Threads** run under one host lock; each has its own frame slice and call chain. The main
  thread's PeekMessageA hands the lock to a waiting game thread (w32_yield): the CD monitor runs.
- **The network screen** (multi player) makes a pure virtual call (0x004a6a39, from
  render_set_background 0x004a2ea0 under screen_network 0x0047a18c): an object there never got its
  derived vtable, most likely because DirectPlay is stubbed. The game's fatal exit ends the run.
- **0x004b4374 ddraw_release_surfaces** reads an EDX the original leaves undefined (after a COM
  call); the port uses 0.
