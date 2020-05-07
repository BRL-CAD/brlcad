/*                         A P P . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2020 United States Government as represented by
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

#include <stdio.h>
#include <string.h>

#include "bu.h"

int
main(int ac, char *av[])
{
    if (ac != 1)
	bu_exit(1, "Usage: %s\n", av[0]);

    bu_setprogname(av[0]);

    const char *pname = bu_argv0_full_path();

    if (pname[0] == '.') {
	char iwd[MAXPATHLEN];
	char fullpath[MAXPATHLEN];
	struct bu_vls apath = BU_VLS_INIT_ZERO;
	bu_getiwd(iwd, MAXPATHLEN);
	bu_vls_sprintf(&apath, "%s%c%s", iwd, BU_DIR_SEPARATOR, pname);
	if (!bu_file_realpath(bu_vls_cstr(&apath), fullpath)) {
	    bu_log("Unable to resolve local path: %s\n", bu_vls_cstr(&apath));
	    return -1;
	}
	bu_vls_free(&apath);
	if (!bu_file_exists(fullpath, NULL)) {
	    bu_log("Resolved to path %s, which does not exist\n", fullpath);
	    return -1;
	}
	bu_log("%s -> %s\n", pname, fullpath);
    } else {
	if (!bu_file_exists(pname, NULL)) {
	    bu_log("Invalid av[0] entry: %s\n", pname);
	    return -1;
	}
	bu_log("%s -> %s\n", pname, pname);
    }

    return 0;
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
