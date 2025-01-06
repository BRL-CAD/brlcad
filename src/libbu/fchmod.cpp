/*                      F C H M O D . C P P
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

#include <filesystem>
#include <string>
#include <system_error>

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
#include "bu/log.h"
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
    /* Presumably Windows - we need to know the file name.
     * https://stackoverflow.com/q/31439011 */
    TCHAR rawfilepath[MAXPATHLEN+1];
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    DWORD pret = GetFinalPathNameByHandle(h, rawfilepath, MAXPATHLEN, VOLUME_NAME_NONE);
    if (!(pret < MAXPATHLEN)) {
	bu_log("Error:  GetFinalPathNameByHandle unable to retrieve full path: %s\n", rawfilepath);
	return -1;
    }

    TCHAR filepath[MAXPATHLEN+1];
    GetFullPathNameA(rawfilepath, MAXPATHLEN, filepath, NULL);
    if (!bu_file_exists(filepath, NULL)) {
	bu_log("Error: GetFinalPathNameByHandle + GetFullPathNameA returned path \"%s\" but it does not exist\n", filepath);
	return -1;
    }

    // Use the C++ filesystem API for this
    std::filesystem::perms perms_mode = static_cast<std::filesystem::perms>(pmode);
    std::error_code ec;
    std::error_condition ok;
    std::filesystem::permissions(filepath, perms_mode, std::filesystem::perm_options::replace, ec);
    if (ec != ok) {
	std::string ec_name = ec.category().name();
	std::string ec_msg = ec.message();
	bu_log("Error setting permissions.  %s  %s\n", ec_name.c_str(), ec_msg.c_str());
	return -1;
    }
    return 0;
#endif
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
