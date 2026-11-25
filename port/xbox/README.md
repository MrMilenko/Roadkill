# Roadkill on the original Xbox

`make -C port/xbox -j8` builds `build/xbox/default.xbe` on macOS or Linux with
[OXDK](https://github.com/MrMilenko/OXDK) (clang, lld-link, cxbe) and its SDL2x port. You need
OXDK in `~/OXDK` (or `OXDK_DIR`) with your own XDK's libraries and headers in OXDK's `xbox/xdk`, as
OXDK's instructions say; none of it is in this repository.

The game is Roadkill's own `src/`, unchanged but for a few `ROADKILL_XBOX` / `ROADKILL_NO_TLS`
switches. This folder adds what the desktop gets from its C library and OS:

| File | |
|---|---|
| `compat/` | the C99 / POSIX headers the XDK's Visual C++ 2003 library lacks (`rk_xbox.h` is included first in every file) |
| `xbox_os.c` | `host/os/os.h`: the 64 MB game space reserved and committed as the game touches it; per-thread blocks in place of `__thread` |
| `xbox_posix.c`, `xbox_pthread.c`, `xbox_libm.c` | snprintf, the environment, `/` paths, directories, clocks; threads, mutexes, condition variables; C99 math |
| `xbox_main.c` | the entry point, `D:\roadkill.log`, the pad as arrow keys / Enter / Esc in the menus |

Install: the XBE in a folder on the hard disk with `data\` beside it holding everything from the
Twisted Metal 2 PC CD, and for the music `data\music\` from `scripts/convert_music.py` (or the CD
image's `.cue` / `.bin`, ten times the size). Settings and the log go
next to the XBE.

Status: a first test build. The game draws through Roadkill's DirectDraw host into an SDL texture
that SDL2x presents with Direct3D 8; a native Direct3D 8 path for the game's own Direct3D is the
next step.
