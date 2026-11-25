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

## Get it

Builds for Windows (64-bit), Linux (x86-64) and macOS (Apple Silicon and Intel) are on the
[Releases page](https://github.com/MrMilenko/Roadkill/releases); every push to `main` also
builds them ([Actions](https://github.com/MrMilenko/Roadkill/actions)). Each zip holds the
program, an empty `data` folder and a `README.txt`.

## Play

1. **Copy everything from your Twisted Metal 2 PC CD into the `data` folder next to Roadkill.**
   `TM2.EXE` must end up directly in `data` (with `SINTAB.DAT` and the folders `CARSDB`,
   `LEVELDB`, `MOVIES`, `SHELLDB1` and the rest beside it). Upper or lower case names both work.
2. **Music (optional):** the music is CD audio, so it is not in those files. Put a `.cue` / `.bin`
   image of the CD in `data` too.
3. Start `roadkill` (`roadkill.exe` on Windows). If it can't find the files, it says which folder
   it looked in.

Roadkill looks for `TM2.EXE` in this order: `[paths] data` in `roadkill.ini` (or `TM2_DATA`);
`data/` next to the program; `data/` in your settings folder (below); `disc/files/` in the
folder it starts in (a source checkout). The music: the first `.cue` in that folder, else in
its parent (`disc/` in a checkout), or `[paths] cd_image`.

macOS: the builds aren't notarized, so the first start needs right-click > Open (or System
Settings > Privacy & Security > Open Anyway).

Keyboard as in the original (arrows to drive, Enter to select, Esc back); a connected game
controller works as the game's joystick. Quit with Esc on the title screen, or close the window.

Settings are in `roadkill.ini`, written with commented defaults on the first run in your user
data folder (macOS `~/Library/Application Support/Roadkill/`, Linux `~/.local/share/roadkill/`,
Windows `%APPDATA%\Roadkill\`). The settings the game saves itself (the original kept them in
the Windows registry) go to `registry.txt` next to it. Environment variables override the file:

| `roadkill.ini` | variable | |
|---|---|---|
| `[paths] data` | `TM2_DATA` | where the game files are (default: the search above) |
| `[paths] cd_image` | `TM2_CDIMAGE` | the disc image's `.cue` for the music |
| `[display] fullscreen`, `scale` | `TM2_FULLSCREEN=1`, `TM2_SCALE=<n>` | full screen; window scale (default 2) |
| `[audio] sound`, `*_volume` | `TM2_NOSOUND=1`, `TM2_VOLUME=<0..100>` | no sound; volumes |
| `[input] controller` | `TM2_NOJOY=1` | controllers (default on) |
| `[gameplay] loop_music`, `fix_crashes` | `TM2_LOOP_MUSIC=1`, `TM2_FIX_CRASHES=1` | changes to the original, off by default |

`docs/QOL.md` describes every switch.

## Build

With CMake and SDL2 (macOS `brew install cmake sdl2`; Debian / Ubuntu `apt install cmake
libsdl2-dev`; Fedora `dnf install cmake SDL2-devel`; Arch `pacman -S cmake sdl2`):

```sh
./scripts/build.sh          # build/roadkill; a source checkout reads disc/files/
```

Windows, cross compiled with MinGW-w64 (from macOS `brew install mingw-w64`, from Linux your
distribution's `mingw-w64` package):

```sh
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DROADKILL_BUNDLED_SDL=ON
cmake --build build-win
```

Release builds use `-DROADKILL_BUNDLED_SDL=ON` (SDL 2.32.10 built from source and linked in: one
file, nothing to install) and `./scripts/package.sh <build dir> <name>` for the zip;
`.github/workflows/build.yml` does both for all three systems.

## Where it stands

From the title screen through the menus into matches, with movies, sound, CD music and
controllers, played on macOS; the Windows build is checked under Wine and the Linux build so far
only starts in CI (reports welcome). Not there yet: network play (Multi Player goes back to
the title screen: the game finds no DirectPlay service; a modern replacement is planned).
`docs/RUNTIME.md` lists the details.

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
