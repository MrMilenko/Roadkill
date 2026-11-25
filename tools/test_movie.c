/*
 * test_movie.c: decodes the first N frames of one of the game's AVI movies with Roadkill's AVI
 * reader (src/host/movies/avi.c) and Indeo decoders (third_party/indeo) and writes them out, to
 * check the decoders against a reference.
 *
 *   test_movie FILE.AVI N OUTDIR
 *     OUTDIR/frame_0000.ppm ..   each frame as RGB (the conversion the game's overlay uses)
 *     OUTDIR/frames.yuv          the N frames as raw planar YUV 4:1:0 (Y w x h, U, V (w/4 x h/4)),
 *                                comparable byte for byte with
 *                                ffmpeg -i FILE.AVI -frames:v N -f rawvideo -pix_fmt yuv410p ref.yuv
 *   and prints the stream's format, the frames decoded and the audio size.
 * A 0 byte chunk or a sync / null frame repeats the previous picture (as players show it).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host/movies/avi.h"
#include "indeo.h"

static int write_ppm(const char* path, const uint32_t* px, int w, int h)
{
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint8_t rgb[3] = {(uint8_t)(px[i] >> 16), (uint8_t)(px[i] >> 8), (uint8_t)px[i]};
        fwrite(rgb, 1, 3, f);
    }
    return fclose(f);
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s FILE.AVI N OUTDIR\n", argv[0]);
        return 2;
    }
    int n = atoi(argv[2]);
    const char* why = NULL;
    AviFile* f = avi_open(argv[1], &why);
    if (!f) { fprintf(stderr, "%s: %s\n", argv[1], why); return 1; }
    const AviInfo* in = avi_info(f);
    char fc[5] = {(char)in->fourcc, (char)(in->fourcc >> 8), (char)(in->fourcc >> 16), (char)(in->fourcc >> 24), 0};
    printf("%s: %dx%d '%s', %d frames at %u us, audio %s %d Hz %d ch %d bit, %u bytes\n", argv[1], in->width,
           in->height, fc, in->frames, in->us_per_frame, in->has_audio ? "PCM" : "none", in->a_rate, in->a_channels,
           in->a_bits, in->a_bytes);
    IndeoDecoder* d = indeo_open(in->fourcc, in->width, in->height);
    if (!d) { fprintf(stderr, "no decoder for '%s'\n", fc); avi_close(f); return 1; }
    if (n > in->frames) n = in->frames;
    uint32_t* rgb = malloc(sizeof(uint32_t) * (size_t)in->width * (size_t)in->height);
    char path[1024];
    snprintf(path, sizeof path, "%s/frames.yuv", argv[3]);
    FILE* yuv = fopen(path, "wb");
    if (!rgb || !yuv) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    IndeoPicture pic;
    int have = 0, decoded = 0, errors = 0;
    for (int i = 0; i < n; i++) {
        const uint8_t* data;
        int size = avi_read_frame(f, i, &data);
        if (size < 0) { fprintf(stderr, "frame %d: read error\n", i); break; }
        int r = size ? indeo_decode(d, data, size, &pic) : 0;
        if (r < 0) { errors++; fprintf(stderr, "frame %d: decode error %d\n", i, r); }
        if (r > 0) { have = 1; decoded++; }
        if (!have) continue;
        indeo_to_xrgb(&pic, rgb, in->width * 4);
        snprintf(path, sizeof path, "%s/frame_%04d.ppm", argv[3], i);
        write_ppm(path, rgb, pic.width, pic.height);
        for (int p = 0; p < 3; p++) {
            int w = p ? (pic.width + 3) >> 2 : pic.width, h = p ? (pic.height + 3) >> 2 : pic.height;
            for (int y = 0; y < h; y++) fwrite(pic.plane[p] + (size_t)y * (size_t)pic.pitch[p], 1, (size_t)w, yuv);
        }
    }
    fclose(yuv);
    printf("%d chunks read, %d pictures decoded, %d errors\n", n, decoded, errors);
    free(rgb);
    indeo_close(d);
    avi_close(f);
    return errors ? 1 : 0;
}
