# Source layout

Roadkill is the port: its code is organized by what it does, in modules. (The decomp it comes from
names files by the wave that ported them, `en2_`, `pf4_`, `sh5_`, ...; those names go away here.
`docs/DECOMP_MAP.tsv` maps every Roadkill file back to its decomp file, so fixes can flow both
ways.)

```
src/
  app/            the programs: roadkill (main), tm2view (arena viewer)
  game/           the decompiled game logic (verified against TM2.EXE)
    ai/           AI drivers: routes, decisions, firing
    car/          car systems: driving model, input, stats, spawn, collisions, damage, wrecks
    combat/       weapons in play: projectiles, specials, pickups, drops, the arena's hazards
    weapons/      weapon effects and records shared by combat
    world/        level loading, terrain, collapse, warps, track vehicles, world tables
    physics/      terrain tiles, probes, ground zones
    scene/        DPC scene walk, collision against the level
    hud/          the in-match HUD: radar, gauges, messages, overlays
    match/        the match loop: session, rounds, cars update, saves, network session
    frontend/     the menus and screens: title, car / level select, options, the shell flow
    audio/        the game's sound and music calls (what to play, when)
  engine/         the decompiled engine
    render/       the software PS1 GPU (2D and 3D rasterisers)
    view/         cameras, viewports, projection, clipping, fog
    draw/         ordering tables, polygons, sprites, quads, debris, animation, CLUT
    ui/           widgets, windows, dialog objects, list views
    io/           files, paths, registry access, resources, loaders
    input/        keyboard, joystick and mouse state as the game reads them
    math/         fixed point math, matrices, rand
    formats/      DPC, TPC, TIM, mesh decoding
  platform/       the decompiled DirectX / Win32 wrapper layer (display, sound, CD, movies)
  runtime/        the address space and call table (rt), and bindings/ per module
  host/           the port's own services on SDL: not decompiled
    win32/        windows, messages, files, registry, threads, timers, GDI
    directx/      DirectDraw, DirectSound, DirectInput, DirectPlay objects
    audio/        the mixer, SDL audio
    cd/           CD audio from the disc image
    input/        keyboard, controllers, scripted input
    dialogs/      the game's Win32 dialog boxes drawn in game
    settings/     roadkill.ini, the user data folder
    qol/          the opt-in changes (docs/QOL.md)
    watcom/       the Watcom C runtime pieces the game calls
```

Rules:
- One module per folder; a file is named for what it holds (`car/damage.c`, not `gp2_damage.c`).
- `game/`, `engine/` and `platform/` are decompiled and verified: their default path is the
  verified code (CONTRIBUTING.md); `host/` is the port's.
- A module's bindings live in `runtime/bindings/<module>.c`.
- Headers are included by module path: `#include "game/car/damage.h"`.
