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
| `[net] port` | 27420 | `TM2_NET_PORT` | network play's UDP port (the next free one, up to +9, when taken); LAN sessions are looked for on it and the next 3 |
| `[net] join` | empty | `TM2_NET_JOIN` | `host:port` asked directly for its session (as well as the LAN) |
| `[net] lan` | 1 | `TM2_NET_LAN` | look for sessions on the LAN (a broadcast while the join screen is open) |
| `[net] announce` | `roadkill.horrible.dev:27400` | `TM2_NET_ANNOUNCE` | the announce server internet sessions are listed and introduced through (empty: none) |
| `[net] public` | 1 | `TM2_NET_PUBLIC` | 1: sessions this machine hosts are announced there |
| `[net] timeout` | 10 | `TM2_NET_TIMEOUT` | seconds without a word from a machine before it leaves the session (the game has no timeout of its own) |
| `[gameplay] loop_music` | 0 | `TM2_LOOP_MUSIC` | see 3 below |
| `[gameplay] fix_crashes` | 0 | `TM2_FIX_CRASHES` | see 4 below |
| `[gameplay] net_drop_stale` | 0 | `TM2_NET_DROP_STALE` | see 5 below |
| `[gameplay] unlock_hidden_cars` | 0 | `TM2_UNLOCK_HIDDEN_CARS` | see 6 below |
| `[gameplay] play_dark_tooth` | 0 | `TM2_PLAY_DARK_TOOTH` | see 7 below |
| `[gameplay] hidden_arenas` | 0 | `TM2_HIDDEN_ARENAS` | see 8 below |
| `[debug] ...` | | `TM2_KEYS`, `TM2_SHOT_EVERY`, `TM2_KEEP_ACTIVE`, `TM2_CD_LOG`, ... | written commented out; `src/host/settings/settings.c` lists them all |

## Status

| Switch | Status |
|---|---|
| 1. Settings file instead of environment variables | done: every `TM2_*` the host read goes through `settings.h` |
| 2. Controller works by default (`[input] controller`, default **on**) | done |
| 3. Looping CD music (`[gameplay] loop_music`, default off) | done |
| 4. Fixes for the original's crash bugs (`[gameplay] fix_crashes`, default off) | done for the ones that can happen, see the list |
| 5. Late car state packets dropped in network play (`[gameplay] net_drop_stale`, default off) | done |
| 6. Sweet Tooth and Minion without the cheat codes (`[gameplay] unlock_hidden_cars`, default off) | done |
| 7. Dark Tooth as the player's car (`[gameplay] play_dark_tooth`, default off) | done: his own slot on the car select, one player and network games |
| 8. The hidden arenas without the debug codes (`[gameplay] hidden_arenas`, default off) | done: network games (the original offers them nowhere else) |
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
ORIGINAL BUG notes are wrong values, uninitialized stack words or network only paths.

Guarded by `fix_crashes` (each logs `roadkill: fix_crashes: ...` once when it changes
something):

| Where | Original behaviour (OpenTM2 tools/names/verified.tsv) | Fix |
|---|---|---|
| textures_load 0x00497390, src/engine/en9_texload.c | line 1582: "the creation result is kept across passes and checked after every record, so the pass after an out of memory one stops at its first record without a page and the next budget is negative: the scaling loop never ends (also with fixed size pages over budget)" | each pass starts with a clean result; the scaling loop stops after a whole round without change. Only reached with Direct3D textures, which the port does not offer yet (software renderer), so it cannot happen today |
| clutAnimNewDef 0x00499ee8, src/engine/en_clut.c | line 597: "the last index keeps growing on a full table, later searches run past it" (and what they find is written) | the index stays on the table's last entry. Needs more than 25 palette animations in a level; not seen in play |

Fixed regardless of the switch (the port's own stubs caused them):

| Where | What happened | Fix |
|---|---|---|
| src/host/win32_reg.c, RegSetValueExA | the game saves its strings with lstrlenA as the size (en2_reg_write_string 0x004ac638) and reads them back into unterminated buffers; the registry kept them without the NUL, so the network screens never found their saved provider, names and session name again | REG_SZ values are kept with their NUL |
| src/host/dialogs/controls.c, list views | a background or text color of 0 (black) was taken as unset: the lobby's player and chat lists (white on black) were drawn white on white, only the selected row readable | the colors the game sets are used, black included |
| obj_reset_a 0x004a2e60 / obj_reset_b 0x004a3620 bindings, src/rt/bind_engine_widget.c | the "pure virtual function called!" fatal exit on the network (multi player), audio options and input config screens: the original returns with EAX = the object and its callers write the derived vtable through it (static init 0x00477640: `[eax + 0x30] = 0x004e50d8`); the bindings answered 0, so the vtable went to address 0x30 and the background widget kept the base vtable 0x004e58fc, whose slot +0x20 is the pure virtual stub 0x004a6a39 (called from render_set_background 0x004a2ea0). Not DirectPlay, and not in the original | the bindings return the object. The network screen now says it found no network providers (the DirectPlay host enumerates none) and goes back to the title, as the original does without providers. The sighting during a demo did not happen again in about 17 attract demo cycles; most likely the same bug through one of these screens |

### 5. Late car state packets in network play

In a match every machine sends its car's state once a frame without a guarantee (type 0x22,
DirectPlay Send flags 0, src/match/mt2_world.c); a receiver snaps the car to whatever arrives
last, so a packet that arrives after a newer one moves the car back (docs/NETWORK.md section 4).
This is how DirectPlay delivered them and the default. With `net_drop_stale = 1` the provider
drops a 0x22 packet whose own counter (packet +8) is not newer than the last one delivered from
that player (src/host/net/net_session.c). Its events (fire, pickups, kill credit) go with it, as
with a lost packet. All machines of a session must have the same value: it is part of the
session's compatibility word (a different one does not list or join).

### 6. Sweet Tooth and Minion without the cheat codes

What the original does: Sweet Tooth (car 12) and Minion (car 13) are on the car select only
after their codes are typed there: GLORIOUS (cheats on, code 0), then ICECREAM (code 18,
Sweet Tooth) or BIGBOY (code 19, Minion). The key sequence table is TM2.EXE 0x004e6a84; the
cheat handler 0x0044e1cc (src/platform/pf8_cheats.c) sets pickup counter 18 / 19
(0x004fff24 / 0x004fff28); the car select's menu hook 0x00477aa0 takes the counters (its buttons
0x12, 0x13 come from TM2.EXE 0x004e8064) and posts scene events 0x12 / 0x13, whose actions store
1 in 0x009f6c1c (Sweet Tooth) / 0x009f6c1d (Minion) (src/shell/sh3_car_select.c, frame +0x974 /
+0x95c). The wheel's filter 0x0047770c skips the cars whose flag is 0. Nothing clears the flags
again: once typed, the cars stay until the game exits (they are not saved). With
`unlock_hidden_cars = 1` the car select binding (src/rt/bind_shell.c, src/rt/qol_cars.c) stores
the same two 1s each time the car select starts, so both cars are on the wheel, also in network
games. Seen: the wheel goes Hammerhead -> Minion -> Sweet Tooth with the switch, Hammerhead ->
Shadow without.

### 7. Dark Tooth as the player's car

Dark Tooth is car 14 (type 0xF0): CARSDB\BB.DPC / .TPC, his own stats in the stats setup
0x0043846c, his name in the car name list, model 0xF0 in the session setup 0x00475af8; the
specials and the driving code take car type 0xF0. In the original he is only the boss. What the
data lacks is his place on the car select (the CARSEL scene's three 14 way switch nodes: 9005
the car models, 9006 / 9007 the two halves of the driver picture), a car info page (no
BIOS\BBINFO.TIM; the 14 entry TIM table 0x004e8024), an ending (the table 0x004e81d0 ends at
car 13), and a size that fits: he is four times a car (collision box 485 x 173, 116 high; cars
89..165 x 50..116, Minion 252), so as the player the chase camera sits inside him and his probe
step height (0xA0) lets him climb the arena walls.

With `play_dark_tooth = 1` (src/rt/qol_cars.c, called from the bindings in
src/rt/bind_shell.c, bind_formats.c, bind_car.c, bind_game.c, bind_match_mt2_save.c; the
verified ports are not changed):

- the car select's wheel has a 15th slot after Minion: when the CARSEL scene's files are loaded
  (level_files_load 0x004740a8), its three switch nodes are copied into 15 way ones and the scene
  objects pointed at the copies. Slot 14 of the models is his own BB.DPC (its BB.TPC pages
  appended to the scene's texture pages, the model relocated against them and run through
  dbscanHierarchy 0x004a15d4 with an empty object callback), drawn at a quarter; slot 14 of the
  driver picture is Sweet Tooth's. The name under it is the game's own "Dark Tooth"; the wheel's
  filter 0x0047770c lets car 14 through. Sweet Tooth and Minion keep their slots (locked as in the
  original until their codes, or `unlock_hidden_cars`);
- his car info page is Sweet Tooth's (bg_tim_load 0x0046f784 is handed Sweet Tooth's name for
  the word after the table); the car the screen comes back to after the page (0x004e8021, kept
  as (car + 13) % 14 for a 14 way wheel) is stored for 15 ways;
- a human's Dark Tooth is a quarter of the boss's size: his model is drawn through a DPC scale
  node (type 12, shift + 2) put between each root group of his BB.DPC and its children when the
  level loads (dpc_load_relocate 0x004502b0, the first BB.DPC of each level_load_assets
  0x004753a4: the humans' cars are loaded before the opponents'), the size values of his stats
  (+0x3E, +0x42, +0x44, +0x46, +0x48, +0x4A, +0x4E, +0x50 and the launch offsets +0xD4..+0xDC)
  are quartered after the stats setup, and his probe step height (car +0x594) is kept at 0x28. A
  quarter because a scale node only halves: half would still be twice a car. The chase camera
  then frames him as it does any car;
- per car things he has none of are Sweet Tooth's: the car info page, the ending if he wins the
  tournament (screenEnding 0x00485290), and a save made as him (Slot<n> +0x80 = 14) loads as
  Sweet Tooth while the switch is off. Saves and the tournament's progress otherwise keep car 14
  (they store the car number only);
- the Dark Tooth stage (tournament level 8) has no boss fight for a human Dark Tooth, as the
  original skips the New York Minion fight for a human Minion: cars find their object slot by
  car type (carBindHumanSlot 0x0045803c, car_bind_ai_by_type 0x00458340), so a boss of the
  human's own type shares the human's object; tried, that fight ends at once. The boss fight is
  marked done (0x00624ec9) when the level loads, and the stage goes on to its last messages and
  the ending;
- network games: the car select reserves car numbers with the host (net_join 0x004637c4; the
  per car tables 0x0050f580 / 0x0050f5bc have room for car 14) and needs nothing more. Every
  machine shrinks a human Dark Tooth the same way, and the switch is bit 2 of the session's
  compatibility word (docs/NETWORK.md), so machines with and without it do not see each other.

Seen: the wheel with his slot and model, Sweet Tooth still on the wheel; a tournament as him
from Los Angeles to Moscow; a save at the Dark Tooth stage through the stage's end, the ending and
the credits; a network game, host as Dark Tooth, the joiner seeing and targeting him. Not tried:
his special weapon, two player split screen.

### 8. The hidden arenas without the debug codes

TM2.EXE has three arenas its menus do not list: level 10, "Florida : Suicide Swamp" (a Jet Moto
crossover: TERRAIN\SWAMP.RMP / .GRP / .PTS, LEVELDB\SWAMP.DPC / .TPC), level 11, "Los Angeles :
Cyburbia" and level 12, "Los Angeles : Roof Tops" (CYB and SROOF, Twisted Metal 1's suburb and
rooftop). arenaSetup 0x004576a8 has their fog and loaders (src/combat/cb2_arena_setup.c); level 9
is the Dark Tooth stage (DEN).

The original offers them only on the network host screen ("Session Setup"), and only after debug
codes. Its arena arrows (0x004685d4 / 0x00468650, src/match/mt_shell.c) step 1..12, skip 9, and
take 10 / 11 / 12 only when pickupCounterTake 0x0044eee4(0x23 / 0x24 / 0x25) is non zero; the
screen's build (0x00467904, src/match/mt2_ui_net.c) also wants 0x1f before it keeps a remembered
arena above 9. Those are entries of the cheat table 0x004ffedc (4 bytes each) that the cheat
handler 0x0044e1cc (src/platform/pf8_cheats.c; key sequences at TM2.EXE 0x004e6a84) sets:

| Code | Entry | Global | What it does |
|---|---|---|---|
| DASBAK (31) | 0x1f | 0x004fff58 | debug mode on, only while 0x004632b0 says a DirectPlay session object exists (the network screens onwards); typed again: off |
| SLEUTH (32) | 0x20 | 0x004fff5c | spectator camera: two input actions (0x47 / 0x48) step the view through the other cars (src/game/car_input.c, `spectate`); with GLORIOUS or debug mode |
| HIDING (33) | 0x21 | 0x004fff60 | toggled (debug mode); no reader found |
| BLOWOUT (34) | 0x22 | 0x004fff64 | sets or clears 0x23, 0x24 and 0x25 together (debug mode); 0x22 itself always reads 0 |
| JMSWAMP (35) | 0x23 | 0x004fff68 | level 10, Suicide Swamp, on the host screen (debug mode) |
| TMBURBS (36) | 0x24 | 0x004fff6c | level 11, Cyburbia (debug mode) |
| TMROOF (37) | 0x25 | 0x004fff70 | level 12, Roof Tops (debug mode) |
| DEREK5 (38) | 0x26 | 0x004fff74 | toggled (debug mode); no reader found |

(33 and 35..37 also need 0x004e6be4, which is 1 in the retail game.) The level select's secret
level path (result 4: level = button + 10, "secret level selected") is wired to 0x23..0x25 only
for network game types 5..7 (bytes 0x004e805c; 0x004e8060 = 0xff 0xff 0xff otherwise), and no
screen of the network flow opens the level select; the one player level select's wheel
(SHELLDB1\LEVELSEL, switch node 9001) has 8 arenas. So in the original the route is: Multi
Player, host a game, type DASBAK then JMSWAMP (TMBURBS, TMROOF) on the Session Setup screen, and
the arrows reach the arena. Joining machines need no code: the arena travels in the session
record.

The pictures the screen shows are SHELLDB3\SC-<arena>.PCX; the disc has SC_<arena>.PCX (ISO 9660
names have no '-'), so the original never shows any of them. Roadkill's file lookup takes '-' and
'_' as one when the exact name is missing (src/host/win32_kernel.c), and its InvalidateRect sends
the main window the WM_PAINT that puts the picture on the screen (src/host/win32_user.c); both
always on, as they only change what failed.

With `hidden_arenas = 1` (src/rt/qol_arenas.c, called from src/rt/bind_combat_level.c and
src/rt/bind_match_mt2_ui_net.c) pickupCounterTake answers 1 for 0x23..0x25, and for 0x1f inside
the host screen's build, without writing the table: the arrows reach the three arenas with no
code typed, the codes still toggle what they toggle, and debug mode is not on for anything else.
Single player has no way to pick them in the original and the switch adds none. Network games
need nothing more: only the host picks the arena, so the switch is not part of the compatibility
word.

Seen: with the codes typed (switch off) and with the switch on, the host screen steps Hong Kong
-> Suicide Swamp -> Cyburbia -> Roof Tops -> Los Angeles; two machine endurance games in Suicide
Swamp (codes typed), Cyburbia and Roof Tops (switch on the host only), both cars driving. Not
tried: challenge / tournament game types there, a whole match to its end.
