ROADKILL: Twisted Metal 2 for Windows 95, native on modern machines
====================================================================

https://github.com/MrMilenko/Roadkill

No game data is included: you need your own Twisted Metal 2 PC CD.

SETTING UP
----------
1. Copy everything from your Twisted Metal 2 PC CD into the "data"
   folder next to Roadkill. TM2.EXE must end up directly in "data".
   ("data/PUT_YOUR_GAME_FILES_HERE.txt" explains it step by step.)
2. Music (optional): put a .cue / .bin image of the CD in "data" too, or
   convert its music to MP3s with convert_music.py (see the note in
   "data") and put the "music" folder it makes in "data".
3. Start Roadkill:
     Windows  double-click roadkill.exe
     macOS    double-click roadkill, or run ./roadkill in Terminal
              (first time: macOS blocks programs from the internet that
              aren't notarized. Right-click roadkill, choose Open, then
              Open again; or System Settings > Privacy & Security >
              Open Anyway; or in Terminal:
              xattr -dr com.apple.quarantine <this folder>)
     Linux    ./roadkill (an X11 or Wayland desktop; for sound,
              PulseAudio / PipeWire or ALSA)

Rather keep the game files somewhere else? Either put them in "data" in
your settings folder (below), or set "data =" under [paths] in
roadkill.ini to the folder that holds TM2.EXE.

PLAYING
-------
Keyboard as in the original: arrows to drive, Enter to select, Esc to go
back. A connected game controller works as the game's joystick.
Quit: Esc on the title screen, or close the window.

SETTINGS
--------
roadkill.ini is written with every option and a comment the first time
Roadkill runs, in:
  Windows  %APPDATA%\Roadkill\
  macOS    ~/Library/Application Support/Roadkill/
  Linux    ~/.local/share/roadkill/
Roadkill plays like the 1997 PC game by default. Widescreen, higher
resolution, fullscreen, volumes, looping music, fixes for the original's
own bugs: all switches in that file, off until you turn them on.
The game's own saved settings go to registry.txt in the same folder.

NOT YET
-------
Network play (Multi Player) is not there yet: the menu goes back to the
title screen.

LICENSE
-------
Roadkill is GPL-3.0-or-later (LICENSE.txt). It includes SDL 2 (zlib
license), FFmpeg's Indeo video decoders (LGPL-2.1-or-later) and dr_mp3 /
dr_flac (public domain); see
THIRD_PARTY.txt. Source: https://github.com/MrMilenko/Roadkill
Twisted Metal is a trademark of Sony Interactive Entertainment. Roadkill
is not affiliated with or endorsed by Sony.
