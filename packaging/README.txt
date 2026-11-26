ROADKILL: Twisted Metal 2 for Windows 95, native on modern machines
====================================================================

https://github.com/MrMilenko/Roadkill

No game data is included: you need your own Twisted Metal 2 PC CD (the
1997 Windows 95 version).

SETTING UP: THE EASY WAY (import_disc.py)
-----------------------------------------
import_disc.py, next to Roadkill, fills the "data" folder from your disc
or a disc image in one step: the game's files, and the CD music as MP3s.
It needs Python 3.8+ and, for the music, ffmpeg:

  Windows  winget install Python.Python.3.12 Gyan.FFmpeg
           (or python.org and ffmpeg.org; reopen the terminal after)
  macOS    brew install python ffmpeg
  Linux    sudo apt install python3 ffmpeg   (or your distribution's)

Then, in a terminal in the Roadkill folder:

  Windows  py import_disc.py "C:\path\to\Twisted Metal 2.cue"
  macOS /  python3 import_disc.py "/path/to/Twisted Metal 2.cue"
  Linux

What to give it:

  .cue / .bin   a rip of the CD: the best, it has the music too. Give the
                .cue (the .bin must be next to it).
  .iso          the game's files only: an .iso can't hold the CD's music
  the CD        the drive (D:\, /Volumes/TWISTED_METAL_2, /media/cdrom):
                the game's files only, as a computer reads a game CD as
                files; for the music, rip the CD to .cue / .bin:
                  Windows: ImgBurn, "Create image file from disc" (.bin/.cue)
                  macOS / Linux: cdrdao (brew / apt install cdrdao):
                    cdrdao read-cd --read-raw --datafile tm2.bin tm2.toc
                    toc2cue tm2.toc tm2.cue

Without ffmpeg it copies the .cue / .bin in as they are (Roadkill plays
them, about 700 MB instead of 45). Run it again any time; it overwrites.

SETTING UP: BY HAND
-------------------
1. Copy everything from the CD into the "data" folder next to Roadkill.
   TM2.EXE must end up directly in "data".
2. Music (optional): put the CD's .cue / .bin in "data" too, or make MP3s
   with convert_music.py ("data/PUT_YOUR_GAME_FILES_HERE.txt" has more).

STARTING
--------
  Windows  double-click roadkill.exe
  macOS    double-click roadkill, or ./roadkill in Terminal. The first
           time, macOS blocks programs that aren't notarized: right-click
           roadkill > Open > Open, or System Settings > Privacy & Security
           > Open Anyway, or: xattr -dr com.apple.quarantine <this folder>
  Linux    ./roadkill (an X11 or Wayland desktop; sound through
           PulseAudio / PipeWire or ALSA)

If Roadkill can't find TM2.EXE it says which folder it looked in. Rather
keep the game files somewhere else? Put them in "data" in your settings
folder (below), or set "data =" under [paths] in roadkill.ini.

PLAYING
-------
Keyboard as in the original: arrows to drive, Enter to select, Esc to go
back. A connected game controller works as the game's joystick (set its
buttons in Options > Input Config). Quit: Esc on the title screen, or
close the window.

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

NETWORK PLAY
------------
Multi Player works between Roadkill players on Windows, macOS, Linux and
the original Xbox, over the internet or your home network: the game's own
1997 lobby, chat, car select and matches, up to 8 players.

  Host:  Multi Player > Internet TCP/IP > New Game. Your game is listed on
         roadkill.horrible.dev (set [net] public = 0 in roadkill.ini to keep
         it off the list) and on your home network.
  Join:  Multi Player > Internet TCP/IP > Join: pick a game from the list.
         Or set [net] join = <address>:27420 in roadkill.ini to ask a host
         directly.

Roadkill uses UDP port 27420 (the next free one up to 27429). Most home
routers let players reach each other without any setup; if a friend can't
join you, forward UDP 27420 to your computer. Your names can be set in
roadkill.ini too: [player] name / alias, [net] session_name.
No computer opponents in network games (as in the original).

LICENSE
-------
Roadkill is GPL-3.0-or-later (LICENSE.txt). It includes SDL 2 (zlib
license), FFmpeg's Indeo video decoders (LGPL-2.1-or-later) and dr_mp3 /
dr_flac (public domain); see THIRD_PARTY.txt. Source:
https://github.com/MrMilenko/Roadkill
Twisted Metal is a trademark of Sony Interactive Entertainment. Roadkill
is not affiliated with or endorsed by Sony.
