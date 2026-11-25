# The DirectX layer of TM2.EXE: decompilation notes

Goal: decompile every DirectDraw, DirectSound and DirectInput use in TM2.EXE (what each call does
for the game, with the original's arguments and error paths written down), then give each one a
platform equivalent in plain C (SDL today). Not a Win32 / COM emulation: the game's own wrapper
functions get ported, the way the rest of the game is. Anyone porting to another API later can
work from these notes.

## What is known (2026-11-21, first pass)

- 211 imports. Graphics are DirectDraw only (no Direct3D import): the game renders in software and
  presents through DirectDraw surfaces. GDI is used for text and some blits (StretchDIBits,
  ExtTextOutA). Sound: DirectSound plus WINMM (MCI for CD audio, mixer, joystick). Input:
  DirectInput plus GetAsyncKeyState and joyGetPosEx. DPLAYX (by ordinal) is multiplayer.
- Watcom calls every import through a `jmp [iat]` thunk (tools/dx/pe_util.py finds all 211).
- The interface pointers are not globals: DirectDrawCreate writes into a field of a wrapper
  object (`[ebp + 8]` in 0x004b3d80 ddraw_init), so the game has its own DirectDraw module
  (wrappers around 0x004b3d80..0x004b4300). DirectSound: 0x004b94f4 dsound_init. DirectInput:
  four create sites (0x0049e110, 0x004a36db, 0x004a7be0, 0x004c3e54).

## The display object (0x00c84840) and its module, as decompiled so far

Fields (from which methods each one receives; tools/dx/com_calls.py lists every COM call by the
field the interface pointer was loaded from):

| Field | What | Evidence |
|---|---|---|
| +0x04 | IDirectDraw (DirectDrawCreate) | QueryInterface then Release in 0x004b3d80 |
| +0x08 | IDirectDraw2 (from QueryInterface) | SetCooperativeLevel, SetDisplayMode, RestoreDisplayMode, CreateSurface, CreateClipper, EnumDisplayModes |
| +0x0c | primary surface | Lock, Unlock, Restore, IsLost, Blt, Flip, GetAttachedSurface, SetClipper |
| +0x10 | back buffer (flip chain) | GetCaps, Blt target before Flip |
| +0x14 | offscreen surface (the one the game draws into by default) | Lock, Restore, Blt (fill) |
| +0x20 | clipper | SetHWnd, Release |
| +0x50 | registry "Renderer" | read in 0x004b3d80 |
| +0x54 | window handle; +0x78..+0x84 client rect in screen coordinates; +0x88 desktop bits per pixel | 0x004b3d80 |
| +0x58 / +0x5c | screen width / height | 0x004b5a14 / 0x004b5a10 |
| +0x60 | surface pitch | 0x004b4418 |
| +0x6c | full screen (0 = window) | 0x004b5128 |
| +0x70 | window offset off (the primary lock is not moved to the client rect) | 0x004b4418 |
| +0x74 | present mode 0, 1, 3, 4 | 0x004b5128 |

Functions (verified by reading the machine code unless marked):

| Address | Does | Notes |
|---|---|---|
| 0x004b3d80 | init: window rect and desktop depth, registry DirectDraw GUID / Renderer, releases a previous setup, DirectDrawCreate (with the GUID, else the default), QueryInterface for IDirectDraw2, SetCooperativeLevel | the DDSCL value last set is cached in 0x004e86ac |
| 0x004b4418 | lock: target 0 = +0x14, 1 = +0x0c; Lock(rect, desc 0x6c, DDLOCK_WAIT); lpSurface -> 0x00a04674 (and the renderer, 0x004ad82c), lPitch -> 0x00a04670 / +0x60 (and 0x004ad760) when it changes; on the windowed primary the pointer moves to the client rect | DECOMPILER TRAP: Ghidra swaps lpSurface and lPitch |
| 0x004b44e4 | unlock (same target choice); 0x00a04674 = 0 | |
| 0x004b4d98 | color fill: Blt with DDBLTFX (size 100, fill color +0x50) on a surface, default +0x14 | the matcher had paired it with PS1 taskMgrStopAll: wrong |
| 0x004b5128 | present: IsLost / restore (0x004b4508) on DDERR_SURFACELOST, then by +0x6c and +0x74: window: Blt to the primary (modes 0, 1, 3, 4); full screen: modes 0 / 1 Blt, 3 Blt into +0x10 then Flip, 4 Flip | the Blt rectangles are not in Ghidra's output; to read from the machine code |
| 0x00497854 | 16 bit rectangle copy (game code, used for the shell backgrounds) | emulator verified, src/core/blit.c |

Port: `src/platform/display.h` is what the game code sees (pointer, pitch, size, format, lock /
unlock / present); `src/platform/sdl_display.c` is the SDL backend (port code).

## The renderer is a software PlayStation GPU

The PC build kept the PS1's drawing model. Game code builds PS1 GPU primitives (the font's 24 byte
SPRT record: color +4..+6, command code +7, position, size, texture coordinates) and hands them
to 0x004967f4, the PC's stand in for PsyQ AddPrim: outside split screen it locks the screen,
rasterizes the primitive at once with `command_dispatch` 0x004ad874 (a software rasterizer that
decodes the command byte, bit 1 = semi transparency cleared first), and unlocks; in split screen
it queues it (0x004963b4). So menus, HUD and 3D all end in one software GPU drawing into the 16 bit
surface. Porting command_dispatch (and the edge / span code it calls, 0x004af164, 0x004c6000 ...)
gives the native build its whole renderer; it can be diffed framebuffer against framebuffer.

## Create call sites

    DirectSoundCreate	at 004b9504	in 004b94f4 dsound_init	PS1: -
    DirectSoundCreate	at 004b9787	(not in a known function)
    DirectDrawCreate	at 004b3fce	in 004b3d80 ddraw_init	PS1: -
    DirectDrawCreate	at 004b4047	in 004b3d80 ddraw_init	PS1: -
    DirectDrawCreate	at 004b425f	(not in a known function)
    DirectInputCreateA	at 0049e16e	in 0049e110 FUN_0049e110	PS1: -
    DirectInputCreateA	at 004a36db	(not in a known function)
    DirectInputCreateA	at 004a7c02	in 004a7be0 FUN_004a7be0	PS1: -
    DirectInputCreateA	at 004c3e8a	in 004c3e54 FUN_004c3e54	PS1: -
    

## Next

1. Trace each create call's output (wrapper struct field) to every use: that separates the real
   DirectX calls from the game's own virtual calls (the raw list from tools/dx/inventory.py has
   about 130 functions with `call [reg + disp]`, many of them the game's object system).
2. Name the wrapper functions and their structs, method by vtable offset (IDirectDraw,
   IDirectDrawSurface, IDirectDrawPalette, IDirectSound, IDirectSoundBuffer, IDirectInput,
   IDirectInputDevice).
3. Port them as src/platform/ (SDL) with each original call documented next to its replacement;
   the menus (shell screens, which blit TIM backgrounds and draw text) come first.


## Correction (2026-11-21, engine port): a hardware 3D device path exists

0x004958ac (old name screen_is_split) does not test split screen: it tests [0x00c85008], set when a
hardware 3D device renderer is active (0x004b3d80 sets it from the registry "Renderer" value). In
that mode display_cmd_build 0x00491018 builds 0x80 / 0x82 records for the device path 0x004b67d0
(COM calls on the display object +0x28) instead of the software GPU: that is why code 0x80 is not
in command_dispatch. No Direct3D DLL is imported; the interface presumably comes from DirectDraw
QueryInterface. The software path (the default, and what is ported) turns textured polygons with
render flag 0x10 into 0xa0 records (gradients by 0x0048f838), untextured ones into 0xb0 / 0x90.
