/* SH3 Quest VR: included first in every hand-written source of the PC test build (CMakeLists, WIN32 only; never in
 * the generated code). What the POSIX sources expect that the MSVC libraries name differently. */
#pragma once
#include <string.h>
#include <time.h>
#include <sys/types.h>

#ifndef strcasecmp
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

/* 64-bit file offsets and sizes like the Linux build's (off_t and struct stat's st_size are 32-bit here, and
 * GetFileSize's "st_size >> 32" then gave the size again as the high half: every file looked 4 GB times bigger);
 * fseeko/ftello map to the 64-bit calls in unistd.h. sys/types.h and sys/stat.h are already in, so their own
 * declarations are not touched. */
#include <sys/stat.h>
#define off_t long long
#define stat _stat64  /* struct stat and stat() both */
#define fstat _fstat64

static inline struct tm *gmtime_r(const time_t *t, struct tm *out) { return gmtime_s(out, t) ? NULL : out; }
static inline struct tm *localtime_r(const time_t *t, struct tm *out) { return localtime_s(out, t) ? NULL : out; }
