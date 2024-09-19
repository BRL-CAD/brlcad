/*                        F C H M O D . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2024 United States Government as represented by
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

#include "common.h"

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "bio.h"

#ifndef HAVE_FCHMOD
/* headers for the GetFileNameFromHandle() function below */
#  include <tchar.h>
#  include <string.h>
#  include <psapi.h>
#  ifdef HAVE_STRSAFE_H
#    include <strsafe.h>
#  endif
#endif

#include "bu/app.h"
#include "bu/file.h"
#include "bu/str.h"

#ifdef HAVE_FCHMOD
/* extern int fchmod(int, mode_t); */
/* use correct fchmod decl: */
#  include <sys/stat.h>

/* c89 strict doesn't declare fchmod */
# ifndef HAVE_DECL_FCHMOD
extern int fchmod(int, mode_t);
# endif
#endif


int
bu_fchmod(int fd,
	  unsigned long pmode)
{
#ifdef HAVE_FCHMOD
    return fchmod(fd, (mode_t)pmode);
#else
    /* Presumably Windows, so get dirty.  We can call chmod() instead
     * of fchmod(), but that means we need to know the file name.
     *
     * https://stackoverflow.com/q/31439011
     */
    {
	TCHAR rawfilepath[MAXPATHLEN+1];
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	DWORD pret = GetFinalPathNameByHandle(h, rawfilepath, MAXPATHLEN, VOLUME_NAME_NONE);
	if (!(pret < MAXPATHLEN))
	    bu_log("Warning:  GetFinalPathNameByHandle unable to retrieve full path: %s\n", rawfilepath);

	TCHAR filepath[MAXPATHLEN+1];
	GetFullPathNameA(rawfilepath, MAXPATHLEN, filepath, NULL);
	//bu_log("GetFinalPathNameByHandle + GetFullPathNameA: %s\n", filepath);

	/* quell flawfinder because this is a necessary evil unless/until
	 * someone rewrites this to use SetNamedSecurityInfo() based on
	 * unix-style permissions/mode settings.
	 */
#  define CHMOD ch ## mod

	return CHMOD(filepath, pmode);
    }
#endif
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
