# Roadkill

Twisted Metal 2 for Windows 95 (1997), native on modern machines. Roadkill is built from the
[TM2-Win9x Decomp](https://github.com/MrMilenko/TM2-Win9x-Decomp): the original `TM2.EXE`
decompiled into C, every function the game reaches checked against the original machine code. It
plays like the PC game; anything extra is a setting, off by default.

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

Keyboard as in the original (arrows to drive, Enter to select, Esc back); game controllers work
as joysticks once switched on in the game's input settings. Settings the game saves (the original
kept them in the Windows registry) go to `~/.config/opentm2/registry.txt`.

Useful switches (environment variables):

| | |
|---|---|
| `TM2_DATA=<folder>` | where the game files are (default `disc/files`) |
| `TM2_CDIMAGE=<file.cue>` | the disc image for the music |
| `TM2_FULLSCREEN=1`, `TM2_SCALE=<n>` | full screen; window scale (default 2) |
| `TM2_NOSOUND=1`, `TM2_NOJOY=1` | no sound; no controllers |

## Where it stands

From the title screen through the menus into matches, with sound, CD music and controllers, on
macOS. Known gaps: the game's setup dialogs (audio, input) are not shown yet, there is no network
play and no movies, and Windows and Linux builds have not been tried. `docs/RUNTIME.md` lists
the details.

## How it works

`src/<area>` is the decompiled game code. Today it still runs the way the decomp checks it: in a
32 bit address space laid out like the original (`src/rt`), with Win32, DirectX and the Watcom C
runtime replaced by small host services on SDL (`src/host`). Roadkill's plan is to clean that up
area by area into plain C, keeping the game playable and checked against the decomp at each step.

## Credits

The decompilation and this port: Milenko. Tokatta007's PlayStation decompilation
([TM2-Decomp](https://github.com/Tokatta007/TM2-Decomp)) was the map for names and intent while
verifying the PC code; for the PlayStation version on PC, see Tokatta007's
[OpenTM2](https://github.com/Tokatta007/OpenTM2). Twisted Metal 2 is © its owners.
