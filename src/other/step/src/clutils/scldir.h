
#ifdef __O3DB__
#include <OpenOODB.h>
#endif

/*
 * C++ interface to Unix file system dir structure
 */
#ifdef HAVE_UNISTD_H
#include <sys/types.h>
#include <unistd.h>
#endif

          /* unistd.h defines _POSIX_VERSION on POSIX.1 systems.  */
#if defined(DIRENT) || defined(_POSIX_VERSION)
#include <dirent.h>
#define NLENGTH(dirent) (strlen((dirent)->d_name))
#else /* not (DIRENT or _POSIX_VERSION) */
#define dirent direct
#define NLENGTH(dirent) ((dirent)->d_namlen)

#ifdef HAVE_SYS_DIR_H
#include <sys/dir.h>
//#elif HAVE_SYS_NDIR_H
//#include <sys/ndir.h>
//#elif HAVE_NDIR_H
//#include <ndir.h>
#endif

#endif /* not (DIRENT or _POSIX_VERSION) */

#if 0
#ifndef sys_dir_h

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(opendir)
#define opendir sys_dir_h_opendir
#define readdir sys_dir_h_readdir
#define telldir sys_dir_h_telldir
#define seekdir sys_dir_h_seekdir
#define closedir sys_dir_h_closedir
#endif

#if defined(SYSV)
#include "//usr/include/sys/types.h"
#include "//usr/include/dirent.h"
#else
#include "//usr/include/sys/dir.h"
#endif

#undef opendir
#undef readdir
#undef telldir
#undef seekdir
#undef closedir

#ifndef sys_dir_h
#define sys_dir_h
#endif

extern DIR* opendir(const char*);
#if defined(SYSV)
extern struct dirent* readdir(DIR*);
#else
extern struct direct* readdir(DIR*);
#endif
extern long telldir(DIR*);
extern void seekdir(DIR*);
extern int closedir(DIR*);

#if defined(__cplusplus)
}
#endif
#endif /* if 0 */

#endif
