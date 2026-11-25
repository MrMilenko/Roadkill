# Indeo 3 / Indeo 4 decoders (from FFmpeg)

The game's movies (`disc/files/MOVIES/*.AVI`) are Video for Windows files with Intel Indeo video:
Indeo 3 (`IV32`, every movie but one) and Indeo Video Interactive 4.1 (`IV41`, `STLOGO.AVI`).
These are FFmpeg's decoders for them, made to build on their own with no libavcodec / libavutil.

Source: FFmpeg 7.1 (git tag `n7.1`, commit `b08d7969c550a804a59511c7b83f2dd8cc0499b8`,
https://github.com/FFmpeg/FFmpeg), `libavcodec/`:

| File | From | Changes |
|---|---|---|
| `indeo3.c`, `indeo3data.h` | `indeo3.c`, `indeo3data.h` | the includes point at `ffcompat.h` (indeo3data.h: in place of `config.h`); the `FFCodec` table at the end is replaced by plain entry points (`ff_indeo3_init / _decode / _close`) |
| `indeo4.c`, `indeo4data.h` | `indeo4.c`, `indeo4data.h` | the includes; the `FFCodec` table replaced by `ff_indeo4_init` |
| `ivi.c`, `ivi.h` | `ivi.c`, `ivi.h` | the includes (`BITSTREAM_READER_LE` moved before them in ivi.c) |
| `ivi_dsp.c`, `ivi_dsp.h` | `ivi_dsp.c`, `ivi_dsp.h` | the includes |
| `ffcompat.h`, `ffcompat.c` | written for Roadkill after `libavutil` (intreadwrite.h, macros.h, common.h, mem.h) and `libavcodec` (get_bits.h, bytestream.h, vlc.h / vlc.c, copy_block.h, hpeldsp, mathtables.c) | the framework the decoders use: bit reader, VLC table builder (vlc.c's `build_table` / `ff_vlc_init_sparse`, kept), byte readers, small `AVCodecContext` / `AVFrame` / `AVPacket`, memory, logging (`INDEO_LOG=1`) |
| `indeo.h`, `indeo.c` | Roadkill | the front end Roadkill calls: open a decoder by FOURCC, decode a chunk, YUV 4:1:0 to XRGB |

The decoders' code itself is unchanged, so fixes from FFmpeg apply as they are. The bit reader
reads zeros past the end of its buffer instead of relying on FFmpeg's input padding.

Checked: `tools/test_movie FILE.AVI N DIR` writes the decoded frames as raw YUV 4:1:0; for all 17
movies of the game every frame is byte for byte the same as
`ffmpeg -i FILE.AVI -f rawvideo -pix_fmt yuv410p` (FFmpeg 7.1).

## License

GNU Lesser General Public License version 2.1 or later (`LICENSE`, FFmpeg's `COPYING.LGPLv2.1`);
each file keeps its FFmpeg copyright and license header. LGPL-2.1-or-later code may be combined
with Roadkill's GPL-3.0-or-later code. The build makes these files a static library (`indeo`)
linked into `roadkill`; its sources are all here.
