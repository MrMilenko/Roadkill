PUT YOUR TWISTED METAL 2 FILES IN THIS FOLDER
=============================================

Roadkill does not include the game. It runs the files from your own
Twisted Metal 2 PC CD (the 1997 Windows 95 version).

1. Copy EVERYTHING from the CD into this "data" folder, so that it looks like:

       data/
         TM2.EXE
         SINTAB.DAT
         CARSDB/
         LEVELDB/
         MOVIES/
         SHELLDB1/  SHELLDB2/  SHELLDB3/
         SCREEN/  TERRAIN/  FONTS/  GRY/  BIOS/  ...

   TM2.EXE must sit directly in this folder, not in a subfolder.
   Upper or lower case names both work.

2. For the music (optional): the music is CD audio, so it isn't in
   those files. Either

   a) put a disc image of the CD in this same folder: the .cue file and
      its .bin file(s), e.g. data/Twisted Metal 2.cue + .bin; or

   b) convert the image's music to MP3s (much smaller, and the .bin
      isn't needed then): with Python 3 and ffmpeg installed, run

          python3 convert_music.py "path/to/Twisted Metal 2.cue"

      (convert_music.py is next to Roadkill) and move the "music" folder
      it makes into this folder: data/music/music.cue, Track02.mp3, ...

   Without either the game plays without music.

Then start Roadkill. If it can't find TM2.EXE, it tells you which folder
it looked in.
