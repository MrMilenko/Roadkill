# Quality of life switches

Roadkill plays like the 1997 PC game by default. Everything here is a setting in `roadkill.ini`;
the switches that change the game are off (or at the original's value) unless noted.

## The settings file

`roadkill.ini` lives in the per user data folder, written with every key, its default and a
comment the first time Roadkill runs:

| | |
|---|---|
| macOS | `~/Library/Application Support/Roadkill/` |
| Linux | `$XDG_DATA_HOME/roadkill/`, else `~/.local/share/roadkill/` |
| Windows | `%APPDATA%\Roadkill\` |

The game's own saved settings (what the original kept in the Windows registry) are
`registry.txt` in the same folder; a `~/.config/opentm2/registry.txt` from earlier Roadkill
builds is copied there once. `TM2_SETTINGS=<file>` reads another settings file.

Every key has an environment variable that overrides the file (the `TM2_*` variables Roadkill
always had keep working). Order: environment, file, built in default. Host code reads them
through `src/host/settings/settings.h` (`settings_int` / `settings_str` / `settings_bool`, key
`"section.name"`); the switches below through `src/host/qol/qol.h` (one file per switch in `src/host/qol/`).

| Key | Default | Variable | |
|---|---|---|---|
| `[display] fullscreen` | 0 | `TM2_FULLSCREEN` | full screen when the game asks for its full screen mode |
| `[display] scale` | 2 | `TM2_SCALE` | whole multiple of 640 x 480 for the window |
| `[display] window_width`, `window_height` | 0 | `TM2_WINDOW_WIDTH`, `_HEIGHT` | a window size in pixels instead (0: the scale) |
| `[display] vsync` | 1 | `TM2_VSYNC` | |
| `[display] widescreen` | 0 | `TM2_WIDESCREEN` | 1 = 16:9 or an aspect (16:10, 21:9): matches show more of the arena left and right, the vertical view unchanged (the horizontal half angle widened in level_setup_viewport); the HUD keeps to the screen edges as the game places it; menus stay 640x480, pillarboxed |
| `[display] internal_scale` | 1 | `TM2_INTERNAL_SCALE` | 1..4: the software renderer draws at S times 640x480 (a second build of src/render with a larger tile grid, src/engine/render/gpu_hi.h); 3D edges sharper, textures at their original resolution, 2D scaled nearest. With both switches off the original renderer's machine code is unchanged |
| `[display] skip_movies` | 0 | `TM2_SKIP_MOVIES` | 1 = no movies (the SingleTrac logo, the intro, the endings): the MCI avivideo device reports no driver, as before the movies were added, and the game goes on at once (src/host/movies/mci_avi.c). 0: they play, as the original |
| `[audio] sound` | 1 | `TM2_NOSOUND=1` is 0 | no effects, no music |
| `[audio] master_volume`, `effects_volume`, `music_volume` | 100 | `TM2_VOLUME`, `TM2_SFX_VOLUME`, `TM2_MUSIC_VOLUME` | 0..100, on top of the game's own volumes |
| `[paths] data` | `disc/files` | `TM2_DATA` | the game files (TM2.EXE is loaded from here) |
| `[paths] cd_image` | the first .cue next to the data folder | `TM2_CDIMAGE` | the disc image for the music |
| `[paths] registry` | registry.txt in the user folder | `TM2_REGISTRY` | |
| `[input] controller` | 1 | `TM2_NOJOY=1` is 0 | see 2 below |
| `[gameplay] loop_music` | 0 | `TM2_LOOP_MUSIC` | see 3 below |
| `[gameplay] fix_crashes` | 0 | `TM2_FIX_CRASHES` | see 4 below |
| `[debug] ...` | | `TM2_KEYS`, `TM2_SHOT_EVERY`, `TM2_KEEP_ACTIVE`, `TM2_CD_LOG`, ... | written commented out; `src/host/settings/settings.c` lists them all |

## Status

| Switch | Status |
|---|---|
| 1. Settings file instead of environment variables | done: every `TM2_*` the host read goes through `settings.h` |
| 2. Controller works by default (`[input] controller`, default **on**) | done |
| 3. Looping CD music (`[gameplay] loop_music`, default off) | done |
| 4. Fixes for the original's crash bugs (`[gameplay] fix_crashes`, default off) | done for the ones that can happen, see the list |
| Widescreen, higher internal resolution | the renderer's work; keys defined here |
| The setup dialogs drawn in game | the dialogs' work |

### 2. Controller by default

The game only reads a pad when its saved `InputJoystickStatus` is 1, which on Windows its input
screen (or the installer) writes; Roadkill's first run seeds the installer's 2 (off). With
`[input] controller = 1` and a game controller connected when the game starts, the registry
host answers 1 for that value (src/host/win32_reg.c, `RegQueryValueExA`), so the game enables
its joystick at start up. The file keeps what the game wrote; the keyboard keeps working. The
game still compares its saved joystick record with the attached pad and switches the pad off
when a different one is attached (the original's behaviour; a visit to the input screen fixes
it).

Seen: the same scripted run (keys start a match, a virtual pad holds the stick forward and A):
before, the game never polls the pad and the car sits at the start; with the switch the pad is
polled every frame and the car drives off.

### 3. Looping CD music

What the original does: its CD monitor thread (0x004b902c, verified.tsv `cd_monitor_thread`)
polls the MCI mode every 3 s and, when a track has stopped and no end callback is set, plays it
again (`cd_command(1)`), so a level's track repeats after up to 3 s of silence; Roadkill runs
that thread as the original did. With `loop_music = 1` the CD player (src/host/cd_audio.c)
starts the play over at once when it reaches its end, with no gap, and the MCI mode stays
"playing". Test: a 2 s piece of track 2 through the mixer; off: stopped after 2 s; on: still
playing, back at the start.

### 4. Fixes for the original's crash bugs

How the port differs from the original whatever the switch: the game's address space is one
host buffer and every game address is masked into it (`rt_ptr`, src/rt/rt.h), so the
original's null reads and reads past tables (verified.tsv: sin / cos 0x0048b624 / 0x0048b690,
fxDebrisDraw 0x00489d20, sfx_slot_find 0x004bbdcc, score_overlay_draw 0x00437134,
unlisted_004acb70, movie_set_window 0x004b9c84, ...) read a wrong value but cannot crash
Roadkill. Divisions by zero the original would fault on are already defined in the ports
(anim_step_random 0x004a0b08 / anim_step_weighted 0x004a0b7c answer 0), as is cd_command
0x004b8b40's endless retry (the port retries only while the box answers Retry). The other
ORIGINAL BUG notes are wrong values, uninitialized stack words or network only paths (no
DirectPlay yet).

Guarded by `fix_crashes` (each logs `roadkill: fix_crashes: ...` once when it changes
something):

| Where | Original behaviour (OpenTM2 tools/names/verified.tsv) | Fix |
|---|---|---|
| textures_load 0x00497390, src/engine/en9_texload.c | line 1582: "the creation result is kept across passes and checked after every record, so the pass after an out of memory one stops at its first record without a page and the next budget is negative: the scaling loop never ends (also with fixed size pages over budget)" | each pass starts with a clean result; the scaling loop stops after a whole round without change. Only reached with Direct3D textures, which the port does not offer yet (software renderer), so it cannot happen today |
| clutAnimNewDef 0x00499ee8, src/engine/en_clut.c | line 597: "the last index keeps growing on a full table, later searches run past it" (and what they find is written) | the index stays on the table's last entry. Needs more than 25 palette animations in a level; not seen in play |

Fixed regardless of the switch (the port's own stubs caused them):

| Where | What happened | Fix |
|---|---|---|
| obj_reset_a 0x004a2e60 / obj_reset_b 0x004a3620 bindings, src/rt/bind_engine_widget.c | the "pure virtual function called!" fatal exit on the network (multi player), audio options and input config screens: the original returns with EAX = the object and its callers write the derived vtable through it (static init 0x00477640: `[eax + 0x30] = 0x004e50d8`); the bindings answered 0, so the vtable went to address 0x30 and the background widget kept the base vtable 0x004e58fc, whose slot +0x20 is the pure virtual stub 0x004a6a39 (called from render_set_background 0x004a2ea0). Not DirectPlay, and not in the original | the bindings return the object. The network screen now says it found no network providers (the DirectPlay host enumerates none) and goes back to the title, as the original does without providers. The sighting during a demo did not happen again in about 17 attract demo cycles; most likely the same bug through one of these screens |
