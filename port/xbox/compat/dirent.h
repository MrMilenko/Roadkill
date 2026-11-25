/* dirent.h for the Xbox build: FindFirstFile / FindNextFile underneath (xbox_posix.c) */
#ifndef RK_DIRENT_H
#define RK_DIRENT_H
struct dirent { char d_name[260]; };
typedef struct RkDir DIR;
DIR* opendir(const char* path);
struct dirent* readdir(DIR* d);
int closedir(DIR* d);
#endif
