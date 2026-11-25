/*
 * cd_image.h: the player's CD image (a .cue sheet over raw 2352 byte sector .bin files), read at
 * run time for the CD audio tracks (port code, not from the original). No game data is copied:
 * the tracks are read straight from the player's image. Stand alone (no runtime), so
 * tools/rt/test_cdaudio.c builds on it too.
 *
 * Addresses: a disc LBA counts 1/75 s sectors from the first track's INDEX 01 (LBA 0); a real
 * disc's MSF address is LBA + 150 (the 2 s lead in). An audio sector is 2352 bytes = 588 frames
 * of 44.1 kHz 16 bit little endian stereo.
 */
#ifndef TM2_CD_IMAGE_H
#define TM2_CD_IMAGE_H

#include <stdint.h>

#define CD_MAX_TRACKS 99
#define CD_SECTOR_BYTES 2352
#define CD_SECTOR_FRAMES 588

typedef struct CdImage CdImage;

/* the .cue's path: TM2_CDIMAGE, else the first .cue in the data root's parent folder (TM2_DATA,
 * default disc/files: disc/). Returns out, or NULL when there is none. */
const char* cd_image_default_path(char* out, int cap);

/* open a .cue (each FILE ... BINARY opened read only); NULL on failure (why: a short reason) */
CdImage* cd_image_open(const char* cue, const char** why);
void cd_image_close(CdImage* c);

int cd_image_tracks(const CdImage* c);                /* number of tracks (1..99) */
int cd_image_is_audio(const CdImage* c, int track);   /* 1 for an AUDIO track */
uint32_t cd_image_track_lba(const CdImage* c, int track);    /* INDEX 01, disc LBA */
uint32_t cd_image_track_len(const CdImage* c, int track);    /* sectors up to the next track */
uint32_t cd_image_end_lba(const CdImage* c);                 /* one past the last sector */
int cd_image_track_at(const CdImage* c, uint32_t lba);       /* the track holding lba (1..n) */

/* read n audio sectors from lba into out (n * 588 stereo frames, host int16); sectors outside
 * the image or of a data track read as silence. Returns the sectors read from the file. */
int cd_image_read(CdImage* c, uint32_t lba, int n, int16_t* out);

#endif /* TM2_CD_IMAGE_H */
