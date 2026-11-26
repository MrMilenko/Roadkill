#!/usr/bin/env python3
"""Sets up Roadkill's "data" folder from your Twisted Metal 2 PC disc, in one step.

    python3 import_disc.py SOURCE [DATA_FOLDER] [--format mp3|flac] [--no-music]

SOURCE is any of:
  - a .cue / .bin image of the CD (the best: it has the music too)
  - an .iso of the CD (the game's files only: an .iso holds no CD audio, so no music)
  - the CD itself or a folder with its files (a drive letter such as D:\\, /Volumes/TM2, /media/cdrom):
    the files only; a mounted disc's music tracks aren't files, rip the disc to .cue / .bin for those

DATA_FOLDER defaults to the "data" folder next to this script (where Roadkill looks first).

The game's files are copied out of the image's data track (an ISO 9660 file system, read here: no
mounting needed). The music, from a .cue / .bin, becomes MP3s (or FLAC) in DATA_FOLDER/music with
scripts/convert_music.py, which needs ffmpeg; without ffmpeg the .cue / .bin are copied in as they
are (Roadkill plays them, about 700 MB). Needs Python 3.8 or later.
"""
import argparse, os, re, shutil, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import convert_music  # noqa: E402


class Track1:
    """Reads 2048 byte user data sectors from the data track of a .cue / .bin or an .iso."""

    def __init__(self, path):
        self.f = None
        if path.lower().endswith(".cue"):
            base = os.path.dirname(os.path.abspath(path))
            name = mode = None
            for line in open(path, encoding="latin-1"):
                s = line.strip()
                if s.upper().startswith("FILE ") and name is None:
                    m = re.match(r'FILE\s+"([^"]*)"|FILE\s+(\S+)', s, re.I)
                    name = m.group(1) if m.group(1) is not None else m.group(2)
                elif s.upper().startswith("TRACK ") and mode is None:
                    mode = s.split()[2].upper()
            if not name or not mode or not mode.startswith("MODE"):
                sys.exit("import_disc: the .cue's first track isn't a data track")
            self.f = open(convert_music.find(base, name), "rb")
            size = int(mode.split("/")[1]) if "/" in mode else 2352
            self.sector = size
            self.offset = 0 if size == 2048 else (16 if mode.startswith("MODE1") else 24)
        else:
            self.f = open(path, "rb")
            self.sector, self.offset = 2048, 0

    def read(self, lba, n):
        out = bytearray()
        for i in range(n):
            self.f.seek((lba + i) * self.sector + self.offset)
            out += self.f.read(2048)
        return bytes(out)


def extract(img, out):
    pvd = img.read(16, 1)
    if pvd[0] != 1 or pvd[1:6] != b"CD001":
        sys.exit("import_disc: no ISO 9660 file system in the data track")
    root = pvd[156:156 + 34]
    count = [0, 0]

    def walk(lba, size, dest):
        os.makedirs(dest, exist_ok=True)
        data = img.read(lba, (size + 2047) // 2048)
        pos = 0
        while pos < size:
            ln = data[pos]
            if ln == 0:   # records don't cross a sector: on to the next one
                pos = (pos // 2048 + 1) * 2048
                continue
            rec = data[pos:pos + ln]
            pos += ln
            ext, dlen, flags, nlen = struct.unpack_from("<I", rec, 2)[0], struct.unpack_from("<I", rec, 10)[0], rec[25], rec[32]
            name = rec[33:33 + nlen]
            if name in (b"\0", b"\1"):
                continue
            name = name.decode("latin-1").split(";")[0].rstrip(".")
            if flags & 2:
                walk(ext, dlen, os.path.join(dest, name))
            else:
                with open(os.path.join(dest, name), "wb") as h:
                    left, at = dlen, ext
                    while left > 0:
                        k = min(left, 512 * 2048)
                        chunk = img.read(at, (k + 2047) // 2048)[:k]
                        h.write(chunk)
                        left -= k
                        at += (k + 2047) // 2048
                count[0] += 1
                count[1] += dlen

    walk(struct.unpack_from("<I", root, 2)[0], struct.unpack_from("<I", root, 10)[0], out)
    return count


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("source", help=".cue / .iso / the CD or a folder with its files")
    ap.add_argument("data", nargs="?", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "data"))
    ap.add_argument("--format", choices=["mp3", "flac"], default="mp3", help="the music's format (default mp3)")
    ap.add_argument("--no-music", action="store_true", help="the game's files only")
    a = ap.parse_args()
    src, data = a.source, os.path.abspath(a.data)
    os.makedirs(data, exist_ok=True)

    if os.path.isdir(src):
        exe = [f for f in os.listdir(src) if f.upper() == "TM2.EXE"]
        if not exe:
            sys.exit(f"import_disc: {src} has no TM2.EXE (is it the Twisted Metal 2 PC disc?)")
        print(f"copying the game's files from {src} ...")
        shutil.copytree(src, data, dirs_exist_ok=True)
    elif src.lower().endswith((".cue", ".iso", ".bin")):
        if src.lower().endswith(".bin"):
            cue = os.path.splitext(src)[0] + ".cue"
            if os.path.exists(cue):
                src = cue
            else:
                sys.exit("import_disc: give the .cue that goes with the .bin (it says where the music tracks are)")
        print(f"copying the game's files out of {os.path.basename(src)} ...")
        n, size = extract(Track1(src), data)
        print(f"  {n} files, {size / 1048576:.0f} MB")
    else:
        sys.exit("import_disc: SOURCE should be a .cue, an .iso, or a folder / the CD")

    if not any(f.upper() == "TM2.EXE" for f in os.listdir(data)):
        sys.exit("import_disc: no TM2.EXE came out: is this the Twisted Metal 2 PC disc?")

    if a.no_music or not src.lower().endswith(".cue"):
        if not a.no_music:
            print("no music: only a .cue / .bin image has the CD's music tracks (see README.txt)")
    elif shutil.which("ffmpeg"):
        print("converting the music ...")
        convert_music.convert(src, os.path.join(data, "music"), a.format)
    else:
        print("ffmpeg isn't installed: copying the .cue / .bin in for the music (install ffmpeg and run this again")
        print("for MP3s, about 45 MB in place of 700)")
        base = os.path.dirname(os.path.abspath(src))
        shutil.copy2(src, data)
        for line in open(src, encoding="latin-1"):
            m = re.match(r'\s*FILE\s+"([^"]*)"|\s*FILE\s+(\S+)', line, re.I)
            if m:
                shutil.copy2(convert_music.find(base, m.group(1) or m.group(2)), data)
    print(f"done: {data}")


if __name__ == "__main__":
    main()
