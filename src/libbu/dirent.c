/*                           D I R E N T . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2011 United States Government as represented by
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "bu.h"
#include "./uce-dirent.h"


int
bu_count_path(char *path, char *substr)
{
    int filecount = 0;
    DIR *dir = opendir(path);
    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
	if (strlen(substr) == 0) {
	    filecount++;
	} else {
	    if (BU_STR_EQUAL(dp->d_name+(strlen(dp->d_name)-strlen(substr)), substr)) {
		filecount++;
	    }
	}
    }
    closedir(dir);
    return filecount;
}

void
bu_list_path(char *path, char *substr, char **filearray)
{
    int filecount = -1;
    DIR *dir = opendir(path);
    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
	if (strlen(substr) == 0) {
	    filecount++;
	    filearray[filecount]=dp->d_name;
	} else {
	    if (BU_STR_EQUAL(dp->d_name+(strlen(dp->d_name)-strlen(substr)), substr)) {
		filecount++;
		filearray[filecount]=dp->d_name;
	    }
	}
    }
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
