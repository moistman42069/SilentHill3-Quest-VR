/* SH3 Quest VR: the POSIX names sh3-nx's runtime uses, for the Windows PC test build (clang, MSVC
 * libraries). Kept free of windows.h; the functions are in compat_win.c. */
#pragma once
#include <io.h>
#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#define mkdir(p, mode) _mkdir(p)
#define ftruncate(fd, n) _chsize_s((fd), (n))
#define ftello _ftelli64
#define fseeko _fseeki64
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#ifndef SIGUSR1
#define SIGUSR1 30  /* no such signal on Windows: signal() refuses it, the debug dump stays off */
#define SIGUSR2 31
#endif

unsigned sleep(unsigned seconds);
int usleep(unsigned long usec);
