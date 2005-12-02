/*                         W H I C H . C
 * BRL-CAD
 *
 * Copyright (C) 2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file which.c
 *
 * Routine to provide BSD "which" functionality, locating binaries of
 * specified programs from the user's PATH (i.e.  not necessarily the
 * system path).  This is useful to locate system binaries and
 * resources at run-time such as where the current binary is being
 * executed from.
 *
 *  Author -
 *	Christopher Sean Morrison
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
#  ifdef HAVE_SYS_PARAM_H
#    include <sys/param.h>
#  endif
#  include <sys/sysctl.h>
#endif

#include "machine.h"
#include "bu.h"


/** b u _ w h i c h
 *
 * Sets argv array of path matches to the given executable cmd name
 * returning up to lim number of occurances found.  the user path is
 * obtained getenv(PATH) or in its absense, via the sysctl
 * ``user.cs_path'' string.
 *
 * It is the callers responsibility to allocate sufficient memory to
 * the argv array itself (lim elements), memory for the array contents
 * will be automatically allocated as needed.  It is the callers
 * responsibility to free the array contents with bu_free_array() as
 * well as the argv array itself.
 */
int bu_which(char *argv[], int lim, const char *cmd)
{
  char *PATH = {0};
  int free_path = 0;
  char *curr_path;
  int max_length;

  char *directory;
  char *fullname = {0};

  int found_it;
  int found_count = 0;

  int i;

  if (!cmd) {
    return 0;
  }

  if (lim <= 0) {
    return 0;
  }

  if (!argv) {
    bu_bomb("bu_which was given a null array?\n");
  }

  if (bu_file_exists(cmd)) {
    /* add the finding */
    argv[found_count] = bu_malloc(strlen(cmd) * sizeof(char) + 1, "bu_which argv entry");
    strncpy(argv[found_count], cmd, strlen(cmd) + 1);
    found_count++;
  }

  /* use getenv() if it is available to get the PATH */
#ifdef HAVE_GETENV
#  define bu_which_found_path 1
  PATH = getenv("PATH");
  goto found_path;
#endif  /* HAVE_GETENV */

  /* otherwise use sysctl() to get the PATH */
#if defined(HAVE_SYSCTL) && defined(CTL_USER) && defined(USER_CS_PATH)
#  define bu_which_found_path 1
 {
   int mib[2];
   size_t len;
   mib[0] = CTL_USER;
   mib[1] = USER_CS_PATH;

   if (sysctl(mib, 2, NULL, &len, NULL, 0) != 0) {
     perror("sysctl unable to read user.cs_path");
     return 0;
   }
   if (len > 0) {
     PATH = bu_calloc(len, sizeof(char), "bu_which PATH");
     free_path = 1;
   }
   if (sysctl(mib, 2, PATH, &len, NULL, 0) != 0) {
     perror("sysctl unable to get user.cs_path");
     return 0;
   }
   goto found_path;
 }
#endif  /* HAVE_SYSCTL */

 /* sanity check, make sure we have _some_ means to get a PATH */
#ifndef bu_which_found_path
#  error "Do not know how to read the PATH environment variable on this system"
#endif

 found_path:

  if (!PATH) {
    /* no path, no match */
    bu_log("Unable to read the environment PATH\n");
    return 0;
  }

  /* something big enough to hold any path */
  max_length = strlen(PATH) + strlen(cmd) + 1;
  fullname = (char *)bu_calloc(max_length+1, sizeof(char), "bu_which fullname");

  /* search the PATH for the executable */
  for (curr_path = PATH; ; *curr_path++ = BU_PATH_SEPARATOR) {
    directory = curr_path;

    if ((curr_path = strchr(curr_path, BU_PATH_SEPARATOR)) != NULL) {
      *curr_path = '\0';

      /* equal means empty, so use current directory */
      if (directory == curr_path) {
	directory = ".";
      }
    } else {
      /* did not find a path separator, so this is the last element in the list */
      if (strlen(directory) == 0) {
	directory = ".";
      }
    }

    (void)snprintf(fullname, max_length, "%s/%s", directory, cmd);

    if (bu_file_exists(fullname)) {
      found_it = 0;

      /* make sure it is a new unique path */
      for (i = 0; i < found_count; i++) {
	if (strncmp(argv[i], fullname, max_length) == 0) {
	  found_it = 1;
	  break;
	}
      }

      /* add the finding if not previously added result */
      if (!found_it) {
	argv[found_count] = bu_malloc(max_length, "bu_which argv entry");
	strncpy(argv[found_count], fullname, max_length);
	found_count++;
      }
    }

    if (!curr_path) {
      break;
    }
    if (found_count >= lim) {
      break;
    }
  } /* end loop over PATH directories */

  bu_free(fullname, "bu_which fullname");
  fullname = NULL;

  /* free up the temporary resources */
#ifdef HAVE_SYSCTL
  if (free_path && PATH) {
    bu_free(PATH, "bu_which PATH");
    PATH = NULL;
  }
#endif

  return found_count;
}
