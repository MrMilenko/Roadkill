/*
 * host/data/data_dir.c: data_dir.h. Port code.
 */
#include "data_dir.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <SDL.h>

#include "host/os/os.h"
#include "host/settings/settings.h"

static char dir[1024], beside[1024], peruser[1024];
static int resolved, found;

static int holds_game(const char* d)
{
    DIR* h = opendir(d);
    if (!h) return 0;
    struct dirent* de;
    int ok = 0;
    while (!ok && (de = readdir(h)) != NULL) ok = !strcasecmp(de->d_name, "TM2.EXE");
    closedir(h);
    return ok;
}

static void trim(char* d)
{
    size_t l = strlen(d);
    while (l > 1 && (d[l - 1] == '/' || d[l - 1] == '\\')) d[--l] = 0;
}

static void resolve(void)
{
    if (resolved) return;
    resolved = 1;
#ifdef ROADKILL_XBOX
    snprintf(beside, sizeof beside, "D:\\data");   /* D: is the XBE's folder */
#else
    char* base = SDL_GetBasePath();   /* the executable's folder, with a trailing separator */
    snprintf(beside, sizeof beside, "%sdata", base ? base : "");
    SDL_free(base);
#endif
#ifdef _WIN32
    snprintf(peruser, sizeof peruser, "%s\\data", settings_dir());
#else
    snprintf(peruser, sizeof peruser, "%s/data", settings_dir());
#endif
    const char* set = settings_str("paths.data", NULL);
    /* roadkill.ini files written before the search existed say "disc/files" (the old default):
     * that counts as not set unless it holds the game */
    if (set && !strcmp(set, "disc/files") && !holds_game(set)) set = NULL;
    if (set) snprintf(dir, sizeof dir, "%s", set);
    else if (holds_game(beside)) snprintf(dir, sizeof dir, "%s", beside);
    else if (holds_game(peruser)) snprintf(dir, sizeof dir, "%s", peruser);
    else if (holds_game("disc/files")) snprintf(dir, sizeof dir, "disc/files");
    else snprintf(dir, sizeof dir, "%s", beside);
    trim(dir);
    found = holds_game(dir);
    os_setenv("TM2_DATA", dir);
}

const char* data_dir(void) { resolve(); return dir; }
int data_dir_found(void) { resolve(); return found; }

void data_dir_explain(void)
{
    resolve();
    char msg[4096];
    if (strcmp(dir, beside))
        snprintf(msg, sizeof msg,
            "Roadkill can't find TM2.EXE in the folder your settings point at:\n\n    %s\n\n"
            "Fix paths.data in\n    %s\n(or leave it empty), or set TM2_DATA.", dir, settings_file());
    else
        snprintf(msg, sizeof msg,
            "Roadkill needs the files from your Twisted Metal 2 PC CD.\n\n"
            "Copy everything on the CD (TM2.EXE, SINTAB.DAT and the folders CARSDB, LEVELDB, MOVIES, "
            "SHELLDB1 and the rest) into this folder:\n\n    %s\n\n"
            "or, if you'd rather keep them with your settings:\n\n    %s\n\n"
            "For the music, put a .cue / .bin image of the CD in the same folder.\n"
            "Another folder: set paths.data in\n    %s",
            beside, peruser, settings_file());
    fprintf(stderr, "roadkill: %s\n", msg);
    if (!getenv("TM2_KEYS")) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Roadkill: game files needed", msg, NULL);
}
