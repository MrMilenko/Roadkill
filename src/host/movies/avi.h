/*
 * avi.h: a small AVI (RIFF "AVI ") reader for the game's movies (the .AVI files of
 * disc/files/MOVIES). Port code, not from the original: stands in for Video for Windows' AVIFile
 * under MCIAVI.
 *
 * It reads the main header (avih: dwMicroSecPerFrame), the first video and the first audio
 * stream (strh / strf: the BITMAPINFOHEADER's compression and size, the WAVEFORMATEX), and
 * lists every chunk of the movi list in file order, going into its "rec " lists (the files
 * interleave one video frame and its audio per record). The idx1 index is not needed: the walk
 * of movi gives the same chunks in the same order.
 */
#ifndef TM2_HOST_MOVIES_AVI_H
#define TM2_HOST_MOVIES_AVI_H

#include <stdint.h>

typedef struct AviFile AviFile;

typedef struct AviInfo {
    uint32_t us_per_frame;   /* avih dwMicroSecPerFrame (66667: 15 frames a second) */
    int frames;              /* video chunks in movi (a 0 byte chunk repeats the last picture) */
    uint32_t fourcc;         /* the video's compression (BITMAPINFOHEADER biCompression) */
    int width, height;
    int has_audio;
    int a_rate, a_channels, a_bits;   /* PCM (format tag 1) only */
    uint32_t a_bytes;                 /* the audio stream's data size */
} AviInfo;

/* NULL with *why set when the file cannot be read or is not an AVI with a video stream */
AviFile* avi_open(const char* path, const char** why);
void avi_close(AviFile* f);
const AviInfo* avi_info(const AviFile* f);

/* frame i's chunk: its data in *data (valid to the next call, 0 bytes possible); -1 on a read
 * error or i out of range */
int avi_read_frame(AviFile* f, int i, const uint8_t** data);

/* the whole audio stream (a_bytes, the chunks in order) into dst; returns the bytes read */
uint32_t avi_read_audio(AviFile* f, uint8_t* dst, uint32_t cap);

#endif /* TM2_HOST_MOVIES_AVI_H */
