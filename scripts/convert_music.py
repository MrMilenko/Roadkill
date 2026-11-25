#!/usr/bin/env python3
"""Converts the Twisted Metal 2 CD's music to MP3 (or FLAC) files that Roadkill plays in place of the
CD image, so the .bin (the whole disc, about 700 MB) doesn't need to come along.

    python3 scripts/convert_music.py path/to/TWISTEDMETAL2.cue [--out DIR] [--format mp3|flac]
                                     [--bitrate 192k]

Needs ffmpeg on the PATH. Writes into DIR (default: a "music" folder next to the .cue):
TrackNN.mp3 for each audio track and music.cue, which lays them out where they are on the disc
(the data track becomes silence of the same length), so the game's CD positions and track
lengths stay the original's. Put the "music" folder inside Roadkill's "data" folder, or set
[paths] cd_image in roadkill.ini to music.cue. FLAC is lossless (about half the size of the
image's audio); MP3 at 192 kbit/s is about a seventh.

The .cue is read as Roadkill reads it (src/host/cd_image.c): FILE "x" BINARY, TRACK nn AUDIO /
MODEx/2352, INDEX 00 / 01 mm:ss:ff; audio sectors are 2352 bytes of 44.1 kHz 16 bit little
endian stereo.
"""
import argparse, os, re, shutil, subprocess, sys

SECTOR = 2352


def msf(s):
    m, sec, f = (int(x) for x in s.split(":"))
    return (m * 60 + sec) * 75 + f


def to_msf(lba):
    return "%02d:%02d:%02d" % (lba // (75 * 60), lba // 75 % 60, lba % 75)


def find(dirname, name):
    p = os.path.join(dirname, name)
    if os.path.exists(p):
        return p
    for e in os.listdir(dirname):
        if e.lower() == name.lower():
            return os.path.join(dirname, e)
    sys.exit(f"convert_music: {name} (named in the .cue) is not in {dirname}")


def read_cue(path):
    base_dir = os.path.dirname(os.path.abspath(path))
    files, tracks = [], []   # files: [path, first lba, sectors]; tracks: {no, audio, file, idx0, idx1}
    for line in open(path, encoding="latin-1"):
        s = line.strip()
        u = s.upper()
        if u.startswith("FILE "):
            m = re.match(r'FILE\s+"([^"]*)"|FILE\s+(\S+)', s, re.I)
            name = m.group(1) if m.group(1) is not None else m.group(2)
            fp = find(base_dir, name)
            first = files[-1][1] + files[-1][2] if files else 0
            files.append([fp, first, os.path.getsize(fp) // SECTOR])
        elif u.startswith("TRACK "):
            no = int(s.split()[1])
            tracks.append({"no": no, "audio": "AUDIO" in u, "file": len(files) - 1, "idx0": None, "idx1": None})
        elif u.startswith("INDEX ") and tracks:
            _, idx, t = s.split()[:3]
            lba = files[tracks[-1]["file"]][1] + msf(t)
            tracks[-1]["idx0" if int(idx) == 0 else "idx1"] = lba
    if not tracks:
        sys.exit("convert_music: no tracks in the .cue")
    end = files[-1][1] + files[-1][2]
    return files, tracks, end


def read_sectors(files, lba, n):
    out = bytearray()
    while n > 0:
        f = next(f for f in files if f[1] <= lba < f[1] + f[2])
        k = min(n, f[1] + f[2] - lba)
        with open(f[0], "rb") as h:
            h.seek((lba - f[1]) * SECTOR)
            out += h.read(k * SECTOR)
        lba += k
        n -= k
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("cue")
    ap.add_argument("--out")
    ap.add_argument("--format", choices=["mp3", "flac"], default="mp3")
    ap.add_argument("--bitrate", default="192k", help="MP3 bit rate (default 192k)")
    a = ap.parse_args()
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        sys.exit("convert_music: needs ffmpeg (macOS: brew install ffmpeg; Debian / Ubuntu: apt install ffmpeg; "
                 "Windows: winget install ffmpeg)")
    files, tracks, end = read_cue(a.cue)
    out = a.out or os.path.join(os.path.dirname(os.path.abspath(a.cue)), "music")
    os.makedirs(out, exist_ok=True)
    # each track's piece of the disc: from its INDEX 00 (else 01) to the next one's
    starts = [t["idx0"] if t["idx0"] is not None else t["idx1"] for t in tracks]
    starts[0] = 0
    cue = [f"REM Roadkill music, made by scripts/convert_music.py from {os.path.basename(a.cue)}"]
    for i, t in enumerate(tracks):
        lo, hi = starts[i], starts[i + 1] if i + 1 < len(tracks) else end
        kind = "AUDIO" if t["audio"] else "MODE1/2352"
        if t["audio"]:
            name = "Track%02d.%s" % (t["no"], a.format)
            codec = ["-c:a", "libmp3lame", "-b:a", a.bitrate] if a.format == "mp3" else ["-c:a", "flac"]
            print(f"track {t['no']:2d}: {(hi - lo) / 75:6.1f} s -> {name}")
            p = subprocess.run([ffmpeg, "-v", "error", "-y", "-f", "s16le", "-ar", "44100", "-ac", "2", "-i", "-",
                                *codec, os.path.join(out, name)], input=read_sectors(files, lo, hi - lo))
            if p.returncode:
                sys.exit(f"convert_music: ffmpeg failed on track {t['no']}")
            cue.append(f'FILE "{name}" {a.format.upper()}')
        else:
            print(f"track {t['no']:2d}: data ({hi - lo} sectors), kept as silence")
            cue.append('FILE "" NONE')
        cue.append(f"REM SECTORS {hi - lo}")
        cue.append(f"  TRACK {t['no']:02d} {kind}")
        if t["idx0"] is not None and i > 0:
            cue.append(f"    INDEX 00 {to_msf(t['idx0'] - lo)}")
        cue.append(f"    INDEX 01 {to_msf((t['idx1'] if t['idx1'] is not None else lo) - lo)}")
    with open(os.path.join(out, "music.cue"), "w", newline="\r\n") as h:
        h.write("\n".join(cue) + "\n")
    print(f"wrote {out}{os.sep}music.cue: put the {os.path.basename(out)} folder in Roadkill's data folder")


if __name__ == "__main__":
    main()
