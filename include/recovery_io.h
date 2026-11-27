#ifndef AH_RECOVERY_IO_H
#define AH_RECOVERY_IO_H
/* ISO C fopen need not set errno. On classic vbcc use the DOS error instead. */
#if defined(__VBCC__) && !defined(__PPC__)
#include <dos/dos.h>
#include <proto/dos.h>
static int ah_file_missing(const char *path)
{long error;BPTR lock=Lock((STRPTR)path,ACCESS_READ);if(lock){UnLock(lock);return 0;}error=IoErr();return error==ERROR_OBJECT_NOT_FOUND||error==ERROR_DIR_NOT_FOUND;}
#else
#include <errno.h>
static int ah_file_missing(const char *path){(void)path;return errno==ENOENT;}
#endif
#endif
