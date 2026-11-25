/* sys/stat.h for the Xbox build: the CRT's, plus the POSIX names */
#ifndef RK_SYS_STAT_H
#define RK_SYS_STAT_H
#include <sys/types.h>
#include_next <sys/stat.h>
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
int rk_stat(const char* path, void* st);
#define stat(p, s) rk_stat((p), (s))
int rk_fstat(int fd, void* st);
#define fstat rk_fstat
#endif
