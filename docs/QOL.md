# Quality of life: candidates

Roadkill plays like the 1997 PC game by default. These are candidate switches, each off unless
noted, for the maintainer to approve before they are built.

| Candidate | Why | Notes |
|---|---|---|
| Window / full screen / integer scaling, chosen in a settings file | play on any screen | partly there as environment variables |
| Widescreen (more of the arena left and right) | modern screens | needs the view setup to take a wider aspect |
| Higher internal resolution | the original is 640x480 max | the software renderer would draw more pixels |
| Settings file next to the user data, not environment variables | ease | |
| Controller defaults that work without visiting the input screen | ease | the game only reads a pad when its saved switch says so |
| The setup dialogs (audio, input, key mapping) drawn in game | they are Win32 dialogs today, not shown | needed for parity, not only QoL |
| Fixes for the original's crash bugs (each documented in the decomp's verified.tsv) | stability | e.g. the pure virtual call on the network screen |
| CD music that loops a level's track | the original plays each track once | |
