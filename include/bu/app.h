/*                          A P P . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef BU_APP_H
#define BU_APP_H

#include "common.h"

#include <stdio.h> /* for FILE */

#include "bu/defines.h"


__BEGIN_DECLS


/** @addtogroup bu_app
 *
 * @brief Routines for inspecting and reporting characteristics of the
 * current running application environment.
 *
 */
/** @{ */


/**
 * DEPRECATED: This routine is replaced by bu_getcwd().
 *             Do not use.
 *
 * returns the full path to argv0, regardless of how the application
 * was invoked.
 *
 * this routine will return "(BRL-CAD)" if argv[0] cannot be
 * identified but should never return NULL.  this routine is not
 * thread-safe.
 */
BU_EXPORT extern const char *bu_argv0_full_path(void);

/**
 * Routine for getting the current working directory full pathname.
 */
BU_EXPORT extern char *bu_getcwd(char *buf, size_t size);

/**
 * Get the name of the running application.  application codes should
 * call bu_setprogname() first to ensure that the program name is
 * stored appropriately on platforms that do not have an intrinsic
 * method for tracking the program name automatically.
 *
 * while this routine is thread-safe and reentrant, the static string
 * returned is shared amongst all threads.
 */
BU_EXPORT extern const char *bu_getprogname(void);

/**
 * Set the name of the running application.  This isn't strictly
 * necessary on platforms that have an intrinsic method for tracking
 * the program name automatically, but is still recommended for
 * portability and is necessary on some strict modes of compilation.
 *
 * while the implementation relies on a static string shared across
 * all threads, this routine is thread-safe and reentrant.
 */
BU_EXPORT extern void bu_setprogname(const char *path);

/**
 * returns the first USER path match to a given executable name.
 *
 * Routine to provide BSD "which" functionality, locating binaries of
 * specified programs from the user's PATH. This is useful to locate
 * binaries and resources at run-time.
 *
 * caller should not free the result, though it will not be preserved
 * between calls either.  the caller should strdup the result if they
 * need to keep it around.
 *
 * routine will return NULL if the executable command cannot be found.
 */
BU_EXPORT extern const char *bu_which(const char *cmd);

/**
 * returns the first SYSTEM path match to a given executable cmd name.
 *
 * Routine to provide BSD "whereis" functionality, locating binaries
 * of specified programs from the SYSTEM path.  This is useful to
 * locate binaries and resources at run-time.
 *
 * caller should not free the result, though it will not be preserved
 * between calls either.  the caller should strdup the result if they
 * need to keep it around.
 *
 * routine will return NULL if the executable command cannot be found.
 */
BU_EXPORT extern const char *bu_whereis(const char *cmd);


/**
 * @brief
 * Routine to open a temporary file.
 *
 * Create a temporary file.  The first readable/writable directory
 * will be used, searching TMPDIR/TEMP/TMP environment variable
 * directories followed by default system temp directories and
 * ultimately trying the current directory.
 *
 * This routine is guaranteed to return a new unique file or return
 * NULL on failure.  The temporary file will be automatically unlinked
 * on application exit.  It is the caller's responsibility to set file
 * access settings, preserve file contents, or destroy file contents
 * if the default behavior is non-optimal.
 *
 * The name of the temporary file will be copied into a user-provided
 * (filepath) buffer if it is a non-NULL pointer and of a sufficient
 * (len) length to contain the filename.
 *
 * This routine is NOT thread-safe.
 *
 * Typical Use:
 @code
  FILE *fp;
  char filename[MAXPATHLEN];
  fp = bu_temp_file(&filename, MAXPATHLEN); // get file name
  ...
  fclose(fp); // close the file when you're done
  ...
  fp = bu_temp_file(NULL, 0); // don't need file name
  bu_fchmod(fileno(fp), 0777);
  ...
  rewind(fp);
  while (fputc(0, fp) == 0);
  fclose(fp);
 @endcode
 */
BU_EXPORT extern FILE *bu_temp_file(char *filepath, size_t len);


/**
 * @brief Wrapper around fchmod.
 *
 * Portable wrapper for setting a file descriptor's permissions ala
 * fchmod().
 */
BU_EXPORT extern int bu_fchmod(int fd, unsigned long pmode);


/**@brief BRL-CAD specific path queries */

/**
 * @brief
 * Report the relative paths being used to hold BRL-CAD applications,
 * libraries, and data.
 *
 * Recognized keys include:
 *
 * Key     | Looks Up
 * ------- | -------------------------------------------
 * bin     | Directory containing applications
 * lib     | Directory containing libraries
 * include | Directory containing headers
 * data    | Directory containing shared data
 * share   | Directory containing shared data
 * doc     | Directory containing documentation
 * man     | Directory containing Unix man pages
 *
 * @return
 * A STATIC buffer is returned.  It is the caller's responsibility to
 * call bu_strdup() or make other provisions to save the returned
 * string, before calling again.
 */
BU_EXPORT extern const char *bu_brlcad_dir(const char *dirkey, int fail_quietly);

/**
 * @brief
 * Locate where the BRL-CAD applications and libraries are installed.
 *
 * The BRL-CAD root is searched for in the following order of
 * precedence by testing for the rhs existence if provided or the
 * directory existence otherwise:
 *
 *   BRLCAD_ROOT environment variable if set
 *   run-time path identification
 *   BRLCAD_ROOT compile-time path
 *   current directory
 *
 * @return
 * A STATIC buffer is returned.  It is the caller's responsibility to
 * call bu_strdup() or make other provisions to save the returned
 * string, before calling again.
 */
BU_EXPORT extern const char *bu_brlcad_root(const char *rhs, int fail_quietly);


/** @} */

__END_DECLS

#endif  /* BU_APP_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
