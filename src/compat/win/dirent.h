/* SH3 Quest VR: opendir/readdir for the Windows PC test build (compat_win.c, over FindFirstFileA). */
#pragma once

struct dirent { char d_name[260]; };
typedef struct DIR DIR;
DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);
