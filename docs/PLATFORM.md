# The platform layer of TM2.EXE (0x004b0000..0x004c0000): decompilation and portable equivalents

What this is: every function of TM2.EXE between 0x004b0000 and 0x004c0000 (the 161 entries of
the function list plus the code Ghidra never split), decompiled from the machine code: what it does
for the game, every DirectDraw / Direct3D / DirectSound / Win32 / MCI call with its arguments and
error paths, and a portable equivalent. Not an emulation of COM or Win32: where a wrapper keeps
game visible state or computes values the game sees, that logic is ported to C over a small
backend interface (the original's COM / Win32 calls become backend calls, same order, same
arguments) and proven against the original under Unicorn; the backend is then implemented on
SDL2. Wrappers with no logic of their own are documented only (said so in each table: "Port
status: documented only"). Companion notes: docs/DIRECTX.md (the first pass), docs/PORTING_GUIDE.md.

Date: 2026-11-21. Everything here was read from the machine code (capstone), with Ghidra's C as a
reading aid only; DECOMPILER TRAP marks where Ghidra's C is wrong, ORIGINAL BUG where the original
does something it surely did not mean (kept by the ports unless said otherwise).

## Conventions

- Watcom register calling convention: arguments in eax, edx, ebx, ecx, then the stack; `ret N`
  = the callee pops N bytes. COM methods are stdcall (`this` first, callee pops).
- `iface::Method (0xNN)` = the vtable slot. The interface is known from the object field the
  pointer came from (tools/dx/com_calls.py).
- 0x004979a2 (used by all the x87 sound / display math) is `frndint` with the rounding control set
  to chop: it truncates toward zero. Some reference tables below say "round" or "frndint" for it
  where written before this was checked: read "chop".
- Imports are called as `call [iat]` (e.g. GetAsyncKeyState [0x00c904b8]); the emulator diffs
  hook the IAT slots.

## The ported parts and their proofs

| Subsystem | Original | Port (src/platform/) | Backend on SDL2 | Proof (tools/emu/) | Cases |
|---|---|---|---|---|---|
| Display: lock / unlock / restore / fill / present / mode index / getters | 0x004b4418, 0x004b44e4, 0x004b4508, 0x004b4d98, 0x004b5128, 0x004b4558, 0x004b454c, 0x004b4554, 0x004b5a10, 0x004b5a14 | pf_display.c / .h | pf_sdl_display.c / .h | check_pf_display.sh, diff_pf_display.py | 12 000 seed 1 + 12 000 seed 7, 138 branch combinations, all match |
| Input: mouse | 0x004b2820, 0x004b295c, 0x004b29e0, 0x004b2ad4, 0x004b2b00, 0x004b2b84, 0x004b2bdc, 0x004b2c44, 0x004b2c9c, 0x004b2d00, 0x004b2d60, 0x004b2db8, 0x004b2e48, 0x004b2ee0, 0x004b2f00, getters | pf_mouse.c / .h | pf_sdl_mouse.c / .h | check_pf_mouse.sh, diff_pf_mouse.py | 12 000 + 12 000, 172..174 combinations, all match |
| Sound: DirectSound buffer wrappers and sample math | 0x004b987c..0x004b9a48 (16 wrappers), 0x004ba76c, 0x004ba890, 0x004ba938, 0x004b9168 | pf_dsound.c / .h | pf_sdl_audio.c / .h | check_pf_dsound.sh, diff_pf_dsound.py | 12 000 + 12 000, 61 combinations, all match |
| Sound: voice list slot update | 0x004bbaf8, 0x004bbd80..0x004bbdb8 | pf_sfx.c / .h | (uses pf_dsound) | check_pf_sfx.sh, diff_pf_sfx.py | 12 000 + 12 000, 53 combinations, all match |

How the COM objects are faked (all four diffs): an object in emulator memory whose first dword
points at a vtable of hooked addresses; a hook reads the stdcall arguments from the stack, records
`(slot, this, args)`, writes what the method would write (Lock's lpSurface / lPitch into the
caller's DDSURFACEDESC, GetStatus / GetVolume outputs), returns a scripted HRESULT and pops the
arguments. The structures the original builds for a call (the 0x6c byte DDSURFACEDESC, the 100
byte DDBLTFX) are checked field by field. The port runs over a backend of C callbacks that record
the same tuples from the same scripts; sequences, returns, outputs and the state the wrapper keeps
are compared. Game functions the wrappers call are stubbed the same way.

## Subsystem index (where each part is below)

| Subsystem | Sections |
|---|---|
| Display (DirectDraw: init, cooperative level, modes, surfaces, clipper, lock / unlock / fill / present, GDI text) | "Display: the ported wrappers"; Part A (0x004b270c..0x004b4374: GDI text 0x004b31d0..0x004b3bac, ddraw_init 0x004b3d80, release / restore); Part B (0x004b4508..0x004b6780: surfaces, flip chain, clipper, modes, present rect, caps) |
| Hardware 3D device path (Direct3D 5, display +0x24 / +0x28) | Part B: 0x004b5bf0..0x004b67d0 (documented only, not ported) and Part D: 0x004bf920..0x004bff08 (texture formats) |
| Input | "Input: the mouse module, ported"; Part A (mouse). DirectInput (keyboard, joystick) is not in this range: its create sites are 0x0049e110, 0x004a36db, 0x004a7be0, 0x004c3e54 (docs/DIRECTX.md) |
| Sound (DirectSound, voices, banks) | "Sound: ..." ported sections; Part C (device, buffers, samples, 3D math, CD audio by MCI, movies by MCI); Part D (sound bank loader, voice list) |
| Win32 helpers / C runtime | Part A (crt_utoa / itoa / sprintf, cursor), Part D (error box, Watcom runtime: memset, malloc, free, strstr, math) |
| Not platform code | 0x004bcb70 (a world grid lookup, old name hdpPush) |

## Portable backends: what stands in for what

| Original | SDL2 (src/platform/) |
|---|---|
| DirectDrawCreate + SetCooperativeLevel + SetDisplayMode + CreateSurface (0x004b3d80, 0x004b4c80, 0x004b45a0) | `pf_sdl_display_open`: SDL_CreateWindow (FULLSCREEN_DESKTOP for exclusive mode), SDL_CreateRenderer, streaming texture + 16 bit SDL_Surfaces; `sdl_create_surfaces` for a mode change |
| IDirectDrawSurface::Lock / Unlock | SDL_LockSurface / SDL_UnlockSurface on the 16 bit draw surface |
| Blt with DDBLT_COLORFILL | SDL_FillRect |
| Blt to the primary, Flip | SDL_UpdateTexture + SDL_RenderCopy + SDL_RenderPresent |
| IsLost / Restore | nothing: SDL surfaces are not lost |
| GDI text (GetDC / TextOutA / ExtTextOutA on the draw surface) | not provided by SDL2 core: draw with the game's own font sprites, or SDL_ttf (not a dependency here) |
| GetAsyncKeyState / GetCursorPos / ScreenToClient / SetCursorPos / GetSystemMetrics | SDL_GetMouseState, SDL_MOUSEBUTTONDOWN latch, SDL_WarpMouseInWindow (pf_sdl_mouse.c) |
| DirectSoundCreate, primary buffer, CreateSoundBuffer / DuplicateSoundBuffer, Lock / Unlock, Play / Stop / GetStatus / SetVolume / SetPan / SetFrequency | `pf_sdl_audio_open` (SDL_OpenAudioDevice, 44.1 kHz stereo) and a software mixer: per buffer gain 10^(v / 2000), pan as the attenuation of the other side, frequency as the resampling step (pf_sdl_audio.c) |
| MCI cdaudio (CD music), MCI avivideo (movies) | not provided: play the tracks from files (SDL_mixer or an Ogg decoder) and movies with a decoder; documented in Part C |

## Display: the ported wrappers (emulator diff)

These carry game visible state (the screen pointer and pitch the renderer draws through, the
present mode, the mode table) and are ported in `src/platform/pf_display.c` over
`PfDisplayBackend` (`src/platform/pf_display.h`); the SDL2 backend is
`src/platform/pf_sdl_display.c`. Proof: `tools/emu/check_pf_display.sh` runs the original under
Unicorn with the surfaces as fake COM objects (vtable entries are hooked addresses that record
method, `this` and arguments and return a scripted HRESULT; Lock writes lpSurface / lPitch into
the caller's DDSURFACEDESC, and the DDSURFACEDESC / DDBLTFX the original built are checked), and
compares call sequences, returns, the display object's +0x60 and the globals 0x00a04674 /
0x00a04670 with the port's backend calls. 12 000 cases on seed 1 and 12 000 on seed 7, 138
branch combinations: all match.

Display object fields these use: +0x00 (first field), +0x0c primary, +0x10 back buffer, +0x14
offscreen, +0x58 / +0x5c width / height, +0x60 pitch, +0x6c page flipping on (0x004b50f8), +0x70 full
screen, +0x74 (byte) present mode, +0x78..+0x84 client RECT (screen coordinates), +0xa0 mode
count (read absolute, 0x00c848e0), +0xa4 mode records of 16 bytes {width, height, bpp, 4th}
(0x00c848e4). Globals: 0x00a04674 screen pointer, 0x00a04670 screen pitch, 0x00c85008 hardware
3D device on.

### 0x004b4418 display_lock (pf_display_lock)

| | |
|---|---|
| Does | Locks a surface for the software renderer and publishes its pointer and pitch |
| Arguments | eax display, edx target (0 = offscreen +0x14; then a target equal to 1 = primary +0x0c; else a surface pointer), ebx out pointer for lpSurface or 0, ecx out pointer for lPitch or 0, [esp+4] RECT* (passed to Lock); `ret 4` |
| Calls | memset 0x004bf830 (desc, 0, 0x6c); IDirectDrawSurface::Lock (slot 0x64) (target, rect, &desc {dwSize 0x6c}, DDLOCK_WAIT = 1, NULL); renderer 0x004ad82c(eax = [0x00a04674]); renderer 0x004ad760(eax = pitch) |
| Error paths | Lock != DD_OK: returns the HRESULT, nothing stored |
| On success | lpSurface (desc +0x24) -> *ebx, else -> 0x00a04674; 0x004ad82c gets [0x00a04674] in both cases (ORIGINAL BUG: with ebx set it is the old pointer). lPitch (desc +0x10) -> *ecx; else if it differs from +0x60: -> 0x00a04670, +0x60, and 0x004ad760. Target == primary and +0x70 (full screen) == 0: 0x00a04674 += +0x60 * client.top + client.left * 2 (32 bit; done to the global even when the pointer went to *ebx, ORIGINAL BUG) |
| Returns | HRESULT of Lock |
| DECOMPILER TRAP | Ghidra swaps lpSurface and lPitch (stack slots mislabeled) |
| Portable equivalent | `SDL_LockSurface` on the 16 bit offscreen SDL_Surface (`pf_sdl_display.c` sdl_lock) |

### 0x004b44e4 display_unlock (pf_display_unlock)

| | |
|---|---|
| Arguments | eax display, edx target (as lock), ebx RECT* |
| Calls | IDirectDrawSurface::Unlock (slot 0x80) (target, rect) |
| Then | 0x00a04674 = 0, whatever Unlock returned |
| Returns | HRESULT of Unlock |
| Portable equivalent | `SDL_UnlockSurface` |

### 0x004b4508 display_restore (pf_display_restore)

| | |
|---|---|
| Arguments | eax display |
| Calls | IDirectDrawSurface::Restore (slot 0x6c) on +0x0c if set; on +0x14 if set and != +0x10 (the back buffer comes back with the primary's flip chain); if [0x00c85008] (hardware 3D device on): 0x00497390 (reload the device's resources) |
| Returns | device on: 0x00497390's result (0 -> 0); else the last Restore HRESULT; no surface and no device: the caller's edx (ORIGINAL BUG, unset register; port returns 0) |
| Portable equivalent | nothing to do: SDL surfaces are not lost (`sdl_restore` returns 0) |

### 0x004b4d98 display_fill (pf_display_fill)

| | |
|---|---|
| Arguments | eax display, edx fill color (16 bit pixel value), ebx surface or 0 (= +0x14) |
| Calls | memset(fx, 0, 100); IDirectDrawSurface::Blt (slot 0x14) (surface, NULL, NULL, NULL, DDBLT_COLORFILL / DDBLT_WAIT = 0x01000400, &fx {dwSize 100, dwFillColor (+0x50) = color}) |
| Error paths | no surface: E_FAIL 0x80004005 without a call |
| Returns | HRESULT of Blt |
| Note | the matcher had paired it with PS1 taskMgrStopAll: wrong (tools/match/reject.tsv) |
| Portable equivalent | `SDL_FillRect(surface, NULL, color)` |

### 0x004b5128 display_present (pf_display_present)

| | |
|---|---|
| Arguments | eax display |
| Calls | IDirectDrawSurface::IsLost (slot 0x60) (primary); == DDERR_SURFACELOST 0x887601c2 -> display_restore 0x004b4508 (result ignored). Then two jump tables (0x004b5114 flip on, 0x004b5100 flip off) on the byte +0x74: |
| Flip on (+0x6c != 0) | 0: Blt(primary, &client +0x78, offscreen, NULL, DDBLT_WAIT, NULL); 1: Blt(primary, NULL, offscreen, NULL, DDBLT_WAIT, NULL); 3: Blt(back +0x10, NULL, offscreen, NULL, DDBLT_WAIT, NULL) (result ignored) then Flip (slot 0x2c) (primary, NULL, DDFLIP_WAIT); 4: Flip(primary, NULL, DDFLIP_WAIT) only |
| Flip off (+0x6c == 0) | 0: Blt(primary, &client, offscreen, NULL, DDBLT_WAIT, NULL); 1, 3, 4: Blt(primary, NULL, offscreen, ...). These cases push ecx (+0x6c, = 0 here) for the source rect and DDBLTFX where the flip on ones push 0: same values. Mode meanings (from the setup code, doc below): 0 windowed Blt to the client rect, 1 full screen Blt, 3 flip chain with a system memory draw surface, 4 flip chain drawn in place |
| Error paths | no primary: returns 0 at once. Modes 2 and > 4: nothing, returns the caller's esi (ORIGINAL BUG, port returns 0) |
| Returns | HRESULT of the last Blt / Flip |
| Portable equivalent | `SDL_UpdateTexture` + `SDL_RenderCopy` + `SDL_RenderPresent` (sdl_blt to the primary, sdl_flip) |

### 0x004b4558 display_set_mode_index (pf_display_set_mode_index)

| | |
|---|---|
| Arguments | eax display, edx mode index |
| Does | 0 <= index < [0x00c848e0] (signed): ddraw_create_surfaces 0x004b45a0(eax display, edx width, ebx height, ecx bpp, stack 4th value) from the 16 byte record at 0x00c848e4 + 16 * index |
| Error paths | index out of range: E_FAIL 0x80004005 |
| Returns | ORIGINAL BUG: on success the caller's edi (edi is set only on the failure path), not the create result; the port returns the create result |
| Portable equivalent | the backend's create_surfaces (`SDL_SetWindowSize` + new texture / surfaces) |

### Field getters

| Address | Old name | Name | Does | Port |
|---|---|---|---|---|
| 0x004b454c | ddraw_get_mode_count | display_mode_count | `mov eax, [0x00c848e0]` (display +0xa0) | pf_display_mode_count |
| 0x004b4554 | carReadMoveCodes (matcher, wrong) | load_dword | `mov eax, [eax]`: a generic first field read; on the display object +0x00 | pf_display_field0 |
| 0x004b5a14 | FUN_004b5a14 | display_width | +0x58 | pf_display_width |
| 0x004b5a10 | FUN_004b5a10 | display_height | +0x5c | pf_display_height |

## Input: the mouse module, ported (emulator diff)

The game reads the mouse with GetAsyncKeyState / GetCursorPos (not DirectInput) into a 0x34 byte
state its callers keep (the attract mode idle timer, the menus). Ported in
`src/platform/pf_mouse.c` over `PfMouseBackend` (the Win32 calls and the game helpers it uses);
SDL2 backend `src/platform/pf_sdl_mouse.c`. Proof: `tools/emu/check_pf_mouse.sh`: the Win32
imports are hooked through their IAT slots (the game calls them as `call [iat]`), the game helpers
are stubbed, every call is recorded with its arguments on both sides and compared with the state,
the key globals and the returns. 12 000 cases on seed 1 (174 branch combinations) and 12 000 on
seed 7 (172): all match.

State: +0x00 present, +0x04 button count, +0x08 / +0x0c cursor x / y (client, -1 unknown), +0x10
button bits (1 / 2 / 4), +0x14 / +0x18 / +0x1c primary / secondary / middle down, +0x20 / +0x24
reference size (640 x 480, then the screen size), +0x28 axis range 0x7f, +0x2c 0, +0x30 dead zone
0x19. Globals 0x00c84810 / 0x00c84814 / 0x00c84818 virtual keys (1 / 2 / 4), 0x004e86a8 registry
not read yet.

| Address | Name | Does (Win32 calls) | Port |
|---|---|---|---|
| 0x004b2820 | mouse_init (old: ddraw_setup_display, wrong) | defaults; GetSystemMetrics(SM_MOUSEPRESENT 0x13), then (SM_CMOUSEBUTTONS 0x2b); middle key 4; first call: keys 1 / 2, registry HKCU "Control Panel\Mouse" "SwapMouseButtons" (0x004ac4b0 / 0x004ac528 / 0x004ac5e4 / 0x004ac590, buffer 0x50) == "1" (lstrcmpA) swaps; the once flag is cleared even when the open fails; returns the state | pf_mouse_init |
| 0x004b295c | mouse_reread_swap (not split by Ghidra) | the registry part alone; "1" -> primary 2 / secondary 1, else the keys stay as they are | pf_mouse_reread_swap |
| 0x004b29e0 | mouse_poll (not split) | present only: clear buttons; app_get_hwnd 0x004a8f34; GetAsyncKeyState of the three keys, `& 0x8001` (down now or pressed since the last call): primary sets +0x10 = 1 (dword), the others or 2 / 4 into its low byte; GetCursorPos (result unchecked), ScreenToClient(main window): failure -> x = y = -1; reference size = screen width / height (0x004958d8 / 0x004958e4) | pf_mouse_poll |
| 0x004b2ad4 | mouse_cursor_client (not split) | 0 without a mouse; GetCursorPos(pt), returns ScreenToClient(hwnd, pt) | pf_mouse_cursor_client |
| 0x004b2b00 | mouse_get_centered_pos | x = cursor.x - ref_w / 2, y = ref_h / 2 - cursor.y (division toward zero); point zeroed first; returns ScreenToClient's BOOL, outputs untouched on 0 | pf_mouse_centered_pos |
| 0x004b2b84 / 0x004b2c44 | mouse_axis_right / _down (not split) | d = pos - size / 2; 0 if d < 0 or size - size / 2 == 0; else d * (range + dead) / (size - size / 2) (imul / idiv); min(., range + dead) - dead, not below 0 | pf_mouse_axis_right / _down |
| 0x004b2bdc / 0x004b2c9c | mouse_axis_left / _up (not split) | d = pos - size / 2; 0 if d >= 0 or \|size / 2\| == 0; else \|d\| * (range + dead) / \|size / 2\|; same clamp | pf_mouse_axis_left / _up |
| 0x004b2d00 / 0x004b2d60 | mouse_set_cursor_pt / mouse_set_cursor (not split) | (POINT* / x, y): 0 <= x < screen width and 0 <= y < screen height: ClientToScreen(hwnd), SetCursorPos; returns ClientToScreen's BOOL (SetCursorPos's dropped), else 0 | pf_mouse_set_cursor_pt / pf_mouse_set_cursor |
| 0x004b2db8 / 0x004b2e48 | mouse_set_cursor_x / _y (not split) | the other coordinate from GetCursorPos / ScreenToClient (results unchecked), then as above. ORIGINAL BUG: without a mouse the current point is stack garbage (the port uses -1: no move); the diff covers mouse present only there | pf_mouse_set_cursor_x / _y |
| 0x004b2ee0 / 0x004b2f00 | mouse_primary_down / mouse_secondary_down | `(GetAsyncKeyState(key) & 0x8001) != 0` | pf_mouse_primary_down / _secondary_down |
| 0x004b2b78 / 0x004b2b7c / 0x004b2b80 / 0x004b2c40 / 0x004b2ed8 / 0x004b2edc | getters (not split) | +0x20 / +0x24 / +0x08 / +0x0c / +0x04 / +0x10 | struct fields (PfMouse), checked in the diff |

Portable equivalent (pf_sdl_mouse.c): GetSystemMetrics -> 1 / 3; GetAsyncKeyState ->
`SDL_GetMouseState` masks plus a "pressed since the last query" bit kept from
SDL_MOUSEBUTTONDOWN events (`pf_sdl_mouse_event`); GetCursorPos -> `SDL_GetMouseState` (window
relative, so ScreenToClient / ClientToScreen are identities); SetCursorPos ->
`SDL_WarpMouseInWindow`; the SwapMouseButtons read reports "not found" (SDL's buttons are already
logical).

## Sound: the DirectSound wrappers and sample math, ported (emulator diff)

Sound device object 0x00c850ec: +0x00 IDirectSound, +0x04 primary buffer, +0x08 (getter
0x004b9a48), +0x10 saved volume percent, +0x18 initialised, +0x1c reverse stereo. Ported in
`src/platform/pf_dsound.c` over `PfDsBackend` (IDirectSoundBuffer / IDirectSound methods and the
game's 0x004bb9c4); SDL2 backend `src/platform/pf_sdl_audio.c` (a software mixer: volume and
pan in hundredths of a dB as gains 10^(v / 2000), frequency as a resampling step, loop, status).
Proof: `tools/emu/check_pf_dsound.sh`: IDirectSound / IDirectSoundBuffer are fake COM objects
with hooked vtable entries (every call recorded: slot, this, arguments; scripted HRESULTs;
GetStatus / GetVolume write a scripted value), 0x004bb9c4 stubbed; the port's backend calls are
compared. The emulator FPU runs with the Win32 default control word 0x027f (53 bit precision), as
the game does; the port computes in double. 12 000 cases on seed 1 and 12 000 on seed 7, 61
branch combinations: all match.

Buffer wrappers: eax = device, edx = buffer, ebx = argument; a NULL buffer returns E_FAIL
0x80004005 without a call; otherwise the method's HRESULT.

| Address | Name | COM call | Port |
|---|---|---|---|
| 0x004b987c | dsbuf_play | IDirectSoundBuffer::Play (0x30)(buf, 0, 0, flags ebx) | pf_ds_play |
| 0x004b9898 | dsbuf_stop | Stop (0x48)(buf); buf != primary (+0x04): SetCurrentPosition (0x34)(buf, 0) (result dropped); returns Stop's | pf_ds_stop |
| 0x004b98cc | dsbuf_release (unlisted) | no IDirectSound: DSERR_UNINITIALIZED 0x887800aa; Release (0x08)(buf) | pf_ds_release_buffer |
| 0x004b98ec | dsound_release (unlisted) | no IDirectSound: E_FAIL; IDirectSound::Release (field not cleared) | pf_ds_release_device |
| 0x004b9908 / 0x004b9a48 | getters (unlisted) | +0x04 / +0x08 | pf_ds_primary / pf_ds_field8 |
| 0x004b990c | dsound_duplicate_buffer | (edx out, ebx source): needs IDirectSound and the source (out unchecked); IDirectSound::DuplicateSoundBuffer (0x14)(source, out) | pf_ds_duplicate |
| 0x004b992c | dsbuf_lock | (ebx bytes, ecx &ptr1, stack &len1, &ptr2, &len2; ret 0xc): Lock (0x2c)(buf, offset 0, bytes, ..., flags 0) | pf_ds_lock |
| 0x004b995c | dsbuf_unlock | (ebx ptr1, ecx len1, stack ptr2, len2; ret 8): Unlock (0x4c) | pf_ds_unlock |
| 0x004b9984 | dsbuf_set_position (unlisted) | SetCurrentPosition (0x34)(buf, ebx) | pf_ds_set_position |
| 0x004b999c | dsbuf_get_status | GetStatus (0x24)(buf, ebx) | pf_ds_get_status |
| 0x004b99b4 | dsbuf_set_volume | SetVolume (0x3c)(buf, ebx) | pf_ds_set_volume |
| 0x004b99cc | dsbuf_get_volume (unlisted) | GetVolume (0x18)(buf, &local); *ebx = local whatever the result (uninitialised on failure in the original; 0 in the port) | pf_ds_get_volume |
| 0x004b99f8 | dsbuf_set_pan | SetPan (0x40)(buf, +0x1c ? -ebx : ebx) | pf_ds_set_pan |
| 0x004b9a18 | dsbuf_set_frequency | SetFrequency (0x44)(buf, ebx) | pf_ds_set_frequency |
| 0x004b9a30 | dsbuf_get_frequency (unlisted) | GetFrequency (0x20)(buf, ebx) | pf_ds_get_frequency |

Sample math (sample record, 0x44 bytes: +0x18 base note, +0x20 bend range, +0x24 base rate, +0x40
buffer):

| Address | Name | Does | Port |
|---|---|---|---|
| 0x004ba76c | sample_set_pan | pan 0 -> 0; a = \|pan\| (INT_MIN stays negative): a <= 64: a * 125 * 0.125; a <= 96: a * 125 * 0.25 - 1000; else a * 250 - 22000 (PS1 0..127 to hundredths of a dB: 64 -> 1000, 96 -> 2000, 127 -> 9750); chop (0x004979a2 is frndint with rounding control = chop, not round), fistp (out of range -> 0x80000000), sign back; clamp > -10000 / < 10000; dsbuf_set_pan | pf_ds_pan_value, pf_ds_sample_set_pan |
| 0x004ba890 | sample_set_pitch | n = min(n, 99), no lower clamp; x = bend * n * 0.01 + note; i = chop(x); f = T[24-i] + (T[23-i] - T[24-i]) * (x - i), T = 0x004e86d0 (26 floats, C1 32.703 .. 138.59; T[24] = 130.81 is also the constant 0x004e8730); rate = chop(rate (unsigned 64 bit) * 130.81 / f); dsbuf_set_frequency. ORIGINAL BUG: i not range checked (the diff keeps -1 <= i <= 23). The table is game data: the port takes it from TM2.EXE at run time (`PfDsDevice.note_freq`) | pf_ds_pitch_value, pf_ds_sample_set_pitch |
| 0x004ba938 | sample_is_playing (unlisted) | dsbuf_get_status into *edx, then *edx = bit 0 (DSBSTATUS_PLAYING) | pf_ds_sample_is_playing |
| 0x004b9168 | sound_play_3d | (eax, edx, ebx passed on; ecx volume 0..99; stack distance, lateral, arg, unused; ret 0x10): dist = min(distance, [0x00c85040] max 5815); vol = (max - dist) * (volume * 127 / 99) / max (imul / idiv); pan = 0 at dist 0; \|lateral\| <= \|dist\|: chop(asin(lateral / \|dist\|) * 255 / pi) (asin 0x004c9eca = atan2(x, sqrt(1 - x x))); else lateral / 5; -> sfx_start 0x004bb9c4(eax, edx, ebx, vol; stack pan, arg) | pf_ds_play_at |

## Sound: the voice list's per slot update, ported (emulator diff)

`src/platform/pf_sfx.c`, proof `tools/emu/check_pf_sfx.sh` (instance maker 0x004bae50 and the
buffer wrappers stubbed and recorded; every 0..127 volume product seen: 127 distinct dB values,
-9688..0). 12 000 cases on seed 1 and 12 000 on seed 7, 53 branch combinations: all match.

| Address | Name | Does | Port |
|---|---|---|---|
| 0x004bbaf8 | sfx_slot_apply | instance = 0x004bae50(slot program, slot key) -> slot +0x04; none: flags = 0, return 1. Flag 4: v1 = instance volume * slot volume / 127; per voice v = voice volume * v1 / 127, dB = 2000 * ln(v / 127) (x87: fild, * 1/127, log 0x004c9f14, * 2000), min(dB, 0), chop, < -10000 -> -10000, chop, fistp -> dsbuf_set_volume 0x004b99b4(device, voice +0x40, dB). Flag 8: 0x004ba76c(voice, slot pan) per voice. Flag 0x10: 0x004ba890(voice, slot pitch). Flag 1: if no voice reports playing (dsbuf_get_status bit 0, stopping at the first that does): dsbuf_play every voice with DSBPLAY_LOOPING when voice +0x0c == 1; returns 1 if the last Play failed. Flags cleared on every path | pf_sfx_slot_apply, pf_sfx_volume_db |
| 0x004bbd80 / 0x004bbd88 / 0x004bbd90 / 0x004bbda4 / 0x004bbdb8 | request setters (unlisted) | or 1 / 2 / 4 (+0x10 = volume) / 8 (+0x14 = pan) / 0x10 (+0x18 = pitch) into the byte +0x0c; return 0 | pf_sfx_request_* |

ORIGINAL BUG (kept): 0x004c9f14 is the natural log (it loads ln 2 for fyl2x; its siblings use
log2 e / log10 2 by the al selector). DirectSound expects 2000 * log10 (hundredths of a dB), as
0x004ba68c / 0x004baaf8 compute: here the curve is 2.3 times steeper (volume 64 -> -1370 instead of
-595). It is how the game sounds, so the port keeps it.
Not diffed: a voice volume product <= 0 takes log's error path 0x004cd015 (the C runtime's
math error handler, which the emulator cannot run); the port returns -10000 there (assumes the handler
returns -HUGE_VAL, which the clamp turns into -10000: not verified). With no voices and a play request the original reads two
uninitialised stack words (ORIGINAL BUG); the port takes "not playing", result 0.

# Reference: every function of 0x004b0000..0x004c0000 by address range

## Part A: 0x004b270c..0x004b4374 (CRT helpers, mouse, GDI text, DirectDraw init / release)

Read from the machine code (capstone) against Ghidra's C. Watcom register convention: arguments
in eax, edx, ebx, ecx, then the stack; every function here ends in a plain `ret` (no stack
arguments) except 0x004b27e6, which is cdecl varargs (caller pops). Interface methods are named by
vtable slot (IDirectDraw2 / IDirectDrawSurface layouts). Several Ghidra names in this range were
wrong; the corrected name is given first and the old one in brackets.

Globals used below:

| Address | What |
|---|---|
| 0x00c84810 / 0x00c84814 / 0x00c84818 | virtual key of the primary / secondary / middle mouse button (1 / 2 / 4; 1 and 2 swapped when the user swapped the buttons). Ghidra labels 0x00c84818 `g_display_mode`: wrong, it is VK_MBUTTON |
| 0x004e86a8 | "read SwapMouseButtons from the registry" once flag (cleared after the first mouse init) |
| 0x004e86ac | the DDSCL flags last passed to SetCooperativeLevel (cache: the call is skipped when equal) |
| 0x00c848e0 | display mode list: count, then 16 byte entries from 0x00c848e4 (w, h, bpp, refresh) filled by EnumDisplayModes |
| 0x00c85004 | live DirectDraw surface count (decremented per surface Release) |
| 0x00c85008 | hardware 3D device path on (docs/DIRECTX.md, Correction) |
| 0x00bdfcd0 | the application object; [0x00bdfcd0] = the main HWND. 0x004a8f34 (Ghidra `widget_mgr_begin`: wrong) = `app_get_hwnd`, 0x004a6dcc (`ddraw_get_hwnd`) = its getter |
| 0x004bf830 | Ghidra `taskMgrUnlink`: wrong, it is `crt_memset(eax = dst, edx = byte, ebx = count)` |
| 0x004bc660 | Ghidra `menuPasswordDigitsCreate`: wrong, it is `error_box(int string_id, ...)` (cdecl): wvsprintfA, ShowCursor, SetClassLongA (background brush), MessageBoxA |

### 0x004b270c tex_pages_free [FUN_004b270c]

| | |
|---|---|
| Does | Frees every texture page buffer: for i < [0x004e85d8], `free([0x00be0350 + 4i] - 0x2000)` (each stored pointer is 0x2000 bytes into its heap block), then [0x004e85d8] = 0. Called by tpc_register 0x00496fd8 when not appending (replacing the texture set) |
| Arguments | none; ret |
| Calls | 0x004bf76f (CRT heap free) per buffer. No DirectX / Win32 |
| Error paths | none |
| Returns | nothing (eax = last free's result) |
| Portable equivalent | `free()` of the port's texture page buffers |
| Port status | documented only (heap bookkeeping; belongs to the texture page loader, not the platform layer) |

### 0x004b276e crt_utoa [FUN_004b276e]

| | |
|---|---|
| Does | Unsigned to text in any radix: repeated `div radix`, digits from the table at 0x004b2749 "0123456789abcdefghijklmnopqrstuvwxyz" into a 0x28 byte stack buffer (with a leading NUL), then copied reversed into buf. Value 0 gives "0" |
| Arguments | eax = value (u32), edx = buf, ebx = radix; ret |
| Calls | none |
| Error paths | none: radix 0 faults (divide error), radix > 36 reads past the table |
| Returns | eax = buf |
| Portable equivalent | `snprintf` for radix 10 / 16; a 10 line loop otherwise |
| Port status | ported, emulator diff: src/platform/pf2_misc.c, tools/emu/check_pf2_misc.sh |

### 0x004b27b8 crt_itoa [FUN_004b27b8]

| | |
|---|---|
| Does | Signed to text: only when radix == 10 and value < 0, writes '-' and negates; then crt_utoa into the next byte |
| Arguments | eax = value, edx = buf, ebx = radix; ret |
| Calls | 0x004b276e |
| Returns | eax = buf (the original pointer, before the '-') |
| Portable equivalent | `snprintf("%d")` (radix 10) |
| Port status | ported, emulator diff: src/platform/pf2_misc.c, tools/emu/check_pf2_misc.sh |

### 0x004b27e6 crt_sprintf [path_sprintf]

| | |
|---|---|
| Does | Watcom `sprintf(buf, fmt, ...)`: runs the formatter core 0x004c92ec (Ghidra `watcom_sprintf`) with the put char callback 0x004b27d3 (`*state->p++ = c; state->count++`) and the va_list, then writes buf[count] = 0. Used for save slot names ("Slot %01d"), paths, "%d %d" |
| Arguments | cdecl: [esp+4] buf, [esp+8] fmt, varargs; plain `ret`, caller pops |
| Calls | 0x004c92ec |
| Returns | eax = characters written (without the NUL) |
| DECOMPILER TRAP | Ghidra shows two arguments (buf, fmt) and drops the varargs; the name `path_sprintf` is a guess from one caller, it is the plain C sprintf |
| Portable equivalent | `vsnprintf` |
| Port status | documented only (libc) |

### 0x004b2820 mouse_init [ddraw_setup_display]

| | |
|---|---|
| Does | Fills a 0x34 byte mouse state (callers keep it on the stack) and the button key globals. Not display setup: the old name was wrong |
| Arguments | eax = mouse state; ret |
| State written | +0x00 present = GetSystemMetrics(SM_MOUSEPRESENT 0x13) != 0; +0x04 = GetSystemMetrics(SM_CMOUSEBUTTONS 0x2b) if present else 0; +0x08 / +0x0c cursor x / y = -1; +0x10 button bits, +0x14 / +0x18 / +0x1c left / right / middle down = 0; +0x20 / +0x24 = 640 / 480 (reference size, replaced by the screen size on poll); +0x28 = 0x7f (axis range), +0x2c = 0, +0x30 = 0x19 (axis dead zone; used by the mouse-as-axis helpers 0x004b2b84..0x004b2c9c) |
| Globals | [0x00c84818] = 4 (VK_MBUTTON) every call. First call only ([0x004e86a8] != 0): [0x00c84810] = 1, [0x00c84814] = 2, then registry HKEY_CURRENT_USER "Control Panel\Mouse" value "SwapMouseButtons" (up to 0x50 bytes) via reg_open / reg_open_subkey / reg_read_data / reg_flush (0x004ac4b0 / 0x004ac528 / 0x004ac5e4 / 0x004ac590); `lstrcmpA(value, "1") == 0` swaps them (2 / 1); [0x004e86a8] = 0 |
| Calls | GetSystemMetrics(0x13), GetSystemMetrics(0x2b), lstrcmpA |
| Error paths | registry key or value missing: keeps 1 / 2 |
| Returns | eax = the state pointer |
| Portable equivalent | SDL2 reports logical buttons (the OS applies the swap): `SDL_GetMouseState`, SDL_BUTTON_LMASK / RMASK / MMASK; "present" = 1 |
| Port status | **ported** (emulator diff): see "Input: the mouse module, ported" above |

### 0x004b2b00 mouse_get_centered_pos [FUN_004b2b00]

| | |
|---|---|
| Does | Cursor position relative to the reference size's centre, y up: x = cursor.x - (+0x20)/2, y = -(cursor.y - (+0x24)/2) (signed division by 2 rounding toward zero; client coordinates of the main window) |
| Arguments | eax = mouse state, edx = int* x out, ebx = int* y out; ret |
| Calls | app_get_hwnd 0x004a8f34; GetCursorPos(&pt) (pt zeroed first); ScreenToClient(hwnd, &pt) |
| Error paths | state +0x00 == 0 (no mouse): returns 0, outputs untouched; ScreenToClient fails: returns 0, outputs untouched |
| Returns | eax = ScreenToClient's BOOL (0 when no mouse) |
| Portable equivalent | `SDL_GetMouseState(&x, &y)` (window relative) then the same arithmetic |
| Port status | **ported** (emulator diff): see "Input: the mouse module, ported" above |

### 0x004b2ee0 mouse_primary_down [FUN_004b2ee0]

| | |
|---|---|
| Does | `(GetAsyncKeyState([0x00c84810]) & 0x8001) != 0`: primary button down now, or pressed since the last query |
| Arguments | none; ret |
| Calls | GetAsyncKeyState(vk) |
| Returns | eax = 0 / 1 |
| Note | 0x004b2f00 (not a separate function in Ghidra) is the same for the secondary button [0x00c84814] |
| Portable equivalent | `SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK`, plus a latched "pressed since last query" bit from SDL_MOUSEBUTTONDOWN events (the 0x0001 bit) |
| Port status | **ported** (emulator diff): see "Input: the mouse module, ported" above |

### Text style object (GDI text), the game's instance at 0x00510dd4

| Offset | Field |
|---|---|
| +0x00 / +0x04 / +0x08 | text colour R, G, B (i32, low byte used) |
| +0x0c / +0x10 / +0x14 | background colour R, G, B |
| +0x18 / +0x1c / +0x20 | shadow colour R, G, B |
| +0x24 | background mode (1 TRANSPARENT, 2 OPAQUE; setter 0x004b3b9c accepts only 1 or 2) |
| +0x2c / +0x30 | box origin used by the clipped text writer 0x004b3500 (ExtTextOutA, ETO_CLIPPED 4, clip = whole screen) |
| +0x34 / +0x38 | shadow on / shadow offset (x and y) |
| +0x3c / +0x40 | HFONT to select (0 = the DC's default font) / the previously selected font |

COLORREF built as `R | G << 8 | B << 16` from the low bytes. The DC is the offscreen surface's
(display object +0x14): 0x004b50a4 = IDirectDrawSurface::GetDC (slot 0x44), 0x004b50b8 =
IDirectDrawSurface::ReleaseDC (slot 0x68), both with ebx = 0 meaning "the default surface +0x14".
Neither text function checks GetDC's result (on failure the HDC is stack garbage).

### 0x004b31d0 text_draw [ddraw_draw_text]

| | |
|---|---|
| Does | Draws a string at (x, y) on the offscreen surface with GDI: optional shadow pass at (x + off, y + off) in the shadow colour, then the text in the text colour, background colour / mode from the style, style font selected around both |
| Arguments | eax = style, edx = string, ebx = x, ecx = y; ret |
| Calls (in order) | surface(+0x14)::GetDC(&hdc); lstrlenA(s); if font: SelectObject(hdc, font) -> +0x40; if shadow: SetBkColor(hdc, bg), SetTextColor(hdc, shadow), SetBkMode(hdc, +0x24), TextOutA(hdc, x+off, y+off, s, len); SetBkColor(hdc, bg), SetTextColor(hdc, text), SetBkMode(hdc, +0x24), TextOutA(hdc, x, y, s, len); if font: SelectObject(hdc, +0x40); surface::ReleaseDC(hdc) |
| Error paths | none checked |
| Returns | eax = the last TextOutA's BOOL |
| Portable equivalent | SDL2_ttf `TTF_RenderText_Shaded` (OPAQUE) / `TTF_RenderText_Blended` (TRANSPARENT) blitted into the 16 bit screen, twice when shadowed; or the game's own bitmap font |
| Port status | candidate for port: the layout (shadow offset, call order, colour packing) is game-visible; the glyph rendering itself is the backend's |

### 0x004b3330 text_draw_centered [FUN_004b3330]

| | |
|---|---|
| Does | As text_draw, centred: measures the string first, then draws at (x - cx/2, y - cy/2) (shadow at + off) |
| Arguments | eax = style, edx = string, ebx = x, ecx = y; ret |
| Calls | surface::GetDC; lstrlenA; GetTextExtentPoint32A(hdc, s, len, &size); then exactly text_draw's sequence with the centred coordinates; surface::ReleaseDC |
| ORIGINAL BUG | the extent is measured before the style's font is selected, so with a style font the centring uses the DC's default font metrics |
| Returns | eax = the last TextOutA's BOOL |
| Portable equivalent | `TTF_SizeText` + the draw above (keep the bug: measure with the default font) |
| Port status | candidate for port (with text_draw) |

### 0x004b3a44 text_width [ddraw_get_text_width]

| | |
|---|---|
| Does | Width in pixels of a string on the offscreen surface's DC |
| Arguments | eax = style (ignored: overwritten before use), edx = string; ret |
| Calls | surface::GetDC; lstrlenA; GetTextExtentPoint32A(hdc, s, len, &size); surface::ReleaseDC |
| ORIGINAL BUG | the style's font is never selected: the width is always that of the DC's default font |
| Returns | eax = size.cx |
| Portable equivalent | `TTF_SizeText(default_font, s, &w, NULL)` |
| Port status | documented only (one backend call) |

### 0x004b3a90 text_height [ddraw_get_text_height]

| | |
|---|---|
| Does | As text_width, returns size.cy (default font; style ignored). 0x004b3af0 (not a separate function in Ghidra) returns both, measuring twice |
| Arguments | eax = style (ignored), edx = string; ret |
| Returns | eax = size.cy |
| Portable equivalent | `TTF_SizeText(default_font, s, NULL, &h)` |
| Port status | documented only |

### 0x004b3b84 text_set_color [ddraw_set_text_color]

| | |
|---|---|
| Does | style+0x00 = edx (R), +0x04 = ebx (G), +0x08 = ecx (B) |
| Arguments | eax = style, edx = R, ebx = G, ecx = B; ret. (0x004b3b90, next to it, sets the background colour +0x0c..+0x14 the same way) |
| Returns | nothing |
| Portable equivalent | none needed (plain struct store) |
| Port status | ported, emulator diff: src/platform/pf2_misc.c, tools/emu/check_pf2_misc.sh |

### 0x004b3bac text_set_shadow [FUN_004b3bac]

| | |
|---|---|
| Does | style+0x34 = edx (shadow on), +0x38 = ebx (offset) |
| Arguments | eax = style, edx = on, ebx = offset; ret |
| Portable equivalent | none needed |
| Port status | ported, emulator diff: src/platform/pf2_misc.c, tools/emu/check_pf2_misc.sh |

### 0x004b3d80 ddraw_init

| | |
|---|---|
| Does | (Re)creates DirectDraw for the display object (eax, always 0x00c84840) and enumerates the display modes. edx = want_3d (1 from the startup call, 0 from the resolution menu) |
| Arguments | eax = display object, edx = want_3d; ret |
| Step 1 (first time, +0x08 == 0) | +0x54 = HWND ([0x00bdfcd0]); GetClientRect(hwnd, +0x78); ClientToScreen(hwnd, +0x78); ClientToScreen(hwnd, +0x80) (client rect in screen coordinates); hdc = GetDC(hwnd); GetSystemMetrics(SM_CXSCREEN 0), GetSystemMetrics(SM_CYSCREEN 1) (results unused); +0x88 = GetDeviceCaps(hdc, BITSPIXEL 12); ReleaseDC(hwnd, hdc) |
| Step 2 registry | crt_memset(guid[16], 0); crt_memset(+0x34, 0, 16); +0x50 = 0; reg_open; reg_open_subkey(HKEY_LOCAL_MACHINE 0x80000002, the game's key); on success: reg_read_data("DirectDraw GUID", guid, 16), reg_read_int("Renderer", +0x50), reg_flush |
| Early out | want_3d == 1 and Renderer (+0x50) == 0 and +0x08 != 0: return 0 (already set up, nothing to change) |
| Step 3 release old | if +0x08: 0x004b5ee8(obj) (3D device teardown), Release +0x30, Release +0x24 (zeroed). Then if +0x08 and cache != 8: IDirectDraw2::SetCooperativeLevel(hwnd, DDSCL_NORMAL 8) (cache = 8); 0x004b4374 (release surfaces); IDirectDraw2::RestoreDisplayMode (slot 0x4c) if +0x08; Release +0x08, Release +0x04 (zeroed) |
| Step 4 3D choice | +0x34 = IID_IDirect3DHALDevice {84e63de0-46aa-11cf-816f-0000c020156e} (copied from 0x004e5594); guid pointer = NULL when the registry GUID's first byte is 0 (see trap); [0x00c85008] = 1 and +0x70 = 1 when want_3d == 1 and Renderer == 1, else [0x00c85008] = 0; when 0, the guid pointer is NULL (the registry device is only used for the 3D path) |
| Step 5 create | DirectDrawCreate(guid or NULL, &+0x04, NULL); on DD_OK: IDirectDraw::QueryInterface(IID_IDirectDraw2 {b3a6f3e0-2b43-11cf-a2de-00aa00b93356} at 0x004e54e4, &+0x08), then IDirectDraw::Release (+0x04 = 0: only IDirectDraw2 is kept). (A SetCooperativeLevel(8) on +0x08 sits between them but +0x08 is always 0 there, so it never runs.) If that failed and a guid was given: the same again with NULL |
| Error paths | result 0x887600DE DDERR_NODIRECTDRAWSUPPORT: error_box(0x13), return it; 0x88760233 DDERR_NODIRECTDRAWHW: error_box(0x14), return it; any other failure: error_box(9, hr & 0xfff), return it |
| Step 6 modes | [0x00c848e0] = 0; IDirectDraw2::SetCooperativeLevel(hwnd, 0x17 = FULLSCREEN | ALLOWREBOOT | NOWINDOWCHANGES | EXCLUSIVE) if cache != 0x17; IDirectDraw2::EnumDisplayModes(DDEDM_REFRESHRATES 1, NULL, &0x00c848e0, callback 0x004b592c); SetCooperativeLevel(hwnd, 8) if cache != 8; qsort 0x004c1132(0x00c848e4, count, 16, compare 0x004b584c); 0x004b5874(obj, 0x00c848e0) (mode list post filter); if [0x00c85008]: 0x004b5bf0(obj) (3D device setup) |
| Returns | eax = 0, or the failing HRESULT |
| Fields | +0x04, +0x08, +0x24, +0x30, +0x34..+0x43, +0x50, +0x54, +0x70, +0x78..+0x87, +0x88 |
| ORIGINAL BUG | the "is the GUID all zero" loop (0x004b3f71) never advances its pointer: it tests the first byte 16 times, so a GUID whose first byte is 0 counts as "none". Ghidra's C shows the same loop faithfully |
| Portable equivalent | `SDL_InitSubSystem(SDL_INIT_VIDEO)`, `SDL_GetNumDisplayModes` / `SDL_GetDisplayMode` (w, h, bpp from the format, refresh_rate) into the same 16 byte list, sorted by the same compare; window from `SDL_CreateWindow`. Cooperative levels have no SDL equivalent (SDL_SetWindowFullscreen at mode set) |
| Port status | candidate for port: the decision logic (early out, 3D flag, GUID choice, fallback create, error mapping, the DDSCL cache 0x004e86ac, call order) is diffable with fake COM vtables + stubbed imports |

### 0x004b41a0 ddraw_release [ddraw_set_cooperative]

| | |
|---|---|
| Does | Tears the display down: if +0x08: 0x004b5ee8(obj), Release +0x30, Release +0x24 (zeroed); SetCooperativeLevel(hwnd +0x54, DDSCL_NORMAL 8) if +0x08 and cache != 8 (cache = 8); 0x004b4374 (release surfaces); RestoreDisplayMode if +0x08; if edx != 0: Release +0x08, Release +0x04 (zeroed) |
| Arguments | eax = display object, edx = release_interfaces; ret |
| Returns | nothing meaningful (eax = last call's) |
| Note | the old name was wrong: it does not set a cooperative level for use, it returns to DDSCL_NORMAL on shutdown. Step 3 of ddraw_init is the same code inlined |
| Portable equivalent | `SDL_SetWindowFullscreen(win, 0)`, destroy textures / renderer; `SDL_QuitSubSystem(SDL_INIT_VIDEO)` when releasing |
| Port status | candidate for port: call sequence and the cache are diffable |

### 0x004b42b8 cursor_unclip [ddraw_handle_activate]

| | |
|---|---|
| Does | ClipCursor(NULL) |
| Arguments | none; ret |
| Returns | eax = 0 |
| Portable equivalent | `SDL_SetWindowGrab(win, SDL_FALSE)` |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b42c8 ddraw_restore_and_clip [FUN_004b42c8]

| | |
|---|---|
| Does | After activation: if the primary (+0x0c) exists: restores the surfaces with 0x004b4508 while [0x00c85008] is forced to 0 (put back after); on the 3D path ([0x00c85008] != 0) recreates the surfaces for mode index +0x00: ddraw_create_surfaces 0x004b45a0(obj, edx = mode.w, ebx = mode.h, ecx = mode.bpp, stack = mode.refresh) from the 16 byte entry at 0x00c848e4 + 16*index; index out of [0, count): result E_FAIL 0x80004005. Then clips the cursor: +0x70 != 0: SetRect(&r, 0, 0, +0x58, +0x5c), ClipCursor(&r); else ClipCursor(NULL) |
| Arguments | eax = display object; ret |
| Calls | 0x004b4508, 0x004b45a0, SetRect, ClipCursor |
| ORIGINAL BUG | on the 3D path with a valid index the result of ddraw_create_surfaces is dropped and the function returns the caller's EDI (`mov ebx, edi` with edi only set on the error branch) |
| Returns | eax = 0 (no primary), the restore result (software path), E_FAIL (bad index) or the caller's EDI (see bug) |
| Note | the cursor clip rect is (0, 0, width, height) in screen coordinates, i.e. it assumes a full screen window |
| Portable equivalent | SDL needs no surface restore (textures survive on SDL_RENDER_TARGETS_RESET by re-upload); `SDL_SetWindowGrab(win, +0x70 != 0)` |
| Port status | candidate for port (the branch logic; the EDI return cannot be reproduced and nothing should rely on it) |

### 0x004b4374 ddraw_release_surfaces [FUN_004b4374]

| | |
|---|---|
| Does | 0x004b4c34 (detach / release the clipper +0x20); if +0x14 and +0x14 != +0x10: Release(+0x14), [0x00c85004]--; if +0x0c: Release(+0x0c) (the flip chain's back buffer +0x10 goes with it), [0x00c85004]--; if the last Release returned 0 (no references left): +0x0c = +0x10 = +0x14 = 0 |
| Arguments | eax = display object; ret |
| ORIGINAL BUG | the "last result" register (edx) starts as the caller's edx: with neither surface present the fields are cleared only if the caller's edx happened to be 0; and if a Release returns a nonzero refcount the pointers are left dangling |
| Returns | nothing meaningful |
| Portable equivalent | `SDL_DestroyTexture` of the screen texture(s) |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### Code in this range that Ghidra did not split into functions (not in the task list)

0x004b27d3 sprintf put char callback; 0x004b295c mouse_reread_swap (the registry read alone);
0x004b29e0 mouse_poll (GetAsyncKeyState of the three keys -> +0x10..+0x1c, cursor -> +0x08 / +0x0c
via GetCursorPos / ScreenToClient or -1, screen size -> +0x20 / +0x24); 0x004b2ad4 cursor client
position; 0x004b2b78 / 0x004b2b7c / 0x004b2b80 / 0x004b2c40 / 0x004b2ed8 / 0x004b2edc field getters;
0x004b2b84 / 0x004b2bdc / 0x004b2c44 / 0x004b2c9c mouse as an axis (right / left / down / up:
|pos - centre| * (range + dead) / half size, clamped to range + dead, minus dead, >= 0); 0x004b2d00 /
0x004b2d60 / 0x004b2db8 / 0x004b2e48 SetCursorPos in client coordinates (bounds checked against the
screen size); 0x004b2f00 secondary button down; 0x004b3500 clipped text (ExtTextOutA); 0x004b3adc
zero three ints; 0x004b3af0 text size (both); 0x004b3b90 set background colour; 0x004b3b9c set
background mode; 0x004b4254 ddraw_create2 (DirectDrawCreate + QueryInterface IDirectDraw2 into
caller pointers); 0x004b43cc release two interface pointers; 0x004b43fc RestoreDisplayMode wrapper.
These should be added to the function list for the next pass.

## DirectDraw surfaces, modes, window, and the hardware 3D (Direct3D 5) path: 0x004b4508..0x004b67d0

Read from the machine code (capstone) on 2026-11-21. Watcom register convention (eax, edx, ebx, ecx,
then stack; callee pops). `D` = the display object 0x00c84840 (every caller passes it in eax). COM
calls are `iface::Method(this, args...)` in the order DirectX declares them. HRESULTs:
DDERR_SURFACELOST = 0x887601c2, DDERR_HWNDALREADYSET = 0x8876023b, E_FAIL = 0x80004005.
`win32_show_error(code, hr & 0xfff)` is 0x004bc660 (cdecl, caller pops); `fatal_quit` is 0x004a8f24
(`game_quit_handler(0x00bdfaf0, 2)`, does not return to the game loop).

### Display object fields and globals found in this range (additions to docs/DIRECTX.md)

| Field / global | What | Evidence |
|---|---|---|
| D+0x00 | index of the current mode in the mode list (-1 = not in it) | 0x004b45a0 |
| D+0x18, +0x1c | two extra surfaces for a loading bitmap (dead code only) | 0x004b7c00 |
| D+0x24 | IDirect3D2 (QueryInterface of D+0x08 with IID_IDirect3D2 {6aae1ec1-662a-11d0-889d-00aa00bbb76a} at 0x004e5564) | 0x004b5bf0 |
| D+0x28 | IDirect3DDevice2 (IDirect3D2::CreateDevice on the offscreen surface D+0x14) | 0x004b5e00 |
| D+0x2c | IDirect3DViewport2 | 0x004b5e00 |
| D+0x30 | released with the 3D objects (created outside this range) | 0x004b5ca8 |
| D+0x34 | 16 byte GUID of the 3D device to create (getter 0x004b5f90) | 0x004b5e00 |
| D+0x44 / +0x48 | IDirect3DMaterial2: background (fog color) / default white material | 0x004b6320 |
| D+0x4c | D3DMATERIALHANDLE of D+0x48 (set as light state MATERIAL each scene) | 0x004b5f94 |
| D+0x64 / +0x68 | bits per pixel / refresh rate of the mode | 0x004b45a0, 0x004b598c |
| D+0x6c | page flipping wanted in full screen (setter 0x004b50f8, getter 0x004b50fc) | 0x004b4c80 |
| D+0x70 | **full screen** (1 by 0x004b4c80, 0 by the windowed path). DIRECTX.md's "window offset off" is this flag: the lock moves the pointer to the client rect only when windowed | 0x004b45a0, 0x004b4c80 |
| D+0x74 (byte) | present mode: 0 windowed Blt to client rect, 1 full screen Blt (no flip chain), 3 flip chain with a system memory render surface (Blt to back buffer, Flip), 4 flip chain drawn in place (Flip) | 0x004b48f4, 0x004b4b18 |
| D+0x78..+0x84 | client rect in screen coordinates (right = left + width, bottom = top + height) | 0x004b45a0 |
| D+0x8c | loading thread handle (dead code) | 0x004b7b34 |
| D+0x90 | last ShowCursor argument | 0x004b4a54 |
| D+0x94 / +0x98 / +0x9c | loading bitmap width / height / flag (dead code) | 0x004b7c00 |
| 0x00c848e0 (D+0xa0) | mode list: count, then 16 byte entries {w, h, bpp, refresh} from +4; room for 30 (0x1e4 bytes to the caps) | 0x004b592c |
| 0x00c84ac4 / 0x00c84c30 | DDCAPS hardware / emulation (0x16c bytes each) | 0x004b5874 |
| 0x00c84d9c..0x00c84da8 | caps flags: has 3D, bank switched video memory, stretch Blt, no 2D during 3D scene | 0x004b5874 |
| 0x00c84dac | Direct3D device caps block (two D3DDEVICEDESC + derived flags, 0x1c8 bytes) | 0x004b5ce8 |
| 0x00c84f74..0x00c84fdc | Direct3D render state cache (0x004b5bf0 sets the defaults) | 0x004b5fc8 |
| 0x00c84fe0 | D3DPRASTERCAPS_ZBUFFERLESSHSR | 0x004b5ce8 |
| 0x00c85000 | loading thread run flag (dead code) | 0x004b7b34 |
| 0x00c85004 | number of live DirectDraw surfaces (++ on create, -- on release) | 0x004b4828 |
| 0x00c85008 | hardware 3D renderer active | 0x004b5f60 |
| 0x004e86ac | last SetCooperativeLevel flags (8 NORMAL, 0x13 FULLSCREEN, ALLOWREBOOT and EXCLUSIVE, 0x17 adds ALLOWMODEX): a call with the cached value is skipped | 0x004b4c80 |
| 0x004e86b0 | last texture handle index set on the device (0x7fff = none) | 0x004b67d0 |

Note: several functions use the global 0x00c84840 directly instead of their `this` (0x004b5b40 reads
0x00c848b0 = D+0x70, the mode list is always 0x00c848e0): there is one display object.

### 0x004b4508 ddraw_restore_surfaces (was FUN_004b4508)

| | |
|---|---|
| Does | Restores lost surfaces after a mode switch / Alt-Tab |
| Arguments | eax = D; ret |
| Calls | IDirectDrawSurface::Restore(D+0x0c) if set; Restore(D+0x14) if set and not the back buffer D+0x10 (an attached back buffer is restored with the primary); if 3D (0x00c85008): 0x00497390 (re-upload the hardware textures, draws "Please Wait") |
| Returns | 3D on: 0x00497390's result (0 -> 0: the Restore results are dropped); 3D off: the last Restore's HRESULT. DECOMPILER TRAP / ORIGINAL BUG: 3D off with D+0x0c and D+0x14 both null returns the caller's edx (uninitialized). (Corrected by the emulator diff.) |
| Fields | D+0x0c, +0x10, +0x14 |
| Portable | nothing (SDL textures are not lost; with SDL_RENDERER: on SDL_RENDER_TARGETS_RESET re-create the streaming texture) |
| Port status | **ported** (emulator diff): see "Display: the ported wrappers" above |

### 0x004b45a0 ddraw_set_mode (was ddraw_create_surfaces)

| | |
|---|---|
| Does | Sets a video mode and rebuilds everything that depends on it |
| Arguments | eax = D, edx = width, ebx = height, ecx = bpp, stack: refresh (0 = any); ret 4 |
| Calls | 0x00495c94(0) clear all three buffers to black (fill 0x004b4d98 + present 0x004b5128, three times); if 3D: 0x004b5ee8 release device; 0x004b4374 release surfaces; find the mode in the list 0x00c848e4 (w, h, bpp equal, refresh equal unless 0) -> D+0 (-1 if absent); D+0x58 w, +0x5c h, +0x64 bpp, +0x68 refresh, D+0x80 = D+0x78 + w, D+0x84 = D+0x7c + h. Full screen (D+0x70): 0x004b4c80 enter full screen; on success ShowCursor per [0x00a5d370] (0x0049e108: cursor wanted), cached in D+0x90. Windowed: GetWindowLongA(hwnd, GWL_STYLE) & 0x7f35ffff \| 0x00ca0000 (WS_POPUP off; WS_CAPTION, WS_SYSMENU, WS_MINIMIZEBOX on) -> SetWindowLongA; IDirectDraw2::SetCooperativeLevel(hwnd, DDSCL_NORMAL) unless cached; 0x004b4374; IDirectDraw2::RestoreDisplayMode; 0x004b4b18 window surfaces; 0x004b4bb4 clipper. Then 0x004b5a94 resize window (client x, y, w, h); 0x0049628c pixel format (555 / 565 -> 0x00a2dba5); 0x00495c94(0) clear; if no error 0x004941f0 view_resize_all(w, h), 0x0049433c(0, 0). If 3D: 0x004b5e00 create device, 0x00497390 reload textures (either non zero is returned at once). ClipCursor(NULL) windowed, else SetRect(&r, 0, 0, w, h) + ClipCursor(&r). 0x004b598c measure refresh |
| Error paths | SetCooperativeLevel failure: win32_show_error(0x0e, hr) + fatal_quit |
| Returns | surface creation HRESULT (0 = ok) |
| Portable | `SDL_SetWindowSize` / `SDL_SetWindowFullscreen(SDL_WINDOW_FULLSCREEN)` with `SDL_SetWindowDisplayMode`, re-create the texture W x H RGB565 / RGB555, `SDL_SetWindowGrab` (ClipCursor), `SDL_ShowCursor` |
| Port status | candidate for port: the mode list search, the D field updates and the order of the game side calls (view resize, pixel format) are game visible |

### 0x004b480c ddraw_flip_to_gdi (was FUN_004b480c)

| | |
|---|---|
| Does | IDirectDraw2::FlipToGDISurface(D+0x08) if set (no caller: dead code) |
| Arguments / Returns | eax = D; returns the HRESULT, 0 without DirectDraw |
| Portable | nothing (`SDL_MinimizeWindow` if needed) |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b4828 ddraw_create_surface (was FUN_004b4828)

| | |
|---|---|
| Does | The one CreateSurface wrapper |
| Arguments | eax = D, edx = IDirectDrawSurface** out, ebx = width, ecx = height, stack: ddsCaps, back buffer count; ret 8 |
| Calls | DDSURFACEDESC (0x6c bytes, zeroed): dwSize 0x6c; dwFlags = DDSD_CAPS (1) if width and height are 0, else DDSD_CAPS \| DDSD_HEIGHT \| DDSD_WIDTH (7) with dwHeight / dwWidth; \| DDSD_BACKBUFFERCOUNT (0x20) with dwBackBufferCount if the count is non zero; ddsCaps.dwCaps = caps. IDirectDraw2::CreateSurface(D+0x08, &desc, out, NULL) |
| Returns | HRESULT; on success ++[0x00c85004] |
| Callers | 0x004b48f4, 0x004b4b18, 0x004a2f44 (outside), dead 0x004b7c00 |
| Portable | `SDL_CreateTexture(SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h)` or a malloc'd 16 bit buffer |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b48b8 ddraw_release_surface (was FUN_004b48b8)

| | |
|---|---|
| Does | `if (*p) { (*p)->Release(); --[0x00c85004]; } *p = 0` |
| Arguments | eax = D (only checked: returns 0 if D+0x08 is null), edx = IDirectDrawSurface** |
| Returns | Release's result (reference count), 0 if *p was null or no DirectDraw |
| Callers | 0x004a2fe5 |
| Portable | `SDL_DestroyTexture` |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

Unlisted, after it: 0x004b48e8 `edx->vtbl[3](edx, ebx)` (IDirectDrawSurface::AddAttachedSurface by slot), no caller.

### 0x004b48f4 ddraw_create_flip_chain (was FUN_004b48f4)

| | |
|---|---|
| Does | Full screen surfaces: primary with back buffers, back buffer pointer, optional system memory render surface |
| Arguments | eax = D; ret |
| Calls | caps = DDSCAPS_PRIMARYSURFACE \| FLIP \| COMPLEX (0x218), \| DDSCAPS_3DDEVICE (0x2000) in 3D mode. 1) unless [0x00c84da8] (NO2DDURING3DSCENE): 0x004b4828(&D+0x0c, 0, 0, caps \| VIDEOMEMORY (0x4000), 2 back buffers); 2) if that failed: same with 1 back buffer; 3) if failed: without VIDEOMEMORY, 1 back buffer. Then IDirectDrawSurface::GetAttachedSurface(D+0x0c, {DDSCAPS_BACKBUFFER 4}, &D+0x10); D+0x14 = D+0x10; D+0x74 = 4. GetCaps(D+0x10, &caps): if the back buffer is in video memory (0x4000) and video memory is bank switched ([0x00c84da0]): 0x004b4828(&D+0x14, W, H, DDSCAPS_SYSTEMMEMORY 0x800, 0); on success D+0x74 = 3 |
| Error paths | all three creates fail: release (0x004b4374), win32_show_error(0x0d, hr), fatal_quit. GetAttachedSurface fails: release, win32_show_error(0x0b, hr), fatal_quit. The system memory surface failing is silent (stays mode 4) |
| Returns | 0 |
| Portable | SDL presents with its own swap chain: one streaming texture, `SDL_RenderPresent` (vsync = flip) |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b4a54 ddraw_show_cursor (was FUN_004b4a54)

| | |
|---|---|
| Does | `if (show != D+0x90) { D+0x90 = show; ShowCursor(show); }` |
| Arguments | eax = D, edx = show (BOOL) |
| Callers | 0x00496198 |
| Portable | `SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE)` |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

Unlisted, after it: 0x004b4a70 ddraw_enter_window: D+0x70 = 0 and the windowed block of 0x004b45a0 (style, SetCooperativeLevel NORMAL, release, RestoreDisplayMode, 0x004b4b18, 0x004b4bb4); returns 0x004b4b18's result. No caller (the same code is inlined in 0x004b45a0).

### 0x004b4b18 ddraw_create_window_surfaces (was FUN_004b4b18)

| | |
|---|---|
| Does | Windowed (and full screen without flipping) surfaces |
| Arguments | eax = D; ret |
| Calls | 0x004b4828(&D+0x0c, 0, 0, DDSCAPS_PRIMARYSURFACE 0x200 [\| 3DDEVICE], 0); 0x004b4828(&D+0x14, D+0x58, D+0x5c, DDSCAPS_SYSTEMMEMORY 0x800 [\| 3DDEVICE], 0). D+0x74 = (D+0x70 != 0): 0 windowed, 1 full screen |
| Error paths | primary: win32_show_error(0x0c, hr) + fatal_quit; offscreen: win32_show_error(0x0f, hr) + fatal_quit |
| Returns | the offscreen create HRESULT |
| Portable | the W x H 16 bit streaming texture of sdl_display.c |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b4bb4 ddraw_attach_clipper (was FUN_004b4bb4)

| | |
|---|---|
| Does | Clipper for the primary so window Blts stay inside the client area |
| Arguments | eax = D; ret |
| Calls | if D+0x20 is null: IDirectDraw2::CreateClipper(D+0x08, 0, &D+0x20, NULL) (skipped without D+0x08), IDirectDrawClipper::SetHWnd(D+0x20, 0, D+0x54). Then IDirectDrawSurface::SetClipper(D+0x0c, D+0x20) if D+0x0c |
| Error paths | CreateClipper: win32_show_error(0x10, hr), return hr (not fatal); SetHWnd: win32_show_error(0x11, hr), return hr. ORIGINAL BUG: without D+0x08 SetHWnd is called on a null clipper |
| Returns | HRESULT of the last call |
| Portable | nothing (SDL clips to the window) |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b4c34 ddraw_detach_clipper (was FUN_004b4c34)

| | |
|---|---|
| Does | Takes the clipper off the primary and releases it |
| Arguments | eax = D |
| Calls | IDirectDrawSurface::GetClipper(D+0x0c, &tmp); if ok SetClipper(D+0x0c, NULL); IDirectDrawClipper::Release(D+0x20); D+0x20 = 0. ORIGINAL BUG (harmless): the reference GetClipper adds is never released |
| Returns | SetClipper / GetClipper HRESULT (caller ignores it) |
| Called by | 0x004b4374 (release surfaces) |
| Portable | nothing |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b4c80 ddraw_enter_fullscreen (was FUN_004b4c80)

| | |
|---|---|
| Does | Exclusive full screen at D+0x58 x D+0x5c x D+0x64 @ D+0x68 and its surfaces |
| Arguments | eax = D; ret |
| Calls | D+0x70 = 1; 0x004b4374 release; SetWindowLongA(hwnd, GWL_STYLE, GetWindowLongA(..) & 0x7f35ffff \| WS_POPUP); IDirectDraw2::SetCooperativeLevel(hwnd, 0x13 = DDSCL_FULLSCREEN \| ALLOWREBOOT \| EXCLUSIVE) unless cached in 0x004e86ac; RestoreDisplayMode; IDirectDraw2::SetDisplayMode(w, h, bpp, refresh, 0). Then: 3D: 0x004b48f4 flip chain + 0x004b4bb4; else D+0x6c: 0x004b48f4 + 0x004b4bb4; else 0x004b4b18 + 0x004b4bb4 |
| Error paths | SetCooperativeLevel fails other than DDERR_HWNDALREADYSET: win32_show_error(0x0e, hr) + fatal_quit. SetDisplayMode fails: win32_show_error(0x0a, w, h, bpp) + fatal_quit (the message shows the mode, not the HRESULT) |
| Returns | the surface creation result (0x004b48f4 returns 0) |
| Portable | `SDL_SetWindowDisplayMode` + `SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN)`; failure -> message + quit |
| Port status | documented only (part of the 0x004b45a0 port's backend) |

### 0x004b4e38 ddraw_fill_rect_fog (was FUN_004b4e38)

| | |
|---|---|
| Does | Fills a rectangle of the offscreen surface with the fog color |
| Arguments | eax = D, edx = RECT* (NULL = whole surface); ret |
| Calls | color = gpu_rgb24_to_16(fog_get_color()) & 0xffff (0x0049653c(0x00494dc0())); DDBLTFX (0x64 bytes, dwSize 0x64, dwFillColor +0x50); IDirectDrawSurface::Blt(D+0x14, rect, NULL, NULL, DDBLT_COLORFILL \| DDBLT_WAIT 0x01000400, &fx) |
| Returns | HRESULT; E_FAIL if D+0x14 is null |
| Callers | 0x00495b68, 0x00495b82 |
| Portable | fill the rectangle in the 16 bit buffer (or `SDL_FillRect` on an SDL_Surface) |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b4e98 d3d_clear_fog (was PutDispEnv / ddraw_set_exclusive: both wrong)

| | |
|---|---|
| Does | Hardware 3D only: clears the render target by drawing a full screen quad in the fog color |
| Arguments | eax = D; ret |
| Calls | four D3DTLVERTEX (32 bytes: sx, sy, sz, rhw, color, specular, tu, tv): (0,0), (W-0.1,0), (W-0.1,H-0.1), (0,H-0.1), sz = 10000.0f, rhw = 0, color = fog_get_color() \| 0xff000000. IDirect3DDevice2::SetRenderState(D3DRENDERSTATE_TEXTUREHANDLE 1, 0) (cache 0x004e86b0 = 0x7fff); SetRenderState(SHADEMODE 9, FLAT 1) unless cached in 0x00c84f74; 0x004b617c(D, 3) blending off; DrawPrimitive(D3DPT_TRIANGLEFAN 6, D3DVT_TLVERTEX 3, v, 4, 0); then the quad is shrunk to (0,0)-(2,2) and drawn ten more times |
| Returns | nothing useful |
| Callers | 0x0046c997 |
| Notes | the ten 2 x 2 draws look like a driver workaround (flush); the matcher's PutDispEnv pair was rejected already (tools/match/reject.tsv) |
| Portable | not ported (hardware 3D path); a GL / SDL_Renderer port would `SDL_SetRenderDrawColor(fog)` + `SDL_RenderClear` |
| Port status | documented only (hardware 3D path) |

### 0x004b50a4 ddraw_get_dc (was FUN_004b50a4)

| | |
|---|---|
| Does | IDirectDrawSurface::GetDC(surface, out) for GDI text |
| Arguments | eax = D, edx = HDC* out, ebx = surface (0 = D+0x14) |
| Returns | HRESULT |
| Callers | the GDI text functions 0x004b3085..0x004b3aa3 |
| Portable | none: text is drawn into the 16 bit buffer by the port's own font code (or SDL_ttf onto an SDL_Surface wrapping the buffer) |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b50b8 ddraw_release_dc (was FUN_004b50b8)

| | |
|---|---|
| Does | IDirectDrawSurface::ReleaseDC(surface, hdc) |
| Arguments | eax = D, edx = HDC, ebx = surface (0 = D+0x14) |
| Returns | HRESULT |
| Portable | none (see 0x004b50a4) |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b50cc ddraw_wait_vblank (was FUN_004b50cc)

| | |
|---|---|
| Does | IDirectDraw2::WaitForVerticalBlank(D+0x08, DDWAITVB_BLOCKBEGIN 1, NULL) |
| Arguments | eax = D |
| Returns | HRESULT, 0 without DirectDraw |
| Callers | 0x0049589f |
| Portable | `SDL_RenderPresent` with `SDL_RENDERER_PRESENTVSYNC`, or `SDL_Delay` to the next 1/refresh tick |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

Unlisted, after it: 0x004b50ec returns D if D+0x08 is set, else 0 (no caller).

### 0x004b50f8 ddraw_set_flip (was ddraw_set_display_mode: wrong)

| | |
|---|---|
| Does | D+0x6c = edx (page flipping wanted at the next full screen switch). 0x004b50fc (unlisted) returns D+0x6c |
| Arguments | eax = D, edx = flag |
| Callers | 0x0047719f, 0x00495b00 |
| Portable | a `vsync` flag of the backend |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b52b8 ddraw_present_rect (was FUN_004b52b8)

| | |
|---|---|
| Does | Present with an optional dirty rectangle and a blitter sync first |
| Arguments | eax = D, edx = RECT* (NULL or the whole screen {0,0,W,H} = full present); ret |
| Calls | local r = *rect, or SetRect(&r, 0, 0, W, H) for a full present. Returns 0 if D+0x0c is null. IDirectDrawSurface::IsLost(D+0x0c); DDERR_SURFACELOST -> 0x004b4508. Sync: Lock(D+0x0c, NULL, &desc, DDLOCK_WAIT 1, NULL) and, if it worked, Unlock(D+0x0c, NULL). Then by D+0x74, all Blts with DDBLT_WAIT (0x01000000) and no DDBLTFX. Full present with D+0x6c set: 0 Blt(primary, &D+0x78, D+0x14, NULL); 1 Blt(primary, NULL, D+0x14, NULL); 3 Blt(D+0x10, NULL, D+0x14, NULL) then Flip(D+0x0c, NULL, DDFLIP_WAIT 1); 4 Flip(D+0x0c, NULL, 1); 2 nothing. Otherwise (a rectangle, or D+0x6c clear): 0 Blt(primary, &D+0x78, D+0x14, NULL) (the rectangle is ignored); 1 Blt(primary, NULL, D+0x14, NULL); 3 Blt(primary, &r, D+0x14, rect); 4 Blt(primary, rect, D+0x14, rect) (copies the back buffer to the front, no flip) |
| Error paths | final HRESULT DDERR_SURFACELOST -> 0x004b4508 (and returned) |
| Returns | HRESULT of the Blt / Flip (or of the sync Lock in mode 2) |
| Callers | 0x00495b28 |
| Portable | `SDL_UpdateTexture(tex, rect, pixels, pitch)` + `SDL_RenderCopy` + `SDL_RenderPresent` |
| Port status | candidate for port: the rectangle normalisation and the D+0x74 / D+0x6c choice of target and rectangles are game visible (a COM call sequence diff like 0x004b5128's) |

Unlisted, after it (no callers, dead code kept by the linker):
- 0x004b54ac ddraw_copy_surface(D, dst eax.., dst surface edx, y ebx, x ecx, stack src surface, src RECT*), ret 8: locks both (DDLOCK_WAIT), copies rows with `rep movsd/movsb`; windowed primary destination offset by D+0x78 / +0x7c. ORIGINAL BUG: copies `width` bytes per row of 16 bit pixels (half the row). Unlocks both.
- 0x004b56c0 ddraw_set_coop(D, flags): SetCooperativeLevel with the 0x004e86ac cache.
- 0x004b56f4 ddraw_toggle_fullscreen -> 0x004b5704 ddraw_set_fullscreen(D, on): windowed only allowed when the desktop is 16 bit (D+0x88 == 16) and no 3D renderer (D+0x50 == 0); calls 0x004b45a0 with the current mode. 0x004b5750 returns D+0x70.
- 0x004b5754 ddraw_find_mode(w, h, bpp, stack refresh): the mode list search of 0x004b45a0 as a function, ret 4.
- 0x004b57c4 ddraw_enum_modes(D, IDirectDraw2*, list): SetCooperativeLevel 0x17 (FULLSCREEN \| ALLOWREBOOT \| ALLOWMODEX \| EXCLUSIVE), EnumDisplayModes(1 = DDEDM_REFRESHRATES, NULL, list, 0x004b592c), SetCooperativeLevel NORMAL, qsort(list+4, count, 16, 0x004b584c). Same code is inlined in ddraw_init 0x004b3d80.

### 0x004b584c mode_compare (was FUN_004b584c)

| | |
|---|---|
| Does | qsort comparator for mode list entries {w, h, bpp, refresh}: bpp, then width, then height (a - b, refresh ignored) |
| Arguments | eax, edx = entries; returns the difference |
| Portable | the same C comparator over `SDL_GetDisplayMode` results |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b5874 ddraw_get_caps (was FUN_004b5874)

| | |
|---|---|
| Does | DirectDraw capabilities into the block at 0x00c848e0 (edx) |
| Arguments | eax = D, edx = 0x00c848e0 |
| Calls | two DDCAPS (0x16c, dwSize set) at edx+0x1e4 (hardware) and edx+0x350 (emulation); IDirectDraw2::GetCaps(D+0x08, hw, hel). On success: edx+0x4bc = dwCaps & DDCAPS_3D (1); +0x4c0 = dwCaps & DDCAPS_BANKSWITCHED (0x08000000); +0x4c4 = dwCaps & DDCAPS_BLTSTRETCH (0x200); +0x4c8 = dwCaps2 & DDCAPS2_NO2DDURING3DSCENE (2) (flag names from ddraw.h of DirectX 5) |
| Returns | HRESULT (0 ok) |
| Called by | 0x004b417e (ddraw_init) |
| Portable | constants: no bank switching, stretch available (SDL_RenderCopy) |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b592c ddraw_enum_modes_cb (was FUN_004b592c)

| | |
|---|---|
| Does | EnumDisplayModes callback (stdcall, ret 8): keeps 16 bit modes no wider than 640 (800 with the 3D renderer) |
| Arguments | stack: DDSURFACEDESC*, list (count dword, then entries) |
| Logic | if ddpfPixelFormat.dwRGBBitCount (desc+0x54) == 16 and dwWidth (desc+0x0c) <= limit (unsigned): entry[count] = {dwWidth, dwHeight (+8), 16, dwRefreshRate (+0x18)}; count++. Returns DDENUMRET_OK (1) always. ORIGINAL BUG: no bound; more than 30 matching modes would overwrite the caps block at 0x00c84ac4 |
| Portable | `SDL_GetNumDisplayModes` / `SDL_GetDisplayMode`, keep 16 bit (or any; the port converts) modes with w <= 640 |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b5988 ddraw_get_refresh_rate (name right)

| | |
|---|---|
| Does | returns D+0x68 |
| Portable | `SDL_GetCurrentDisplayMode().refresh_rate` |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b598c ddraw_measure_refresh (was FUN_004b598c)

| | |
|---|---|
| Does | Refresh rate into D+0x68: ask DirectDraw, else time 50 flips |
| Arguments | eax = D |
| Calls | E_FAIL without D+0x08. D+0x68 = 0; IDirectDraw2::GetMonitorFrequency(D+0x08, &D+0x68). On failure and full screen (D+0x70 == 1): 0x00495c94(0) clear; present (0x00495b14 -> 0x004b5128) and timer_get_elapsed 0x00493038 for t0; then for n = 1..50: present, period = (t - t0) / n (unsigned); D+0x68 = 1000000 / period (timer in microseconds). Windowed and failing: 0 |
| Returns | D+0x68. ORIGINAL BUG: a period of 0 (no vsync in the flip) divides by zero |
| Called by | 0x004b47fa (end of 0x004b45a0) |
| Portable | `SDL_GetCurrentDisplayMode(..).refresh_rate`, 60 if 0 |
| Port status | candidate for port (the averaging arithmetic is game visible through the frame timer) |

### 0x004b5a20 ddraw_get_pixel_masks (was FUN_004b5a20)

| | |
|---|---|
| Does | Red, green, blue, alpha masks of the primary |
| Arguments | eax = D, edx = &r, ebx = &g, ecx = &b, stack: &a (each may be NULL); ret 4 |
| Calls | IDirectDrawSurface::GetSurfaceDesc(D+0x0c, &desc 0x6c); dwRBitMask desc+0x58, G +0x5c, B +0x60, alpha +0x64 |
| Returns | 0 always (the GetSurfaceDesc result is ignored) |
| Callers | 0x0049596e, 0x00496234, 0x0049628c (sets 0x00a2dba5: 0 = 555 (0x7c00/0x3e0/0x1f), 1 = 565 (0xf800/0x7e0/0x1f), 2 = other) |
| Portable | masks of the texture format (`SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_RGB565, ...)`) |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b5a94 ddraw_resize_window (was FUN_004b5a94)

| | |
|---|---|
| Does | Sizes the window to the mode |
| Arguments | eax = D, edx = x, ebx = y, ecx = w, stack: h; ret 4 |
| Calls | windowed (D+0x70 == 0): style = GetWindowLongA(hwnd, GWL_STYLE); SetRect(&r, x, y, x + w, y + h); AdjustWindowRect(&r, style, GetMenu(hwnd) != 0). Full screen: SetRect(&r, 0, 0, w, h). MoveWindow(hwnd, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE) |
| Returns | 0 |
| Called by | 0x004b4784 |
| Portable | `SDL_SetWindowSize(w, h)` (+ `SDL_SetWindowPosition`) |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b5b40 ddraw_set_client_origin (was FUN_004b5b40)

| | |
|---|---|
| Does | windowed only ([0x00c848b0] = D+0x70 of the global, not `this`): GetClientRect(hwnd, &r); SetRect(&this+0x78, x, y, x + r.right, y + r.bottom) |
| Arguments | eax = D, edx = x, ebx = y |
| Returns | 0. No caller (dead) |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

Unlisted, after it, no callers: 0x004b5b88 D+0x78 = client rect in screen coordinates (GetClientRect + ClientToScreen x2); 0x004b5bc4 D+0x78 = GetWindowRect; 0x004b5bdc copies D+0x78 to edx.

### 0x004b5bf0 d3d_init (was FUN_004b5bf0)

| | |
|---|---|
| Does | 3D renderer start: render state defaults and IDirect3D2 |
| Arguments | eax = D |
| Calls | memset(0x00c84f74, 0, 0x6c); cache defaults: [f74] SHADEMODE = 1 FLAT, [f78] FILLMODE = 3 SOLID, [f88] DITHERENABLE = 1, [f8c] TEXTUREPERSPECTIVE = 1, [f9c] ZFUNC = 4 LESSEQUAL, [fa0] TEXTUREMAPBLEND = 4 MODULATE, [fa4] TEXTUREMAG / MIN = 2 LINEAR, [fac] filtering on = 1, others 0 (antialias, specular, z enable, z write, blend). D+0x24..+0x30 = 0. IDirectDraw2::QueryInterface(D+0x08, IID_IDirect3D2, &D+0x24) |
| Returns | QueryInterface HRESULT |
| Called by | 0x004b418e (ddraw_init, when 0x00c85008) |
| Portable | not ported (hardware 3D path) |
| Port status | documented only (hardware 3D path) |

Unlisted, before / after: 0x004b5ca8 d3d_shutdown (0x004b5ee8, Release D+0x30, Release D+0x24; no caller); 0x004b5f68 set [0x00c85008]; 0x004b5f70 set it only when asked for 1 and the registry Renderer D+0x50 == 1 (then also D+0x70 = 1: hardware 3D forces full screen); 0x004b5f90 returns D+0x34.

### 0x004b5ce8 d3d_get_device_caps (was FUN_004b5ce8)

| | |
|---|---|
| Does | Device caps into 0x00c84dac (edx) |
| Calls | two D3DDEVICEDESC (0xcc bytes each, dwSize set): hardware at out, emulation at out+0xcc; IDirect3DDevice2::GetCaps(D+0x28, hw, hel). Then out+0x198 = hw.dcmColorModel != 0 (hardware device); from that desc (hardware if so, else emulation): out+0x1a4 = dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_SQUAREONLY (0x20); +0x1b0 / +0x1b4 min texture w / h; +0x1a8 / +0x1ac max texture w / h; +0x1b8 = dwDevCaps & D3DDEVCAPS_TEXTURENONLOCALVIDMEM (0x1000); +0x1bc / +0x1c0 / +0x1c4 = dwShadeCaps & ALPHAFLATSTIPPLED 0x2000 / ALPHAFLATBLEND 0x1000 / ALPHAGOURAUDBLEND 0x4000; [0x00c84fe0] = dwRasterCaps & ZBUFFERLESSHSR (0x8000) (names from d3dcaps.h of DirectX 5) |
| Returns | GetCaps HRESULT, 0 ok |
| Port status | documented only (hardware 3D path) |

### 0x004b5e00 d3d_create_device (was FUN_004b5e00)

| | |
|---|---|
| Does | Device, viewport, materials, render states |
| Calls | IDirect3D2::CreateDevice(D+0x24, &D+0x34 (device GUID), D+0x14 (render target), &D+0x28); 0x004b5ce8(D, 0x00c84dac); IDirect3D2::CreateViewport(D+0x24, &D+0x2c, NULL); IDirect3DDevice2::AddViewport(D+0x28, D+0x2c); D3DVIEWPORT2 (0x2c): dwX = dwY = 0, dwWidth = W, dwHeight = H, dvClipX = -1.0, dvClipY = H / W, dvClipWidth = 2.0, dvClipHeight = 2 H / W, dvMinZ = 0, dvMaxZ = 1.0; IDirect3DViewport2::SetViewport2(D+0x2c, &vp); IDirect3DDevice2::SetCurrentViewport(D+0x28, D+0x2c); 0x004b5fc8 apply states; 0x004b6320 materials |
| Error paths | CreateDevice / CreateViewport / AddViewport results ignored; SetViewport2 or SetCurrentViewport failure returns it |
| Returns | HRESULT (0x004b6320's on the full path) |
| Called by | 0x004b47b8 (0x004b45a0) |
| Port status | documented only (hardware 3D path) |

### 0x004b5ee8 d3d_release_device (was FUN_004b5ee8)

| | |
|---|---|
| Does | Release D+0x44, D+0x48 (materials); if D+0x2c: IDirect3DDevice2::DeleteViewport(D+0x28, D+0x2c) when the device exists, Release D+0x2c; 0x00497828 (free the texture table 0x00a2dc48, 0x200 entries of 24 bytes); Release D+0x28. Each pointer zeroed |
| Returns | 0 |
| Port status | documented only (hardware 3D path) |

### 0x004b5f60 d3d_is_active (was FUN_004b5f60)

| | |
|---|---|
| Does | returns [0x00c85008] (screen_is_split 0x004958ac is this as a bool: misnamed, see DIRECTX.md) |
| Portable | the port has no hardware 3D path: 0 |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b5f8c d3d_get_device (was FUN_004b5f8c)

| | |
|---|---|
| Does | returns D+0x28 |
| Callers | 0x004bfe6a, 0x004bff92 |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b5f94 d3d_begin_scene (was FUN_004b5f94)

| | |
|---|---|
| Does | IDirect3DDevice2::SetLightState(D+0x28, D3DLIGHTSTATE_MATERIAL 1, D+0x4c); BeginScene(D+0x28) |
| Callers | 0x0049634f |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b5fb8 d3d_end_scene (was FUN_004b5fb8)

| | |
|---|---|
| Does | IDirect3DDevice2::EndScene(D+0x28) |
| Callers | 0x00496372 |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b5fc8 d3d_apply_render_states (was FUN_004b5fc8)

| | |
|---|---|
| Does | Pushes the whole state cache to a new device |
| Calls | returns 1 without a viewport (D+0x2c); BeginScene (failure returned); SetRenderState: SHADEMODE [f74], TEXTUREPERSPECTIVE [f8c], ZENABLE [f94], ZWRITEENABLE [f98], ZFUNC [f9c], FILLMODE [f78], DITHERENABLE [f88], SPECULARENABLE [f90], ANTIALIAS [f84], CULLMODE = 1 NONE, TEXTUREMAG [fa4], TEXTUREMIN [fa4], TEXTUREMAPBLEND [fa0], ALPHABLENDENABLE [fa8], COLORKEYENABLE (41) = [0x00a2dc4c] <= 0; EndScene |
| Returns | EndScene HRESULT |
| Port status | ported, emulator diff: src/platform/pf2_dd.c, tools/emu/check_pf2_dd.sh |

### 0x004b611c d3d_set_filter (was FUN_004b611c)

| | |
|---|---|
| Does | Bilinear on / off, cached in [0x00c84fac]: TEXTUREMAG and TEXTUREMIN = 2 LINEAR (on) or 1 NEAREST (off) |
| Arguments | eax = D, edx = on |
| Returns | 0 if unchanged, else the last SetRenderState HRESULT |
| Port status | ported, emulator diff: src/platform/pf2_d3d.c, tools/emu/check_pf2_d3d.sh |

### 0x004b617c d3d_set_blend (was FUN_004b617c)

| | |
|---|---|
| Does | PS1 semi transparency mode to Direct3D blending |
| Arguments | eax = D, edx = mode 0..4 (3 = opaque) |
| Calls | ALPHABLENDENABLE (27) cached in [0x00c84fa8]: turned on for mode != 3, off for 3. Then SRCBLEND (19) / DESTBLEND (20), cached in [0x00c84fd8] / [0x00c84fdc] but always sent: 0 and 4: SRCALPHA 5 / INVSRCALPHA 6 (PS1 0.5B + 0.5F, and B + F/4 approximated the same); 1: SRCALPHA / ONE 2 (additive); 2: ZERO 1 / INVSRCALPHA (subtractive approximated); 3: nothing |
| Returns | last SetRenderState HRESULT, 0 for mode 3 without a change |
| Port status | ported, emulator diff: src/platform/pf2_d3d.c, tools/emu/check_pf2_d3d.sh |

Unlisted, after it: 0x004b62b4 set SRCBLEND / DESTBLEND directly (edx, ebx); 0x004b62e0 set SHADEMODE 1 / 2 with the 0x00c84f74 cache. No callers.

### 0x004b6320 d3d_create_materials (was FUN_004b6320)

| | |
|---|---|
| Calls | IDirect3D2::CreateMaterial(D+0x24, &D+0x44, NULL); 0x004b6428(D, fog color r, g, b); IDirect3DMaterial2::GetHandle(D+0x44, D+0x28, &h); IDirect3DViewport2::SetBackground(D+0x2c, h); CreateMaterial(&D+0x48); D3DMATERIAL (0x50): diffuse 1, 1, 1, 1, ambient 1, 1, 1 (alpha 0), dwRampSize 16; SetMaterial(D+0x48); GetHandle(D+0x48, D+0x28, &D+0x4c) |
| Returns | a failing CreateMaterial's HRESULT, else 0 |
| Port status | documented only (hardware 3D path) |

### 0x004b6428 d3d_set_background_color (was FUN_004b6428)

| | |
|---|---|
| Does | Background material diffuse = (r, g, b) / 255 (0.00392156862745098), dwRampSize 1; IDirect3DMaterial2::SetMaterial(D+0x44) |
| Arguments | eax = D, dl = r, bl = g, cl = b |
| Returns | E_FAIL without D+0x44; 0 without a viewport; else the HRESULT |
| Callers | 0x00494db8 (fog color change), 0x004b636f |
| Port status | documented only (hardware 3D path) |

Unlisted, after it: 0x004b64d0 (0x004b64d0..0x004b677d), no caller: builds a textured quad from the 0x00a30c4c texture table (index [0x00c84ff4]) when [0x00c84ff0] and draws it with SetRenderState x4 + DrawPrimitive. Dead code.

### 0x004b6780 d3d_draw_ot (was FUN_004b6780)

| | |
|---|---|
| Does | Walks a 0x1000 entry PS1 style ordering table from the far end (edx+0x3ffc down to edx) and draws every record with 0x004b67d0 |
| Arguments | eax = D (unused: 0x00c84840 is used), edx = OT base |
| Logic | for each slot from 0xfff down to 0: follow the chain (first dword of each record = next) until it reaches 0 or the next lower slot's address; each record -> 0x004b67d0(0x00c84840, record) |
| Returns | DECOMPILER TRAP: eax = the caller's edi (meaningless; the caller 0x00496396 ignores it) |
| Port status | ported, emulator diff: src/platform/pf2_d3d.c, tools/emu/check_pf2_d3d.sh |

### 0x004b67d0 d3d_draw_prim (was FUN_004b67d0): hardware 3D device path, documented only

| | |
|---|---|
| Does | Draws one queued GPU record through IDirect3DDevice2 (D+0x28) |
| Arguments | eax = D, edx = record; ret |
| Interface | IDirect3DDevice2 (DirectX 5), sure: slot 0x5c SetRenderState(state, value), 0x74 DrawPrimitive(type, D3DVT_TLVERTEX 3, verts, count, 0) with D3DTLVERTEX (32 bytes) vertices, and the device comes from IDirect3D2::CreateDevice (IID_IDirect3D2 in the data at 0x004e5564). No Direct3D DLL import is needed: IDirect3D2 is QueryInterface of the IDirectDraw2 |
| Dispatch | code = record byte +7 with bit 1 (semi transparency) cleared; semi = bit 1 |
| 0x80 / 0x82 | the device record from display_cmd_build 0x00491018: +4 u16 flags (bit 0 textured, bit 12 gouraud), +6 u8 vertex count, +8 texture index (& 0xfff), +0x0c blend mode, +0x14 D3DTLVERTEX[count]. Texture changed (cache 0x004e86b0): SetRenderState(TEXTUREHANDLE, [0x00a2dc48 + 24 i]) and 0x004b611c(filter = [0x00a30c74 + 48 i] == 0); untextured: TEXTUREHANDLE 0 (cache 0x7fff). SHADEMODE FLAT / GOURAUD by bit 12 (cached). Blend: semi -> 0x004b617c(+0x0c); textured with a color keyed texture ([0x00a30c58 + 48 i] and [0x00a2dc4c + 24 i] > 0) -> mode 4; else 3. DrawPrimitive(D3DPT_TRIANGLEFAN 6, TLVERTEX, rec+0x14, count, 0) |
| 0x20 / 0x28 | POLY_F3 / POLY_F4: flat, untextured; color alpha 0x80 if semi else 0xff; blend mode 0 if semi else 3; vertices built on the stack; TRIANGLEFAN of 3 / 4 |
| 0x24 / 0x2c | POLY_FT3 / POLY_FT4: textured (texture from the record, filter via 0x004b611c), TRIANGLEFAN 3 / 4 |
| 0x74 / 0x7c | SPRT_8 / SPRT_16: textured sprite quad, TRIANGLEFAN 4 |
| 0x40 / 0x50 | LINE_F2 / LINE_G2: D3DPT_LINESTRIP 3, 2 vertices |
| 0xc0 | game record (not a PS1 GPU code): line strip with a count from the record, LINESTRIP |
| other codes | nothing drawn |
| Calls per record | 0 to 4 SetRenderState (texture handle, filter, shade mode, blend), one DrawPrimitive |
| Returns | DrawPrimitive HRESULT |
| Port status | ported, emulator diff: src/platform/pf2_d3d.c, tools/emu/check_pf2_d3d.sh |

Unlisted code after it (0x004b7b26..0x004b7f90), no callers, dead: 0x004b7b34 starts a thread (CreateThread, D+0x8c, run flag 0x00c85000) running 0x004b7bc4 = loop { 0x004b7d9c(D, 0, -1, -1); Sleep(33) }; 0x004b7b84 stops it (flag 0, WaitForSingleObject INFINITE, CloseHandle); 0x004b7c00 loads a bitmap (LoadImageA(.., LR_CREATEDIBSECTION 0x2000), GetObjectA) into two system memory surfaces D+0x18 / +0x1c via GetDC + BitBlt(SRCCOPY) and sets a color key (SetColorKey DDCKEY_SRCBLT 8); 0x004b7d9c blits it. An animated loading screen that was left unused.

## Sound: 3D positioning, CD audio (MCI), DirectSound, movies (MCI avivideo), sound banks (0x004b7fa4..0x004bb03c)

Read from the machine code (capstone) with Ghidra's C as a guide. No DirectSound3D: the game positions
sounds itself (distance / lateral offset -> PS1 style volume 0..127 and pan) and converts to DirectSound
millibels at the buffer wrappers. Watcom register convention (eax, edx, ebx, ecx, then stack, callee pops).
`E_FAIL` = 0x80004005. All DirectSound buffer wrappers take the device object in eax (always
0x00c850ec) and the IDirectSoundBuffer in edx, and return the HRESULT.

### Objects and globals

| Object | Layout |
|---|---|
| DirectSound device 0x00c850ec (ctor 0x004b93b4, dtor 0x004b93cc registered in the Watcom init table) | +0x00 IDirectSound*; +0x04 primary IDirectSoundBuffer*; +0x08 (returned by 0x004b9a48 / 0x004b9a90, never written here); +0x0c 0 after init; +0x10 saved primary volume in percent (-1 = not read yet; ctor -1); +0x14 (1 from ctor; get 0x004b9be4 / set 0x004b9be0); +0x18 initialised flag; +0x1c reverse stereo (negate pan; ctor 0) |
| CD audio player 0x00c850bc (init 0x004b8574: +0x1c = 1, +0x20 = 0, +0x18 = 0) | +0x00 MCI device id; +0x04 from track; +0x08 to track (-1 = to the end); +0x0c aux volume saved at open (restored at close); +0x10 monitor thread handle; +0x14 thread id; +0x18 open flag; +0x1c "use aux volume" flag (set 0x004b8fc8, get 0x004b8fcc); +0x20 end of track callback (set 0x004b8990; 0 = replay) |
| Music object (C++ style, vtable 0x004e62ac over base vtable 0x004e632c of pure virtual stubs 0x004a6a39) | +0x04 current track (base ctor 0x004c9f04: -1). Slots: [0] 0x004b9380 (ret), [1] play track 0x004b92e0, [2] pause 0x004b931c, [3] resume 0x004b9334 |
| Sample (0x44 bytes) | +0x00 id; +0x04 volume 0..127; +0x0c loop flag (1 = DSBPLAY_LOOPING); +0x10 root note 0..24; +0x18 / +0x1c loop start / end (header), +0x20 = +0x1c - +0x18; +0x24 base frequency (Hz); +0x2c WAVEFORMATEX (tag 1, +0x2e channels, +0x30 rate, +0x34 bytes/s, +0x38 block align, +0x3a bits, +0x3c cbSize 0); +0x40 IDirectSoundBuffer* |
| Voice (0x18 bytes) | +0x00 volume 0..127; +0x04..+0x0c header; +0x10 sample count; +0x14 sample array (0x44 each) |
| Voice group (0x0c bytes) | +0x00 voice count; +0x04 voice array (0x18 each); +0x08 id per voice (int[count]) |
| 3D sound listener 0x00c8500c..0x00c8505c | +0c distance (nearest view); 10 distance (view 1); 14 / 18 view matrix / position pointers; 1c..24 sound - view position; 28..30 view space vector; 34 divisor (2); 38 / 3c 640 / -640; 40 max distance 5815; 44 0; 48 / 4c clamp +3357 / -3357; 50 5815; 54 500; 58 lateral x (nearest view); 5c lateral x (view 1) |
| Movie (MCI "mov" alias) | 0x00c8510c movie window handle; 0x00c85110 playing; 0x00c85114 open; 0x00c85118 double size; 0x00c8511c avivideo available |
| 0x00c850e0 | sound level 0..3 (3 at start up) |
| 0x00c850e8 | current CD track (2 after open) |

MCI codes used: 0x803 MCI_OPEN, 0x804 MCI_CLOSE, 0x806 MCI_PLAY (flags 4 MCI_FROM, 8 MCI_TO; TMSF track in
the low byte), 0x807 MCI_SEEK (8 MCI_TO), 0x808 MCI_STOP, 0x809 MCI_PAUSE, 0x80d MCI_SET (0x100 door open,
0x200 door closed, 0x400 time format = 10 MCI_FORMAT_TMSF), 0x814 MCI_STATUS with 0x100 MCI_STATUS_ITEM
(3 number of tracks, 4 mode, 5 media present). Modes: 0x20d stopped, 0x211 paused.

### 0x004b7fa4 sound_listener_defaults (old name audio_init_b)

| | |
|---|---|
| Does | Sets the 3D sound constants: 0xc85044 = 0, 0xc85048 = 3357, 0xc8504c = -3357, 0xc85038 = 640, 0xc8503c = -640, 0xc85034 = 2, 0xc85050 = 5815, 0xc85040 = 5815, 0xc85054 = 500 |
| Arguments / returns | none; returns 2 (eax = the divisor) |
| Calls | none |
| Portable | plain C (pf: same constants) |
| Port status | candidate (trivial, part of the 3D sound math) |

### 0x004b8010 sound_listener_locate (matcher name fxImpactParticlesDraw: WRONG)

| | |
|---|---|
| Does | Locates a world position relative to the camera(s) for 3D sound. edx = view (0 / 1) or -1 = both views (split screen): per view, vector = pos + view position (0x00494e64(view) -> 0x00a03aec + 12 view), times the view matrix (0x00494e84(view) -> 0x00a03b04 + 48 view) by math_mat_vec_mul_col 0x0048cb80 into 0xc85028; each component / [0xc85034] (idiv); lateral x (before clamping) -> 0xc85058 (view 1: 0xc8505c); components clamped to [0xc8504c, 0xc85048]; distance = integer square root (bitwise, 0x40000000 down) of the sum of squares -> 0xc8500c (view 1: 0xc85010). Both views: the nearer one wins (unsigned compare): 0xc8500c / 0xc85058 = view 1's |
| Arguments | eax = int32[3] world position, edx = view or -1; ret (no stack) |
| Calls | 0x00494e64, 0x00494e84, 0x0048cb80; no DirectX / Win32 |
| Returns | nothing useful (results in the globals) |
| Caller | 0x0046f977 (game sound code) |
| Portable | plain C |
| Port status | candidate for port (pure integer math; stub the two view getters and the matrix multiply) |
| Note | DECOMPILER TRAP-lite: Ghidra's -1 branch rewrites the square root loop with the inverted condition; equivalent |

### 0x004b84a0 sound_settings_defaults (old name audio_init_a)

| | |
|---|---|
| Does | Default sound setting block: 0xc85084 / 0xc85090 / 0xc85078 / 0xc85080 / 0xc8508c / 0xc85098 = 0x7fff; 0xc850a4 / 0xc850b4 = 0x7f; 0xc8507c / 88 / 94 / a8 / b8 / 68 / a0 = 0; 0xc85060 / 0xc85070 = 0x18; 0xc850ac / 0xc850b0 = 0x80; 0xc85074 = 0x2d; 0xc8509c = 99 |
| Returns | 0x2d |
| Callers | 0x00473e2e, 0x00477238 (both followed by 0x004b7fa4 and 0x004b92c0) |
| Port status | documented only (data init; plain C if needed) |

### 0x004b858c cd_open

| | |
|---|---|
| Does | Opens the CD audio device. edx = drive letter 'A'..'Z': MCI_OPEN with flags 0x3302 (MCI_OPEN_TYPE, TYPE_ID, ELEMENT, SHAREABLE, WAIT), type 0x204 (MCI_DEVTYPE_CD_AUDIO), element wsprintfA("%c:"); any other value: flags 0x2000, type "cdaudio". On success: +0x18 = 1, +0 = device id, +8 = -1, +4 = 1; aux device (0x004b8eac(cd, 1)) >= 0: auxGetVolume(dev, &cd+0x0c); +0x10 = 0; MCI_SET 0x400 time format TMSF (10). On failure +0x18 = 0 |
| Arguments | eax = CD player, edx = drive letter (from 0x00499958 = [0x00a3a5d0]) |
| Calls | memset 0x004bf830 (matcher name taskMgrUnlink: WRONG, it is memset), wsprintfA, mciSendCommandA(0, MCI_OPEN, ...), auxGetVolume, mciSendCommandA(id, MCI_SET, 0x400, {format 10}) |
| Returns | garbage (eax of the last call; callers use +0x18) |
| Portable | SDL2 has no CD audio: open the music tracks from files (SDL_mixer / own decoder) |
| Port status | documented only |

### 0x004b8694 cd_close

| | |
|---|---|
| Does | If open: if +0x1c, restore the aux volume +0x0c (auxSetVolume(aux dev, +0x0c)); MCI_CLOSE(id, 0, 0) |
| Arguments / returns | eax = CD player; returns eax unchanged |
| Port status | documented only |

### 0x004b86dc cd_play

| | |
|---|---|
| Does | If open: flags = MCI_FROM with from = (byte) +4 if +4 > 0; MCI_STATUS number of tracks; if +8 > +4 and +8 <= tracks (unsigned), MCI_TO with to = (byte) +8; MCI_PLAY. On success with no monitor thread: CreateThread(0, 0, 0x004b902c, 0, 0, &cd+0x14) -> +0x10 |
| Returns | MCI error (0 if not open) |
| Port status | documented only (portable: start the music stream of track +4) |
| Unlisted relatives | 0x004b87a8 play to +8 only (resume); 0x004b8818 pause (TerminateThread monitor, status mode, MCI_PAUSE unless stopped); 0x004b88b4 stop (TerminateThread, MCI_STOP); 0x004b88fc seek to track +4; 0x004b8930 / 0x004b8960 door open / closed; 0x004b8994 poll (see 0x004b8b40 case 8); 0x004b8a40 media present; 0x004b8a90 number of tracks |

### 0x004b8990 cd_set_end_callback

| | |
|---|---|
| Does | +0x20 = edx (called when a track stops; 0 = replay) |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b8a28 cd_set_tracks

| | |
|---|---|
| Does | If open: +4 = edx (from), +8 = ebx (to), return 1; else 0 |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b8ad4 cd_get_mode

| | |
|---|---|
| Does | If open: MCI_STATUS item 4 (mode) -> *edx on success; returns the MCI error (0 if not open) |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b8b40 cd_command (Ghidra: decompile failed, jump table at 0x004b8b1c)

| | |
|---|---|
| Does | edx (byte) = command 0..8: 0 open (0x004b858c with the CD drive letter), 1 play (0x004b86dc), 2 resume (play to +8 only, restart monitor), 3 pause, 4 stop, 5 seek to +4, 6 door open, 7 door closed, 8 poll (status mode: stopped -> callback +0x20 or command 1, paused -> command 2). > 8: nothing. On an MCI error: mciGetErrorStringA(err, buf, 0x80); error 0x123 shows the game's error box 0x004bc660(0) and retries the command while it returns 4 (IDRETRY) |
| Arguments | eax = CD player, edx = command |
| Returns | MCI error |
| Port status | documented only (portable: a music track player state machine) |
| Note | 0x004bc660 (matcher name menuPasswordDigitsCreate: WRONG) is the game's error MessageBoxA (cdecl, message table 0x004e8840, 12 bytes per entry) |

### 0x004b8eac cd_find_aux_device

| | |
|---|---|
| Does | Not open: 0. edx = 0: returns the MCI device id. Else: first aux device with wTechnology 1 (AUXCAPS_CDAUDIO), else first with 2 (AUXCAPS_AUXIN), else 1 |
| Calls | auxGetNumDevs, auxGetDevCapsA(i, &caps, 0x30) |
| Port status | documented only (no portable equivalent: the port has its own music volume) |

### 0x004b8f48 cd_set_volume (old name audio_set_volume)

| | |
|---|---|
| Does | If open and +0x1c: auxSetVolume(aux device, edx) (low word left, high word right); returns 1 ok / 0 failed / -1 no device; 0 if closed or flag off |
| Callers | 8 (options screen, 0x0046fd4f ...) |
| Portable | music stream gain |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |
| Unlisted | 0x004b8f8c get: auxGetVolume(aux, edx) |

### 0x004b8fc8 cd_set_use_aux_volume (old name audio_setup)

| | |
|---|---|
| Does | +0x1c = edx |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b902c cd_monitor_thread

| | |
|---|---|
| Does | Thread body, never returns: every 3 s (Sleep(3000)): if the CD player is open (0xc850d4): MCI_STATUS mode on 0xc850bc; stopped: call the callback 0xc850dc or cd_command(1) (replay); paused: cd_command(2) |
| Port status | documented only (portable: the music stream's end of track event) |
| Note | Ghidra calls the device id global g_audio_mixer: it is the CD player's MCI id (0x00c850bc +0) |

### 0x004b90d0 sound_startup

| | |
|---|---|
| Does | dsound_init(0x00c850ec) (edx = 3 passed and ignored), sound level 0xc850e0 = 3 |
| Callers | none by direct call (start up table) |
| Port status | documented only |
| Unlisted | 0x004b90e8 set primary format: Stop primary (0x004b9898), SetFormat (0x004b9808: channels edx, rate ebx, bits ecx, cbSize stack), Play looping; ret 4 |

### 0x004b9140 sound_bank_load thunk (matcher name sub_80144E78: wrong)

| | |
|---|---|
| Does | jmp 0x004bb1f0 (the sound bank loader, Ghidra name ddraw_blit_surface: WRONG; magic 0x03067027). 0x004b9148: jmp 0x004bb6c4 |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b9150 sound_get_level (old name audio_play_sfx: WRONG)

| | |
|---|---|
| Does | returns [0xc850e0]. Unlisted 0x004b9158: set it if 0 <= edx <= 3 |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b9168 sound_play_3d

| | |
|---|---|
| Does | Volume and pan from distance: d = min(stack1, [0xc85040]); vol = ((max - d) * ((ecx * 127) / 99)) / max (idiv); pan = 0 if d == 0; else if \|x\| <= \|d\| (x = stack2): chop(asin(x / \|d\|) * 255 * (1/pi)) (0x004c9eca asin, frndint 0x004979a2); else x / 5 (idiv). Then 0x004bb9c4(eax, edx, ebx, ecx = vol; stack pan, stack3) |
| Arguments | eax, edx, ebx passed through (bank / sound / voice); ecx = volume 0..99; stack1 distance, stack2 lateral x, stack3 passed through, stack4 unused; ret 0x10 |
| Calls | 0x004c9eca asin (x87), 0x004bb9c4 (play voice, other range) |
| Callers | 0x00472f68, 0x0047303b, 0x004730f8 |
| Portable | plain C (asin) + the voice play backend |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |
| DECOMPILER TRAP | Ghidra shows 7 parameters; the callee pops 4 stack words (ret 0x10) and stack4 is never read. Ghidra's "FUN_004c9eca(CONCAT22(...))" is asin of x/\|d\| (fld x; fdiv \|d\|) |

### 0x004b9240 sound_stop

| | |
|---|---|
| Does | group = 0x004bb738(eax, edx); ebx = -1: stop every voice of the group (0x004bafdc); else find voice ebx (0x004baf9c) and stop its samples (0x004baac0) |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b9268 / 0x004b9270 thunks

| | |
|---|---|
| Does | jmp 0x004bc5cc / jmp 0x004bc4e4 (other range). Unlisted 0x004b9278: voice playing? (0x004bb738, 0x004baf9c, 0x004badbc status == 1) |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b92b0 music_cd_ctor (old name audio_create_secondary)

| | |
|---|---|
| Does | base ctor 0x004c9f04 (+4 = -1, vtable 0x004e632c), then vtable +8 = 0x004e62ac (CD music) |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b92c0 music_cd_open (old name audio_init_secondary)

| | |
|---|---|
| Does | 0x004c9f00 (empty), cd_command(0x00c850bc, 0 open), current track 0xc850e8 = 2 |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b92e0 music_cd_play_track (vtable slot 1)

| | |
|---|---|
| Does | edx = track; -1: nothing. Else cd_set_tracks(track, track + 1), cd_command(1 play), 0xc850e8 = track, 0x004931fc (timer), obj+4 = track. Returns track |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b931c music_cd_pause (slot 2) / 0x004b9334 music_cd_resume (slot 3)

| | |
|---|---|
| Does | cd_command(3) / cd_command(2); return edx. Unlisted 0x004b934c: every 50th call cd_command(8 poll) |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b93cc dsound_device_dtor

| | |
|---|---|
| Does | If +0x18: primary volume restore: v = +0x10 (percent); mB = v == 0 ? -10000 : round(1000 * log10(v * 0.01)) clamped to >= -10000 -> SetVolume(primary) via 0x004b99b4; Stop(primary) [slot 0x48]; Release(primary) [0x08] if both pointers set; Release(IDirectSound) [0x08]; +0x18 = 0, +0 = 0, +4 = 0 |
| Returns | eax = the device |
| Note | ORIGINAL dead code: after Stop it compares the buffer with +4 (the same pointer) so SetCurrentPosition is never called |
| Portable | SDL_CloseAudioDevice |
| Port status | documented only |

### 0x004b94c4 dserr_filter

| | |
|---|---|
| Does | eax = edx (HRESULT); returns 0 if edx == 0 or edx == 0x88780096 (DSERR_BUFFERLOST), else edx |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b94f4 dsound_init

| | |
|---|---|
| Does | DirectSoundCreate(NULL, &dev+0, NULL); SetCooperativeLevel(ds, main hwnd 0x004a8f34(), 2 DSSCL_PRIORITY) [IDirectSound 0x18]; CreateSoundBuffer [0x0c] with DSBUFFERDESC {size 0x14, flags 0xc1 = PRIMARYBUFFER CTRLPAN CTRLVOLUME, bytes 0, wfx NULL} -> +4; SetFormat [IDirectSoundBuffer 0x38] WAVEFORMATEX {PCM, 2 channels, 11025 Hz, 44100 bytes/s, align 4, 16 bits, cbSize = low byte of the last HRESULT (0)}; Play(0, 0, DSBPLAY_LOOPING) [0x30]; +0x0c = 0; if +0x10 == -1: GetVolume [0x18] -> +0x10 = v / 100 + 100 (idiv); SetVolume(primary, 0) [0x3c]; +0x18 = 1 |
| Error paths | any failure returns the HRESULT at once (+0x18 stays 0); missing interface pointers give E_FAIL |
| Returns | HRESULT of Play |
| Portable | SDL_OpenAudioDevice(44100 or 11025 Hz, AUDIO_S16, 2 channels) with a mixer |
| Port status | documented only |
| Unlisted | 0x004b9780 DirectSoundCreate(NULL, eax, NULL); 0x004b9790 SetCooperativeLevel(ds, hwnd, edx) |

### 0x004b9674 dsound_close (old name audio_init_device: WRONG)

| | |
|---|---|
| Does | Same as 0x004b93cc (restore volume, Stop, Release primary, Release DirectSound) |
| Returns | 0 if not initialised; else Release's result (E_FAIL if no IDirectSound) |
| Callers | 0x004751f8, 0x004774c9, 0x00477545, 0x004775b9 (before movies: the movie player needs the sound card) |
| Port status | documented only |

### 0x004b97b0 dsound_create_buffer

| | |
|---|---|
| Does | CreateSoundBuffer [0x0c](ds, &{0x14, flags = stack1, bytes = ebx, 0, wfx = ecx}, out = edx, NULL) |
| Arguments | eax device, edx out pointer, ebx bytes, ecx WAVEFORMATEX*, stack flags; ret 4 |
| Returns | HRESULT; E_FAIL without IDirectSound |
| Portable | allocate a mixer channel buffer |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |
| Unlisted | 0x004b9808 SetFormat [0x38](edx, {PCM, ebx channels, ecx rate, rate * ch * (bits / 8), align (byte) ch * bits / 8, bits = stack1, cbSize = stack2}); ret 8 |

### 0x004b987c dsbuf_play

| | |
|---|---|
| Does | Play [0x30](buf, 0, 0, flags = ebx) |
| Returns | HRESULT; E_FAIL if buf == NULL |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |

### 0x004b9898 dsbuf_stop

| | |
|---|---|
| Does | Stop [0x48](buf); if buf is not the primary (+4): SetCurrentPosition [0x34](buf, 0) |
| Returns | Stop's HRESULT; E_FAIL if NULL |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |
| Unlisted | 0x004b98cc Release buffer (DSERR_UNINITIALIZED 0x887800aa without IDirectSound); 0x004b98ec Release IDirectSound; 0x004b9908 returns +4 (primary) |

### 0x004b990c dsound_duplicate_buffer

| | |
|---|---|
| Does | DuplicateSoundBuffer [IDirectSound 0x14](ds, original = ebx, out = edx) |
| Returns | HRESULT; E_FAIL if no IDirectSound or ebx == NULL |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |

### 0x004b992c dsbuf_lock

| | |
|---|---|
| Does | Lock [0x2c](buf, offset 0, bytes ebx, ecx = &ptr1, stack1 = &len1, stack2 = &ptr2, stack3 = &len2, flags 0); ret 0xc |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |

### 0x004b995c dsbuf_unlock

| | |
|---|---|
| Does | Unlock [0x4c](buf, ebx ptr1, ecx len1, stack1 ptr2, stack2 len2); ret 8 |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |

### 0x004b999c dsbuf_get_status (old name audio_check_playing)

| | |
|---|---|
| Does | GetStatus [0x24](buf, ebx = &status) |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |

### 0x004b99b4 dsbuf_set_volume

| | |
|---|---|
| Does | SetVolume [0x3c](buf, ebx millibels). Unlisted 0x004b99cc GetVolume [0x18] -> *ebx |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |

### 0x004b99f8 dsbuf_set_pan

| | |
|---|---|
| Does | SetPan [0x40](buf, device +0x1c ? -ebx : ebx) |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |

### 0x004b9a18 dsbuf_set_frequency

| | |
|---|---|
| Does | SetFrequency [0x44](buf, ebx Hz). Unlisted 0x004b9a30 GetFrequency [0x20]; 0x004b9a48 returns +8; 0x004b9a4c primary volume percent (GetVolume / 100 + 100) -> *edx; 0x004b9a90 set primary volume percent edx (same log10 formula as 0x004b93cc, constants 0x004e3740..); 0x004b9b34 reapply +0x10 |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |

### 0x004b9bf0 movie_init (old name audio_subsys_init)

| | |
|---|---|
| Does | mciSendStringA("open avivideo"); 0xc8511c = (result == 0). Returns the MCI error. Unlisted 0x004b9c1c movie_shutdown: "close mov" if open, "close avivideo" |
| Caller | 0x004a8b8e |
| Portable | a video decoder of the port's choosing (the .AVI files are Video for Windows) |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b9c7c movie_set_double_size

| | |
|---|---|
| Does | 0xc85118 = eax |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004b9c84 movie_set_window

| | |
|---|---|
| Does | 0x004958d8, 0x004958e4; "where mov source" -> parses the 3rd and 4th numbers (width, height); ebx / ecx = -1 take them; doubled if 0xc85118 == 1; wsprintfA("put mov window client at %i %i %i %i", eax, edx, w, h) |
| Port status | documented only |
| Unlisted | 0x004b9db0 "close mov" and clear 0xc85110 / 0xc85114 |

### 0x004b9de0 movie_open

| | |
|---|---|
| Does | Closes a previous movie; "open %s alias mov style child parent %d" (file eax, main hwnd 0x004a6dcc(0xbdfcd0)); ok: open = 1, movie_set_window(edx, ebx, ecx, stack), "window mov state show", "status mov window handle" -> 0xc8510c (atoi 0x004c9f83); errors: mciGetErrorStringA + error box 0x004bc660, open = 0 |
| Returns | 1 if it failed |
| Port status | documented only |
| Unlisted (0x004b9ee8..0x004ba15c) | play / play reverse ("play mov [reverse] window notify"), pause (volume "setaudio mov volume to %i" = [0x004e86cc] * 10, "pause mov"), seek to start / end, "step mov [reverse] by 1", "status mov mode" == "stopped" |

### 0x004ba15c movie_play (old name audio_load_track: WRONG)

| | |
|---|---|
| Does | Plays a movie and waits: needs 0xc8511c == 1 (else -1); 0 in hardware 3D mode (0x004958ac); flags (stack byte): 1 -> 0x00496184, 2 required; 0x0049364c; movie_open; volume [0x004e86cc] * 10; 0x004a8e6c; "play mov window notify" (callback hwnd); WaitMessage loop until 0xc85110 clears (MM_MCINOTIFY handler 0x004ba39c) or a key (0x00493748 poll, 0x00493694 timeout) unless flag 0x10; if still playing: "pause mov", seek to end; "close mov" |
| Returns | 1 if skipped by a key, else 0 |
| Port status | documented only |

### 0x004ba39c movie_notify_done

| | |
|---|---|
| Does | 0xc85110 = 0 (playing off) |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004ba4ec sample_create

| | |
|---|---|
| Does | WAVEFORMATEX at +0x2c from the file header (edx): tag 1, channels hdr+6, rate +8, bytes/s +0xc, align +0x10, bits +0x12, cbSize 0; ecx != -1: +0 = ecx; dsound_create_buffer(flags 0xea = CTRLVOLUME CTRLPAN CTRLFREQUENCY LOCSOFTWARE STATIC, bytes hdr+0x18, out +0x40); Lock(all), copy ebx data into both regions (rep movsd / movsb), Unlock |
| Returns | 0, or -1 if the create or the lock failed |
| Portable | copy PCM into a mixer sample |
| Port status | documented only |
| Unlisted | 0x004ba49c sample init (+0x40 = 0, +0 = -1); 0x004ba4ac / 0x004ba5ec release buffer; 0x004ba4c4 duplicate (copy +0, +0x2c..+0x3d, DuplicateSoundBuffer -> +0x40); 0x004ba604 sample_set_base_pitch: +0x24 = round(rate * 130.81 / table[24 - root +0x10]) (table 0x004e86d0: 25 floats, note frequencies 32.703 .. 130.81), SetFrequency; 0x004ba65c play (looping if +0x0c == 1); 0x004ba68c sample_set_volume: v = (+4 * edx) / 127, mB = round(2000 * log10(v / 127)) clamped to [-10000, 0], SetVolume; 0x004ba758 GetVolume; 0x004ba938 playing = GetStatus & 1 |
| DECOMPILER TRAP | Ghidra shows the copy loops reading uninitialised locals; they are the Lock outputs (ptr1, len1, ptr2, len2) |

### 0x004ba67c sample_stop (old name audio_load_sample: WRONG)

| | |
|---|---|
| Does | dsbuf_stop(0x00c850ec, sample +0x40) |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004ba76c sample_set_pan

| | |
|---|---|
| Does | PS1 pan p (edx) to DirectSound pan: p == 0 -> 0; a = \|p\|: a <= 64: a * 125 * 0.125; a <= 96: a * 125 * 0.25 - 1000; else a * 250 - 22000; frndint, fistp, sign restored; then clamp: <= -10000 -> -10000, >= 10000 -> 10000; dsbuf_set_pan (negated when device +0x1c) |
| Arguments | eax = sample, edx = pan |
| Calls | 0x004979a2 frndint; SetPan via 0x004b99f8 |
| Portable | SDL mixer channel pan from the same value |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |
| DECOMPILER TRAP | Ghidra's output is unusable (float_round() with no operands, a nonsense range test) |

### 0x004ba890 sample_set_pitch

| | |
|---|---|
| Does | n = min(edx, 99) (no lower clamp); x = +0x20 * n * 0.01 + +0x18 (x87); i = chop(x); f = table[24 - i] + (table[23 - i] - table[24 - i]) * (x - i) (table 0x004e86d0); freq = chop(+0x24 (as uint64) * 130.81 / f); SetFrequency (0x004b9a18) |
| Port status | **ported** (emulator diff): see "Sound: the DirectSound wrappers and sample math, ported" above |
| ORIGINAL BUG | no range check: i outside 0..23 indexes outside the 25 entry table (24 - i > 24 reads 0x004e8730 = 130.81 and beyond) |
| DECOMPILER TRAP | Ghidra's output drops all the x87 math |

### 0x004ba95c sample_read_header

| | |
|---|---|
| Does | 8 reads of 4 bytes (0x004c9ff0 = read(handle edx, buf, 4)) into +0, +4, +8, +0x10, +0x14, +0x0c, +0x18, +0x1c (that order); +0x20 = +0x1c - +0x18; returns bytes read (-1 results skipped) |
| Port status | documented only (file parsing belongs with the bank loader) |

### 0x004baac0 voice_stop

| | |
|---|---|
| Does | dsbuf_stop on each sample's buffer (count +0x10, array +0x14) |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004bac7c voice_read

| | |
|---|---|
| Does | 5 dwords (+0 .. +0x10) from the file; +0x14 = ebx (sample array); sample_read_header for each of +0x10 samples; returns bytes read |
| Port status | documented only |

### 0x004bae50 group_alloc_voice

| | |
|---|---|
| Does | id (edx) != -1 and a voice already has it: return that voice. Else the first voice none of whose samples is playing (GetStatus & 1) gets the id and is returned; none free: 0, or voice 0 when id == -1 |
| Port status | candidate (voice stealing logic; would need fake GetStatus results) |

### 0x004baf9c group_find_voice

| | |
|---|---|
| Does | returns the voice whose id == edx, else 0 |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004bafdc group_stop_all

| | |
|---|---|
| Does | dsbuf_stop on every sample of every voice; returns 0 |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004bb03c group_read

| | |
|---|---|
| Does | Reads the voice count (+0), id table = widget_mgr_alloc(count * 4) -> +8, +4 = ebx (voice array); reads voice 0 (voice_read, samples at ecx); voices 1..count-1 are copies of voice 0 with their own sample records (fields +0..+0x20, +0x28 copied; format / buffer filled later by the loader with DuplicateSoundBuffer) |
| Arguments | eax group, edx file handle, ebx voice array, ecx sample array |
| Returns | bytes read |
| Port status | documented only |

## 0x004bb1b0..0x004bff08: sound banks and the sound effect voice list, the CD / DirectX error box, Watcom runtime, Direct3D texture helpers

What this stretch really is (the old names were wrong for most of it):

- 0x004bb1b0..0x004bb738: the **sound bank** object (a PS1 VAB style bank: programs, tones, samples)
  and its loader. Not DirectDraw: `ddraw_create_wrapper`, `ddraw_cleanup`, `ddraw_blit_surface` were
  wrong names.
- 0x004bb9c4..0x004bc5cc: the **sound effect voice list** at 0x00c85124 (50 slots, a doubly linked
  list in priority order, a voice budget, per frame update that pushes volume / pan / pitch / play
  to DirectSound buffers through the 0x004b98xx..0x004b9a18 buffer wrappers on the sound device
  0x00c850ec).
- 0x004bc660: the error message box (CD audio, movie, ActiveMovie, DirectDraw, DirectSound, CD-ROM,
  DirectPlay, network errors): the matcher's `menuPasswordDigitsCreate` is wrong.
- 0x004bc710..0x004bf848: Watcom C runtime (strstr, startup, exit, file handle table hooks, x87
  trig helpers, malloc / free, memset, C++ array construct).
- 0x004bcb70: game code (a 2D grid lookup returning one of two triangles per cell), not platform;
  the matcher's `hdpPush` is wrong.
- 0x004bf920..0x004bff08: the **Direct3D texture** helpers of the hardware 3D device path
  (texture format choice through IDirect3DDevice2::EnumTextureFormats, texture size fitting,
  texture surface + palette creation). Documented only, like 0x004b67d0.

Sound device 0x00c850ec: +0x00 IDirectSound, +0x04 the primary IDirectSoundBuffer (the buffer
wrappers skip the rewind on it). DirectSoundBuffer slots used through the wrappers:
0x24 GetStatus (0x004b999c), 0x30 Play (0x004b987c), 0x34 SetCurrentPosition, 0x48 Stop
(both in 0x004b9898), 0x3c SetVolume (0x004b99b4), 0x40 SetPan (0x004b99f8), 0x44 SetFrequency
(0x004b9a18); IDirectSound 0x14 DuplicateSoundBuffer (0x004b990c). Every buffer wrapper returns
E_FAIL 0x80004005 for a null buffer.

### Sound bank structures

Bank object (0x1c bytes; one lives in the game flow object, constructed in game_flow_init 0x00477124):

| Off | What |
|---|---|
| +0x00 | program count |
| +0x04 | programs (new[] of 0x0c byte records, array descriptor 0x004e6318) |
| +0x08 | tone count |
| +0x0c | tones (0x18 byte records, descriptor 0x004e6304, built with the no-header array construct 0x004bf848) |
| +0x10 | sample count |
| +0x14 | samples (new[] of 0x44 byte records, descriptor 0x004e62f0) |
| +0x18 | wave count |

Program (0x0c): +0x00 tone count, +0x04 pointer to its first tone. Tone (0x18): +0x08, +0x0c (the
signed pair the voice list orders by, see 0x004bbdcc), +0x10 voice count (samples per instance).
Sample (0x44): +0x00 wave index, +0x04 volume 0..127, +0x0c loop flag (1 = loop), +0x10 root note
(0..24), +0x24 playback frequency (computed by the loader), +0x2c..+0x3d copied with a shared wave,
+0x30 the wave's sample rate, +0x40 IDirectSoundBuffer.

### 0x004bb1b0 sound_bank_construct (old name ddraw_create_wrapper: wrong)

| | |
|---|---|
| Does | zeroes the bank's seven dwords (+0x04..+0x18, then +0x00) |
| Arguments | eax = bank; ret |
| Calls | none |
| Returns | eax unchanged (bank) |
| Portable equivalent | `memset(bank, 0, sizeof *bank)` |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004bb1e4 sound_bank_destruct (old name ddraw_cleanup: wrong)

| | |
|---|---|
| Does | calls 0x004bb6c4 sound_bank_free, returns the bank (Watcom destructor shape) |
| Arguments | eax = bank; ret |
| Portable equivalent | the bank free below |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004bb1f0 sound_bank_load (old name ddraw_blit_surface: wrong)

| | |
|---|---|
| Does | loads a sound bank file into the bank and creates one DirectSound buffer per sample |
| Arguments | eax = bank, edx = file name; ret (no stack arguments) |
| Calls, in order | `open(name, 0x200 O_BINARY)` 0x004ca197; `filelength` 0x004ca372; `read` 0x004c9ff0 x5: magic (4), program count -> +0x00, tone count -> +0x08, sample count -> +0x10, wave count -> +0x18; `new` 0x004c4508 + array construct: programs (count*0xc + 4, descriptor 0x004e6318), tones (count*0x18, 0x004bf848), samples (count*0x44 + 4, descriptor 0x004e62f0); per program 0x004bb03c(program, fd, tone cursor, sample cursor) reads its records (tone cursor += tones*0x18, sample cursor += tone[0].+0x10 * tones * 0x44); `lseek(fd, 1, SEEK_CUR)` 0x004ca4e2 (one pad byte); `malloc(2 500 000)` 0x004bf680; `read(fd, buf, filelength - bytes read so far)`; walks the waves: each is a 0x1c byte header (+0x18 data size) then the data, pointers kept in two 70 entry stack arrays; `close` 0x004ca3cb; per sample: if an earlier sample has the same wave index, copy its wave index and +0x2c..+0x3d and `DuplicateSoundBuffer` (IDirectSound slot 0x14 via 0x004b990c, source = the earlier sample's +0x40, destination = this +0x40); else 0x004ba4ec(sample, wave header, wave data, -1) creates the buffer from the wave; then sample+0x24 = round(sample+0x30 (rate, as unsigned 64 bit) * 130.81f / notefreq[24 - sample+0x10]) and `SetFrequency` (slot 0x44 via 0x004b9a18) with it; finally `free(buf)` 0x004bf76f |
| Constants | magic 0x03067027; 130.81f at 0x004e8730 (C3); note table 0x004e86d0 = 25 floats, C1 32.703 .. C3 130.81 (equal temperament): the sample plays so that its root note sounds as C3 |
| Error paths | open fails: return -1. Magic read == 4 bytes and wrong: close, -1. Any of the four count reads short: close, -1. A failed program or tone allocation: -1 **without closing the file** (ORIGINAL BUG, handle leak). malloc fails: -1, file left open. |
| Returns | 0 on success, -1 (0xffffffff) on error |
| ORIGINAL BUGs (keep in a port's error behavior, or document the deviation) | a short read of the magic is not an error (only a read of 4 bytes with the wrong value fails); after the sample allocation it tests the **tone** pointer (+0x0c) again instead of the sample pointer (+0x14); the wave data read is not bounded by the 2 500 000 byte buffer; more than 70 waves overflow the stack arrays; a root note outside 0..24 reads outside the note table |
| DECOMPILER TRAP | Ghidra drops the frequency expression (shows only `float_round`): the machine code is `fild qword [rate:0]; fmul [0x004e8730]; fdiv [0x004e86d0 + 4*(0x18 - root)]`, rounded by 0x004979a2, `fistp`. Ghidra also loses the open / filelength / read arguments (register and cdecl stack mix). |
| Portable equivalent | `fopen` / `fread` (or the port's file layer) + SDL2 audio: one decoded PCM buffer per wave, sharing for samples with the same wave index (no duplicate needed: SDL voices reference the same PCM), the frequency kept as the voice's playback rate (resampled by the mixer) |
| Port status | candidate for port: the parse and the frequency rule are pure; the buffer creation is the backend's `pf_audio_create_buffer` / `pf_audio_duplicate` / `pf_audio_set_frequency` |

### 0x004bb6c4 sound_bank_free

| | |
|---|---|
| Does | if programs: delete[] them (array destruct 0x004a9ff8 with descriptor 0x004e6318, then operator delete 0x004aa018), +0x04 = +0x00 = 0; if tones: operator delete (no element destructors), +0x0c = +0x08 = 0; if samples: delete[] with descriptor 0x004e62f0 (the sample destructor, which releases the DirectSound buffer), +0x14 = +0x10 = 0 |
| Arguments | eax = bank; ret |
| Returns | 0 |
| Portable equivalent | `free` of the three arrays and `pf_audio_release_buffer` per sample |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### 0x004bb738 sound_bank_program

| | |
|---|---|
| Does | returns &programs[index] if index < program count (signed compare), else 0 |
| Arguments | eax = bank, edx = index; ret |
| Returns | pointer or 0. A negative index passes the check (ORIGINAL BUG: pointer before the array) |
| Portable equivalent | plain C |
| Port status | ported, emulator diff: src/platform/pf2_snd.c, tools/emu/check_pf2_snd.sh |

### The voice list at 0x00c85124

| Off | What |
|---|---|
| +0x000 | active count |
| +0x004 + 0x28*i | slot i record (i = 2..51; 0 = head node, 1 = tail node) |
| +0x824 + 4*i | slot in use (1) |
| +0x8f4 + 4*i | next[i]; next[0] (0x00c85a18) = first, the walk ends at 1 |
| +0x9c4 + 4*i | prev[i]; prev[1] (0x00c85b4c... read as +0x9c8) = last |
| +0xa94 | voice budget (0x00c85bb8; set to 0x18 = 24 at 0x004ba487) |

Slot record (0x28): +0x00 program, +0x04 instance (from 0x004bae50: +0x00 volume, +0x10 voice
count, +0x14 voices of 0x44 bytes, +0x40 the buffer), +0x08 key (caller's id), +0x0c flags
(1 play, 2 stop, 4 volume, 8 pan, 0x10 pitch), +0x10 volume 0..127, +0x14 pan, +0x18 pitch,
+0x1c voice count (tone[0]+0x10), +0x20 / +0x24 the signed order pair (tone[0]+0x0c / +0x08).

### 0x004bb9c4 sfx_start

| | |
|---|---|
| Does | finds or allocates the list slot for (bank program index, key) with 0x004bbdcc; sets volume, pan, pitch and flags \|= 0x1d (play, volume, pan, pitch) |
| Arguments | eax = bank, edx = program index (no bounds check: &programs[edx]), ebx = key, ecx = volume, stack: pan, pitch; `ret 8` |
| Calls | 0x004bbdcc(0x00c85124, program, key) |
| Returns | 1 if no slot, else 0 |
| Caller | 0x004b9168 (computes the volume from distance: ((max - min(d, max)) * (v * 127 / 99)) / max with max = 0x00c85040, and the pan) |
| Portable equivalent | plain C over the list; the DirectSound work happens in 0x004bbaf8 |
| Port status | candidate for port (pure list logic) |

### 0x004bbaf8 sfx_slot_apply

| | |
|---|---|
| Does | for one slot: (re)creates its instance with 0x004bae50(program, key) (instance -> +0x04); then by flags: 4 = volume: for each voice, v = (instance+0x00 * slot volume / 127) then (voice+0x04 * v) / 127, dB = 2000 * **ln**(v / 127), clamped to <= 0 and > -10000 else -10000, rounded, `SetVolume` (0x004b99b4, slot 0x3c); 8 = pan: 0x004ba76c(voice, slot pan) for each voice; 0x10 = pitch: 0x004ba890(voice, slot pitch); 1 = play: if no voice reports playing (`GetStatus` slot 0x24 via 0x004b999c, bit 0), `Play` (0x004b987c, slot 0x30, flags = voice+0x0c == 1 ? DSBPLAY_LOOPING : 0) on every voice; flags = 0 |
| Arguments | eax = slot record; ret |
| Returns | 1 if the instance could not be made or the last Play returned non zero; else 0 |
| ORIGINAL BUG (keep) | the volume uses the natural log, not log10 (0x004c9f14 is called with al = 0x0a which selects `fldln2`; Ghidra says the log10 / log2 blocks are unreachable): DirectSound hundredths of dB would be 2000*log10. Volumes fall 2.3 times faster than intended; a port must keep 2000*ln for the same loudness curve. With no voices, the "playing" flag and the Play result are read uninitialized from the stack. |
| DECOMPILER TRAP | Ghidra shows the dB computation but passes `extraout_EAX` to 0x004b99b4: the machine code passes eax = 0x00c850ec, edx = voice+0x40, ebx = the chopped dB. The play loop's flag is `[esp]`, written through ebx = esp by GetStatus. |
| Portable equivalent | SDL2 mixer state per voice: gain = 10^(dB/2000) linear (dB from the same ln formula), pan, rate; `pf_audio_play(buf, loop)` |
| Port status | **ported** (emulator diff): see "Sound: the voice list's per slot update, ported" above |

### 0x004bbdcc sfx_slot_find

| | |
|---|---|
| Does | walks the list for a slot with the same program and key: returns it. Else: if count < 50 (0x32): insertion point = first slot from the head whose pair is "negative" (+0x24 <= 0 and +0x20 < 0), or the tail node 1; if full: the last slot (prev[1]); if the list is empty, return 0; if that slot's pair is negative (+0x24 < 0, or +0x24 == 0 and +0x20 < 0) it is evicted (each voice stopped with 0x004ba67c, instance = 0, unlinked, count--) and the insertion point is its successor; then 0x004bc31c allocates a free slot before the insertion point; on success the record is set: program, instance 0, key, flags 0, +0x1c/+0x20/+0x24 from the program's first tone (+0x10 / +0x0c / +0x08; a program with no tones reads address 0x10: ORIGINAL BUG) |
| Arguments | eax = list, edx = program, ebx = key; ret |
| Returns | the slot record or 0 |
| DECOMPILER TRAP | Ghidra's `if (iVar1 < 0) return 0` after the free search is dead: the walk ends at node 1, which is a valid insertion point (append) |
| Portable equivalent | plain C |
| Port status | candidate for port (pure, game visible: which sounds survive when 50 are active) |

### 0x004bc31c sfx_slot_alloc

| | |
|---|---|
| Does | first slot 2..51 whose in use flag is not 1: in use = 1, linked before `before` (next = before, prev = prev[before], prev[before] = slot, next[old prev] = slot), count++ |
| Arguments | eax = list, edx = before; ret |
| Returns | slot index, or -1 when all 50 are in use |
| Portable equivalent | plain C |
| Port status | candidate for port (pure) |

### 0x004bc4e4 sfx_update (called each frame through 0x004b9270)

| | |
|---|---|
| Does | eax = 0x00c85124 and enters the list update. Pass 1, every slot: flags & 2 (stop): `Stop` + rewind (0x004b9898) every voice of the instance, then 0x004ba67c on every voice, unlink; else if it has an instance and no voice is playing (GetStatus): stop and unlink (the sound ended). Pass 2, from the head: sum the slots' voice counts (+0x1c); a slot that takes the sum over the budget (+0xa94) is stopped and unlinked; else 0x004bbaf8 applies its pending changes, and if that fails the slot is stopped and unlinked; the walk stops once the sum reaches the budget. Pass 3: every slot after that point is stopped and unlinked. |
| Arguments | none; ret |
| Returns | 0 |
| DECOMPILER TRAP | the entry is 8 bytes (`mov eax, 0x00c85124` + filler) and the body jumps into blocks at 0x004bbf..0x004bc2.. that belong to an unlisted twin taking the list in eax (Watcom shared the code); Ghidra's labels LAB_004bc081 etc. lie before the function's entry |
| Portable equivalent | plain C plus `pf_audio_is_playing` / `pf_audio_stop` |
| Port status | candidate for port (pure list logic with the buffer calls through the backend) |

### 0x004bc5cc sfx_stop_all (called through 0x004b9268 from render_frame 0x0047312c)

| | |
|---|---|
| Does | while the list is not empty: the first slot's instance, if any, gets 0x004baac0 (Stop + rewind every voice); instance = 0, in use = 0, unlinked, count-- |
| Arguments | none; ret |
| Returns | 0 |
| Portable equivalent | plain C + `pf_audio_stop` |
| Port status | candidate for port (with the list) |

### 0x004bc660 win32_error_box (matcher name menuPasswordDigitsCreate: wrong; Ghidra's old name win32_show_error)

| | |
|---|---|
| Does | formats an error message from a table and shows it in a message box |
| Arguments | cdecl, variadic: stack[0] = error index (only its low byte is used, as a signed char), then the format's arguments; plain `ret` (caller pops) |
| Calls | `wvsprintfA(buf[800], table[i].format, &args)`; `ShowCursor(TRUE)`; 0x004a8f34 (the main window handle); `GetStockObject(4 BLACK_BRUSH)`; `SetClassLongA(hwnd, GCL_HBRBACKGROUND -10, brush)` (old value kept); `MessageBoxA(hwnd, buf, table[i].caption, table[i].type)`; `ShowCursor(FALSE)`; `SetClassLongA(hwnd, -10, old)` |
| Table | 0x004e8840, 12 bytes per entry {MessageBox type, caption, format}, 32 entries: 0..1 CD audio (type 0x35 = MB_RETRYCANCEL \| MB_ICONEXCLAMATION), 2..3 movie, 4..8 ActiveMovie, 9..20 DirectDraw (create, unsupported mode %i x %i in %i bit, back buffer, primary, back buffers, cooperative level, system memory buffer, clipper create / SetHWnd / attach, not supported, no hardware), 21 DirectSound (game continues without sound effects), 22 CD-ROM not in drive (0x35), 23..27 DirectPlay, 28..31 network; all others 0x30 = MB_OK \| MB_ICONEXCLAMATION |
| Returns | MessageBox's result (IDRETRY 4 / IDCANCEL 2 / IDOK 1) |
| DECOMPILER TRAP | Ghidra's `in_stack_00000001 >> 0x18` is `[esp+0x32d] sar 24`: the first stack argument's low byte, sign extended |
| Callers | 20 sites: ddraw_init, ddraw_create_surfaces, 0x004b48f4, 0x004b4a54, the CD audio and movie code, the shell |
| Portable equivalent | `SDL_ShowMessageBox` (buttons Retry / Cancel for types 0x35, OK otherwise) with `SDL_ShowCursor(SDL_ENABLE)` around it; the texts are game data: read them from the player's TM2.EXE at runtime or use the port's own wording |
| Port status | candidate for port (`pf_error_box(index, ...)`): backend only, no game state |

### 0x004bc710 strstr (Watcom runtime)

| | |
|---|---|
| Does | C `strstr(haystack eax, needle edx)`: empty needle returns the haystack; one character needle scans two bytes at a time; else `repne scasb` / `repe cmpsb` |
| Returns | pointer or 0 |
| DECOMPILER TRAP | overlapping instructions at 0x004bc794 (a jump into the middle of an instruction); Ghidra's C is unreadable but equal to strstr |
| Portable equivalent | `strstr` |
| Port status | documented only (C library) |

### Watcom runtime, one row each

| Address | Routine | Portable equivalent |
|---|---|---|
| 0x004bc7da | thread data hook default: returns [0x00c85bc0] (the single thread data block made by 0x004caf36); in the hook table at 0x004e89c4 | none needed (Watcom runtime) |
| 0x004bc7df | a lone `ret`: the default for the other runtime hooks (heap / file lock and unlock, 0x004e89c8.. 0x004e8a04) | none needed |
| 0x004bc7e0 | jmp 0x004cac51: add an OS handle to the file handle table 0x00c8be48 (grows it), returns the fd | none needed (C stdio) |
| 0x004bc7e5 | jmp 0x004cad66: clear fd slot (0 < fd < count) | none needed |
| 0x004bc7ea | `__InitRtns` style startup: is DLL flag 0x00c85bbc, thread data 0x004caf36 (ExitProcess(1) if it fails in an EXE), file table init 0x004cad92, GetEnvironmentStrings -> 0x004e8e79, GetModuleFileNameA(NULL) -> strdup -> 0x004e8e4c, GetCommandLineA -> strdup -> 0x004e8e48 then the program name skipped (quoted or to the first blank, ctype table 0x004e633c bit 2), GetVersion -> 0x004e8e7f / 0x004e8e80 / 0x004e8e81 | none needed (C main) |
| 0x004bc911 | the C startup body (old name crt_subsystem_init, right): GetModuleHandleA(NULL), 0x004bc7ea, 0x004cab7f (exception registration chain from FS:[0]), stack limit FS:[8] into the thread data, crt_init(0xff) (initializers) | none needed |
| 0x004bc957 | `exit` (old name crt_exit, right): 0x004cabbd (unhook exception chain, flush), 0x004cb299(0, 0xff) finalizers, hook 0x004e8a00, ExitProcess(code) | `exit` |
| 0x004bf5c2 / 0x004bf5cc | `fcos` / `fsin` on st0, repeated after 0x004bf5d6 reduces the argument while C2 says it was out of range | `cos` / `sin` |
| 0x004bf5d6 | the reduction: `fprem` by 2 pi (tbyte 0x004e8e28) until C2 clear | none needed |
| 0x004bf619 | tan: `fptan` (or the emulation 0x004cb794 if 0x004e94bc bit 0), pops the 1.0 | `tan` |
| 0x004bf63a / 0x004bf64f | atan (fld1; fpatan) / atan2 (`fpatan` st1, st0), emulation 0x004cb848 under the same flag | `atan` / `atan2` |
| 0x004bf680 | `malloc` (Watcom near heap: size rounded to 4, minimum 0x0c, mini heap list 0x004e8e34 with rover 0x004e8e38 and the largest free size cache 0x004e8e3c, grows with 0x004cbb33, then the out of memory hook 0x004cbb8e; heap lock hooks 0x004e89e0 / 0x004e89e8); 0 for size 0 or > 0xffffffd4 | `malloc` |
| 0x004bf76f | `free` (finds the mini heap: last used 0x00c8b34c, else walks the list; 0x004cb924 frees; updates the largest free cache) | `free` |
| 0x004bf830 | `memset(eax, dl, ebx)` (old name crt_memset, right; the matcher's taskMgrUnlink is wrong): byte spread to a dword, 0x004cbba0 fills | `memset` |
| 0x004bf848 | C++ array construct without a count header (eax = memory, edx = count, ebx = descriptor: +1 constructor, +0x0d element size), with an exception frame (0x004cbc71); returns the memory, or 0 if it was 0 | none needed (C: calloc / loops) |

### 0x004bcb70 grid_cell_triangle (game code, not platform; matcher name hdpPush: wrong)

| | |
|---|---|
| Does | looks up a point in a 2D grid of cells, each split into two triangles, and copies the chosen triangle's four i16 (three values plus one, e.g. a plane) for one or two layers |
| Arguments | eax = object (+0x04 grid header), edx = point {x, z} (two i32), ebx = extra cell shift (signed char added to both cell shifts), ecx = out1 (4 x u16), stack: out2 (4 x u16); `ret 4` |
| Grid header | +0x04 u16 width, +0x06 u16 depth, +0x08 i16 x origin (read as `[+6] sar 16`), +0x0a i16 z origin, +0x0e u8 cell shift x, +0x0f u8 sub shift x, +0x10 u8 cell shift z, +0x11 u8 sub shift z, +0x12 bit 0 two layers; cells from +0x14, 0x14 bytes (0x28 with two layers) |
| Logic | outside [x0, x0+width] or [z0, z0+depth]: return 0. Cell = ((z-z0) >> sz) * (width >> sx) + ((x-x0) >> sx) with sx, sz the cell shifts + ebx. ux = (x-x0) >> sub x, uz = (z-z0) >> sub z; the cell's flag bit 1 mirrors ux (~ux); (uz & mask z) < (ux & mask x) picks the first triangle (u16 +0..+4 and +0x10, flag +0x06) else the second (+0x08..+0x0c and +0x12, flag +0x0e); masks = (1 << (cell shift + ebx - sub shift)) - 1. Second layer the same from +0x14 of the cell into out2. |
| Returns | 1, or 0 if the chosen first layer triangle's flag bit 0 is clear (out1 not written then) or the point is outside |
| Callers | 0x0048e25c, 0x0049a1b4, 0x0049d4ac |
| Port status | out of the platform scope: belongs to the world / collision code; not diffed here |

### Direct3D texture helpers (hardware 3D device path; documented only, not ported)

The device comes from 0x004b5f8c(display 0x00c84840) (the display object's Direct3D device, +0x28).
Texture records: 0x00a30c48 + 0x30 * index (+0x04 width, +0x08 height; +0x00, +0x0c, +0x10, +0x20
are read by 0x004bff08 and passed to 0x004c0488, not decoded here).

#### 0x004bf920 tex_format_enum_callback

| | |
|---|---|
| Does | `HRESULT CALLBACK (LPDDSURFACEDESC desc, ctx)` for EnumTextureFormats (stdcall, `ret 8`): copies desc+0x48 (DDPIXELFORMAT, 32 bytes); rejects DDPF_ALPHA (2), non DDPF_RGB (0x40), fewer than 8 bits, 8 bits without DDPF_PALETTEINDEXED8 / 4 (0x28); scores |bits - wanted bits| and the summed |popcount(mask) - wanted| of R, G, B, A; keeps the best (smallest bit difference, then smallest mask difference; with an alpha request, the smallest alpha difference) in ctx+0x14..+0x1c and the format in ctx+0x20 |
| Context | +0x00 wanted bits, +0x04..+0x10 wanted R, G, B, A bit counts, +0x14 best bit difference, +0x18 best mask difference, +0x1c best alpha difference (999999 = none), +0x20 chosen DDPIXELFORMAT |
| Returns | 1 DDENUMRET_OK; 0 DDENUMRET_CANCEL on the "exact match" path |
| ORIGINAL BUG / DECOMPILER TRAP | the exact match test compares ebp, edx, eax: registers never set in the callback (Ghidra: unaff_EBP, in_EDX, in_EAX), so it depends on what DirectX left in them |
| Portable equivalent | none (SDL renders textures in a fixed format) |

#### 0x004bfc88 tex_fit_size

| | |
|---|---|
| Does | w = round(tex.width / *u_scale), h = round(tex.height / *v_scale); if square textures are required (0x00c84f50) make w == h by doubling the smaller (its scale *= 0.5) when the other is over twice as big, else halving the bigger (its scale *= 2.0); defaults max 256 x 256 (0x00c84f54 / 58) and min 32 (0x00c84f5c / 60) when 0; while w > max w: w /= 2, u scale *= 2; same for h |
| Arguments | eax = texture index, edx = float* u scale, ebx = float* v scale; ret |
| Port status | documented only (pure, but only the hardware path uses it) |

#### 0x004bfe50 tex_choose_format

| | |
|---|---|
| Does | context = {bits eax, R edx, G ebx, B ecx, A stack, differences 999999}; `IDirect3DDevice2::EnumTextureFormats` (slot 0x24) with 0x004bf920; copies the chosen DDPIXELFORMAT to 0x00c8b350 |
| Arguments | eax, edx, ebx, ecx, stack 1; `ret 4` |
| Port status | documented only |

#### 0x004bfed0 tex_format_bits

| | |
|---|---|
| Does | returns [0x00c8b35c]: the chosen format's dwRGBBitCount |
| Port status | ported, emulator diff: src/platform/pf2_misc.c, tools/emu/check_pf2_misc.sh |

#### 0x004bff08 tex_create

| | |
|---|---|
| Does | creates a Direct3D texture for texture record `index`: device `GetRenderTarget` (slot 0x40), QueryInterface (IID at 0x004e5504, IDirectDrawSurface2) and `GetDDInterface` (surface slot 0x90) to reach the IDirectDraw, Release x3; 0x004bfc88 fits the size; destroys the old one (0x004c1024); DDSURFACEDESC (0x6c) flags 0x1007 (CAPS, HEIGHT, WIDTH, PIXELFORMAT) with the 0x00c8b350 format, caps 0x1800 (TEXTURE \| SYSTEMMEMORY); device GetCaps (slot 0x0c, two 0xcc byte D3DDEVICEDESC); IDirectDraw CreateSurface (slot 0x18): system memory copy; if the hardware description has flags, a second CreateSurface with caps 0x04005000 (TEXTURE \| VIDEOMEMORY \| ALLOCONLOAD), else the system surface is AddRef'd and used; 0x004c0488 fills the texels; 8 bit or less: a 256 entry palette from the 15 bit CLUT (bit 15 -> alpha 0xff), CreatePalette (slot 0x14, 0x44 = 8BIT \| ALLOW256), SetPalette (0x7c) on both surfaces; SetColorKey (0x74) on both (0xff for palettized, 0 otherwise, skipped when the format has alpha bits); QueryInterface on the surface (IDirect3DTexture2), GetHandle (slot 0x0c); with a video copy, Load (slot 0x14) from the system copy; Releases. (Slots from the machine code; the arguments of the last calls were read from Ghidra only.) On any failure 0x004c1024 destroys it. |
| Arguments | eax = out texture object (+0x04 alpha bits, +0x08, +0x0c surface, +0x10 palette, +0x14 video flag), edx = index, ebx = float* u scale, ecx = float* v scale; ret |
| Port status | documented only (hardware 3D device path) |

Not in the function list but inside this range (found while reading; no entry in platform.tsv):
0x004bb756 (stop every voice of a bank), 0x004bb804 (start a program's voices directly: the same
volume formula as 0x004bbaf8 with constants 0x004e3954 / 0x004e395c), 0x004bba10 (find an earlier
sample with the same wave index), the list update twin around 0x004bbf40..0x004bc2xx, 0x004bc390,
0x004bf62e (cdecl tan wrapper).


## Corrections and additions from the emulator ports (2026-11-24)

- 0x004b67d0 d3d_draw_prim: code 0xc0 draws a fixed 2 point LINESTRIP (vertices +8 and +0xc), not "a count
  from the record". POLY_F4 (0x28) is drawn as a fan in the order v0, v1, v3, v2 (+8, +0xc, +0x14, +0x10);
  POLY_FT4 (0x2c) keeps the stored order. Sprites: 0x74 maps the whole w x h (no uv bytes), 0x7c uses the
  uv bytes +0x10..+0x13; texture index at +0x10 (0x74) / +0x14 (0x7c), +0x20 (FT3), +0x28 (FT4). Unfiltered
  textures (info +0x28 != 0) get scale (1/size) * ((size - 1)/size) and offset 0.5/size (texel centres).
  Specular (and tu / tv of untextured primitives) are never written: stack garbage. Codes it does not
  draw return the caller's ecx.
- Texture tables: 0x00a2dc48 + 24 i (handle, color key count) and 0x00a30c4c + 48 i (width, height, +0xc
  color keyed, +0x28 no filtering) overlap for i >= 0x200; the original reads them as one block.
- 0x004b617c d3d_set_blend: the 5 entry jump table at 0x004b6168 = {0x4b61ee, 0x4b6227, 0x4b625f,
  0x4b6297, 0x4b61ee}: modes 0 and 4 identical (SRCALPHA / INVSRCALPHA), 1 = SRCALPHA / ONE, 2 = ZERO /
  INVSRCALPHA, 3 = nothing.
- Constants used by the sprite path: 0x004e36e9 = -1.0f, 0x004e36ed = 0.5f (unaligned).
- Unset register returns (ORIGINAL BUG, ports return 0 or take the register as an argument):
  detach_clipper / attach_clipper (esi), release_surfaces (edx test), voice_stop (edx), set_blend and
  draw_prim (ecx), draw_ot (edi).
- 0x004b8ad4 cd_get_mode: only dwItem of MCI_STATUS_PARMS is set; dwCallback / dwTrack are stack garbage.
- 0x004b276e crt_utoa: radix 0 faults, radix 1 never terminates (writes past its stack buffer).
- Unlisted functions seen on the way (not in the task list, not ported): 0x004b43cc (release two
  interface pointers), 0x004b43fc (IDirectDraw2 slot 0x4c), 0x004b48e8, 0x004b50ec, 0x004b50fc (get +0x6c),
  0x004b5b88 / 0x004b5bc4 / 0x004b5bdc (client rect helpers), 0x004b5f68 / 0x004b5f70 (3D flag setters),
  0x004b5f90 (D + 0x34), 0x004b62b4 (set SRC / DESTBLEND), 0x004b62e0 (set SHADEMODE cached), 0x004b8f8c
  (cd_get_volume), 0x004b8fcc, 0x004b9148 (jmp 0x004bb6c4), 0x004b9158 (sound_set_level 0..3), 0x004b9c1c
  (movie close), 0x004b3b90 / 0x004b3b9c (text style setters).


### Second pass (pf3_, 2026-11-24)

## Corrections / additions to docs/PLATFORM.md

- 0x004b3d80 ddraw_init: the IID copy to +0x34 is `repne movsd` (f2 a5; capstone prints it without the
  prefix): all 16 bytes are copied. The cooperative level call inside the first DirectDrawCreate try
  (0x004b3fde..0x004b3fff) is unreachable; the retry's copy is reachable (a failed QueryInterface can leave
  +0x08 set). The result taken after QueryInterface is IDirectDraw::Release's whenever +0x04 is set.
- 0x004bff08 tex_create: the IDirectDraw comes from device GetDirect3D -> QueryInterface(0x004e5504) ->
  slot 0x90, each object Released at once and the last one used after its Release. The 8 bit palette puts
  the PS1 red field in peBlue and blue in peRed. A video memory copy is made when the device caps'
  color model (hw desc +8) is set, then IDirect3DTexture2::Load from the system copy.
- 0x004bb1f0 sound_bank_load: magic checked only when the read returned 4 bytes; the sample array
  allocation is never checked (the tone pointer is tested twice; that second test is unreachable code);
  wave header / data pointers live in two 70 entry stack tables (more waves overflow them); a sample's
  wave id is not range checked; root note not range checked in the frequency table index.
- 0x004b8b40 cd_command: 9 entry jump table at 0x004b8b1c. Commands > 8 use the caller's esi as the
  error code. A retry answer (4) from the error box (only for MCIERR 0x123) sets a flag that is never
  cleared: the original then repeats the command forever, even after it succeeds. The poll (8) treats
  MCI_MODE_STOP 0x20d as "track ended" (callback or play again) and 0x211 PAUSE as resume. The pause
  command skips MCI_PAUSE when the mode is STOP. Only the MCI fields named in pf3_cd.c are set.
- 0x004bf920 tex_format_enum_callback: an early test compares the caller's stale ebp / edx / eax with
  the context and, on a match, takes the format and returns 0 (stops the enumeration).
- 0x004bfc88 tex_fit_size: constants 0x004e4920 (x2) / 0x004e491c (x0.5) are the scale multipliers;
  limits 0x00c84f54..0x00c84f60 default to 256 / 256 / 32 / 32. A side that chops to 0 under the square
  rule makes the original loop forever.
- 0x004b45a0 ddraw_set_mode: the cursor is shown (1) when 0x0049e108 returns 0 and hidden otherwise.
- 0x004b4c80 ddraw_enter_fullscreen: with no IDirectDraw2 the original calls SetDisplayMode on NULL.
- Not done this pass (left as is): sfx_update 0x004bc4e4 (its code is split in chunks before the entry,
  jumps back to 0x004bbfbd / 0x004bc081 / 0x004bc0aa), sound_listener_locate 0x004b8010, and the rest of
  the r4b list.

### Third pass (pf4_, 2026-11-24)

## Replacement line for r4b2_verified.tsv (cd_command; I was told not to edit r4b2_*)

004b8b40	cd_command	[VERIFIED 2026-11-24 emu diff, except the retry path] ... (as now) ... THE PORT DELIBERATELY DIFFERS on the retry path: in the original an error box answer of 4 (retry, error 0x123 only) sets a flag that is never cleared, so the original repeats the command forever; the port repeats the command only while the box keeps answering retry. That path (0x004b8e75 / 0x004b8e7a) is not compared. See src/platform/pf3_cd.c

## Files

New files only (pf4_ prefix); diff_pf2_env.py / diff_pf3_common.py imported, not edited.
- src/platform/pf4_listener.c (0x004b7fa4, 0x004b8010), pf4_present.c (0x004b52b8), pf4_fog.c (0x004b4e98),
  pf4_text.c (0x004b31d0, 0x004b3330, 0x004b3a44, 0x004b3a90), pf4_movie.c (0x004ba15c), pf4_dsinit.c
  (0x004b94f4), pf4_misc.c (0x004bae50, 0x004b84a0, 0x004b8694, 0x004b90d0, 0x004b270c, 0x004b27e6),
  pf4_cd.c (0x004b8eac, 0x004b858c, 0x004b86dc), pf4_d3d.c (0x004b5bf0, 0x004b5ce8, 0x004b5ee8, 0x004b5e00,
  0x004b41a0, 0x004b42c8). pf4_fog.c and pf4_d3d.c include src/render/gpu3d_float.h and need -ffp-contract=off.
- tools/emu/check_pf4_{listener,present,fog,text,movie,dsinit,misc,cd,d3d}.sh + diff_pf4_*.py.
- Lines: tools/names/verified.tsv (25 functions, 2272 instructions).

## Additions / corrections to docs/PLATFORM.md

- 0x004b8010 sound_listener_locate: in the split screen case (view -1) the return value is view 1's
  distance, or view 1's lateral when view 1 is the nearer (eax reused for the copy to 0x00c85058).
  The distance is an inline bitwise integer square root with signed compares.
- 0x004b52b8 ddraw_present_rect: two 5 entry jump tables, 0x004b52a4 (full present with page flipping)
  and 0x004b5290 (otherwise); matches the doc's table.
- 0x004b4e98 d3d_clear_fog: c = -0.1 (double 0x004e369d): the quad is (W - 0.1) x (H - 0.1); sz 10000.
- 0x004ba15c movie_play: ret 8 (two stack arguments: movie_open's extra argument and the flags);
  0x004ba322..0x004ba32a is unreachable (the selector is always 1, so 0x004e3890 is always sent).
- 0x004b94f4 dsound_init: SetFormat 11025 Hz 16 bit stereo, primary buffer flags 0xc1, Play(LOOPING);
  the saved primary volume is mB / 100 + 100.
- 0x004b8eac cd_find_aux_device returns 1 (not -1) when no aux device is found; callers test < 0.
- 0x004b86dc cd_play takes the MCI_STATUS track count even when that call failed.
- 0x004bae50 group_alloc_voice: a voice with no samples inherits the previous voice's playing status
  (uninitialised for voice 0).
- 0x004b41a0 ddraw_release returns 0 on the full release path when the IDirectDraw2 field is already 0
  (eax reloaded from the field), else the last call's result.
- 0x004b42c8 ddraw_restore_and_clip returns the caller's edi after re-setting the mode in 3D mode.
- Not done: dsound_device_dtor 0x004b93cc and dsound_close 0x004b9674 (log10 through 0x004c9f14: the x87
  transcendental question), cd_monitor_thread 0x004b902c (never returns), movie_set_window 0x004b9c84,
  movie_open 0x004b9de0, sample_create 0x004ba4ec, sample_read_header 0x004ba95c, voice_read 0x004bac7c,
  d3d_create_materials 0x004b6320, d3d_set_background_color 0x004b6428, ddraw_measure_refresh 0x004b598c.

### Upper range (pf5_, 2026-11-24)

## Corrections / additions to docs/PLATFORM.md

- sfx_update 0x004bc4e4: pass 1 (stop requests: Stop + rewind 0x004b9898 then release
  0x004ba67c per voice; finished sounds: release only), pass 2 (budget: the slot that takes the sum
  over +0xa94 is dropped and the walk goes on while the sum is below the budget; a failed
  0x004bbaf8 drops the slot and gives its voices back to the sum), pass 3 (the rest dropped).
  ORIGINAL BUG: an instance with no voices is judged by the stack word the last status read left
  (or what was on the stack before): the port takes the initial word as an argument.
- sfx_slot_find 0x004bbdcc: insertion point with < 50 in use = first slot with +0x24 <= 0 and
  +0x20 < 0 (note: a different test from the eviction's "negative": +0x24 < 0, or +0x24 == 0 and
  +0x20 < 0). With 50 in use and a non negative last slot the new slot is attempted before the last
  slot and fails (the list is full): returns 0. The "no insertion point" return 0x004bbf38 is
  dead. A program with no tones reads address 0x10..0x18 (tested with page 0 mapped).
- Unlisted helpers next to the list, not ported: 0x004bba10 (voice search in an instance),
  0x004bba30 (slot clear), 0x004bba64 (slot init from a program), 0x004bbab0 (stop an instance's
  voices, instance = 0), 0x004bc2b8 (index of (program, key) or -1), 0x004bc2ec (order position
  search), 0x004bc390 (stop and unlink one slot by index), 0x004bc42c (release and unlink all).
- grid_cell_triangle 0x004bcb70: the second layer is still looked up after a first layer miss
  (the result is 0 then, out1 untouched); shift counts are taken mod 32 when the extra shift makes
  them negative.
- strstr 0x004bc710 and malloc 0x004bf680: they push / load / pop segment registers (es, fs, gs):
  no-ops on Win32; Unicorn has no descriptor table, so the harness carries those instructions out.
  `mov edx, ds` gives 0 under the emulator; the selector is passed on to 0x004cb87c / 0x004cb924
  and is meaningless in a flat port (the port takes it as an argument).
- malloc: the request size (not the rounded one) is passed to 0x004cb87c; the rounded size only
  decides whether the walk starts at the rover. Grow 0x004cbb33 is tried once per out of memory
  round; a non zero out of memory hook 0x004cbb8e restarts the walk (and allows grow again).
- crt_init_rtns 0x004bc7ea: the environment pointer goes to the unaligned 0x004e8e79; the program
  name skip uses the ctype table indexed (u8)(c + 1) bit 2 (blank).
- crt_subsystem_init 0x004bc911: the stack limit comes from fs:[8] (TIB); the port reads it as
  memory at PF5_TIB + 8.
- Not done: the x87 helpers 0x004bf5c2 / 0x004bf5cc / 0x004bf5d6 / 0x004bf619 / 0x004bf63a /
  0x004bf64f (fsin / fcos / fprem / fptan / fpatan: transcendental x87 results, the open question
  3 of the night log).

### Last pass below 0x004bb000 (pf6_, 2026-11-24)

## Additions / corrections to docs/PLATFORM.md

- MCI strings (read from TM2.EXE): 0x004e3797 "where mov source", 0x004e37a8 "put mov window client at
  %i %i %i %i", 0x004e377e "close mov", 0x004e37cd "open %s alias mov style child parent %d",
  0x004e37f5 "window mov state show", 0x004e380b "status mov window handle", 0x004e3824 "setaudio mov
  volume to %i", 0x004e383e "play mov window notify", 0x004e3874 "pause mov", 0x004e3890 "seek mov to
  end", 0x004e36f4 "%c:", 0x004e36f8 "cdaudio".
  So 0x00c8510c is the movie window handle (atoi of the status reply), and the "setting command" in
  my pf4_movie.c comments / r4b3 line for 0x004ba15c is "setaudio mov volume to (0x004e86cc * 10)";
  in that function 0x004e3874 is "pause mov" and 0x004e3890 "seek mov to end" (the lead may want to fix
  those words when merging pf4; the code and its check are unaffected).
- 0x004b6320 / 0x004b6428: IDirect3DMaterial2 GetHandle is vtable slot 0x14 (SetMaterial 0x0c).
- 0x004ba4ec sample_create: both locked parts are filled from the start of the wave data (the second
  should continue after the first): ORIGINAL BUG, only matters when the lock wraps.
- 0x004b9c84 movie_set_window: the reply parse has no end of string tests.
- 0x004b902c cd_monitor_thread: never returns; the port's loop ends on a backend stop value from Sleep
  (a seam documented in pf6_cdmon.c). Its callback gets eax = the mode (0x20d), edx = 0.
- Still not done below 0x004bb000: dsound_device_dtor 0x004b93cc, dsound_close 0x004b9674 (log10,
  user decision pending).
