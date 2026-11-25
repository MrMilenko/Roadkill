# Roadkill

**Twisted Metal 2, in portable code. Updated for modern PCs and ports.**

Roadkill has one purpose: the 1997 PC game, Twisted Metal 2 for Windows 95, native on modern
machines. Roadkill is built from the
[TM2-Win9x Decomp](https://github.com/MrMilenko/TM2-Win9x-Decomp): the original `TM2.EXE`
decompiled into C, every function the game reaches checked against the original machine code.

**Twisted Metal 2 PC behavior, by default.** Roadkill plays like the 1997 PC game. Everything
else (widescreen, higher resolution, fixes for the original's own bugs, looping music, ...) is a
toggle in `roadkill.ini`, off unless you switch it on.

**No game data is included.** You need your own copy of the Twisted Metal 2 PC disc. Roadkill
reads everything from your files at run time.

## Build

macOS or Linux, with CMake and SDL2 (`brew install cmake sdl2`, or your distribution's `cmake` and
SDL2 development package):

```sh
./scripts/build.sh
```

Windows: not yet (see "Where it stands").

## Play

Copy the contents of your PC disc into `disc/files/` (the `TM2.EXE`, `SINTAB.DAT` and the game
folders), and for the music put the disc image's `.cue` / `.bin` in `disc/`. Then:

```sh
./build/roadkill
```

Keyboard as in the original (arrows to drive, Enter to select, Esc back); a connected game
controller works as the game's joystick.

Settings are in `roadkill.ini`, written with commented defaults on the first run in your user
data folder (macOS `~/Library/Application Support/Roadkill/`, Linux `~/.local/share/roadkill/`,
Windows `%APPDATA%\Roadkill\`). The settings the game saves itself (the original kept them in
the Windows registry) go to `registry.txt` next to it. Environment variables override the file:

| `roadkill.ini` | variable | |
|---|---|---|
| `[paths] data` | `TM2_DATA` | where the game files are (default `disc/files`) |
| `[paths] cd_image` | `TM2_CDIMAGE` | the disc image's `.cue` for the music |
| `[display] fullscreen`, `scale` | `TM2_FULLSCREEN=1`, `TM2_SCALE=<n>` | full screen; window scale (default 2) |
| `[audio] sound`, `*_volume` | `TM2_NOSOUND=1`, `TM2_VOLUME=<0..100>` | no sound; volumes |
| `[input] controller` | `TM2_NOJOY=1` | controllers (default on) |
| `[gameplay] loop_music`, `fix_crashes` | `TM2_LOOP_MUSIC=1`, `TM2_FIX_CRASHES=1` | changes to the original, off by default |

`docs/QOL.md` describes every switch.

## Where it stands

From the title screen through the menus into matches, with sound, CD music and controllers, on
macOS. Known gaps: there is no network
play and no movies, and Windows and Linux builds have not been tried. `docs/RUNTIME.md` lists
the details.

## How it works

`src/<area>` is the decompiled game code. Today it still runs the way the decomp checks it: in a
32 bit address space laid out like the original (`src/rt`), with Win32, DirectX and the Watcom C
runtime replaced by small host services on SDL (`src/host`). Roadkill's plan is to clean that up
area by area into plain C, keeping the game playable and checked against the decomp at each step.

## License

Roadkill's code is licensed under the GNU General Public License, version 3 or (at your option)
any later version: see `LICENSE`. It does not cover Twisted Metal 2 itself or anything read from
your copy of the game.

## Credits

The decompilation and this port: Milenko. Tokatta007's PlayStation decompilation
([TM2-Decomp](https://github.com/Tokatta007/TM2-Decomp)) was the map for names and intent while
verifying the PC code; for the PlayStation version on PC, with online play and much more, see Tokatta007's
[OpenTM2](https://github.com/Tokatta007/OpenTM2). Twisted Metal 2 is © its owners.
