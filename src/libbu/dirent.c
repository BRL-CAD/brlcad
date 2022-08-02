/*                         D I R E N T . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2022 United States Government as represented by
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

#include "bu/file.h"
#include "bu/path.h"
#include "bu/malloc.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "uce-dirent.h"


static int
cmpdir(const void *a, const void *b, void *UNUSED(context))
{
    return (bu_strcmp(*(const char **)a, *(const char **)b));
}


size_t
bu_file_list(const char *path, const char *pattern, char ***files)
{
    size_t i = 0;
    size_t filecount = 0;
    DIR *dir = NULL;
    struct dirent *dp = NULL;

    /* calculate file count */
    dir = opendir(path);
    while (dir && (dp = readdir(dir)) != NULL) {
	if (!pattern
	    || (strlen(pattern) == 0)
	    || (bu_path_match(pattern, dp->d_name, 0) == 0))
	{
	    filecount++;
	}
    }
    if (dir)
	(void)closedir(dir);

    /* bail now if there's no files array pointer to fill in */
    if (!files) {
	return filecount;
    }

    /* allocate enough space plus room for a null entry too */
    *files = (char **)bu_calloc(filecount+1, sizeof(char *), "files alloc");

    dir = opendir(path);
    while (dir && (dp = readdir(dir)) != NULL) {
	if (!pattern
	    || (strlen(pattern) == 0)
	    || (bu_path_match(pattern, dp->d_name, 0) == 0))
	{
	    (*files)[i++] = bu_strdup(dp->d_name);
	}
    }
    if (dir)
	(void)closedir(dir);

    bu_sort(*files, filecount, sizeof(char *), cmpdir, NULL);

    return filecount;
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
