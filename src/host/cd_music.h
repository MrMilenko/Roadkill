/*
 * cd_music.h: a CD audio track kept as a compressed file (MP3 or FLAC, scripts/convert_music.py),
 * decoded as 44.1 kHz 16 bit stereo for cd_image.c (port code). Stand alone (no runtime).
 */
#ifndef ROADKILL_CD_MUSIC_H
#define ROADKILL_CD_MUSIC_H

#include <stdint.h>
#include <stdio.h>

typedef struct CdMusic CdMusic;

/* takes f (closed by cd_music_close); kind: "MP3" or "FLAC" (the .cue's FILE type), else by the
 * name's extension. NULL when it can't be decoded, or isn't 44.1 kHz stereo. */
CdMusic* cd_music_open(FILE* f, const char* name, const char* kind);
void cd_music_close(CdMusic* m);
/* n stereo frames from frame `at` into out (int16 L R ...); past the end reads silence. Returns
 * the frames decoded. */
uint32_t cd_music_read(CdMusic* m, uint64_t at, uint32_t n, int16_t* out);
/* the track's length in whole CD sectors (588 frames), rounded up; measures the file when its
 * header doesn't say */
uint32_t cd_music_sectors(CdMusic* m);
/* 1 when name / kind is a type cd_music_open takes */
int cd_music_kind(const char* name, const char* kind);

#endif
