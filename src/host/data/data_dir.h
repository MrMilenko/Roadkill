/*
 * host/data/data_dir.h: where the player's Twisted Metal 2 files are (port code).
 *
 * The first of these that holds TM2.EXE (any letter case):
 *   1. paths.data in roadkill.ini, or TM2_DATA (used as given, found or not)
 *   2. data/ beside the Roadkill executable          (what the release zips ship, empty)
 *   3. data/ in the per user folder (settings_dir)   (~/Library/Application Support/Roadkill/data, ...)
 *   4. disc/files/ in the folder Roadkill starts in   (a source checkout)
 * The music: the disc image's .cue in that folder, else in its parent (cd_image.c).
 */
#ifndef ROADKILL_DATA_DIR_H
#define ROADKILL_DATA_DIR_H

/* the game's data folder, without a trailing separator; when none holds TM2.EXE, the folder the
 * player is asked to fill (2.). TM2_DATA is set to it, for the runtime's own readers. */
const char* data_dir(void);
/* 1 when data_dir() holds TM2.EXE */
int data_dir_found(void);
/* tells the player where the files go (a dialog, and stderr); for when data_dir_found() is 0 */
void data_dir_explain(void);

#endif
