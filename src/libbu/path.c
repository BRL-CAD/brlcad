/*                         P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

#include <string.h>
#include <ctype.h>

#include "bu/log.h"
#include "bu/path.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"


char *
bu_dirname(const char *cp)
{
    char *ret;
    char *found_dslash;
    char *found_fslash;
    size_t len;
    const char DSLASH[2] = {BU_DIR_SEPARATOR, '\0'};
    const char FSLASH[2] = {'/', '\0'};
    const char *DOT = ".";
    const char *DOTDOT = "..";

    /* Special cases */
    if (UNLIKELY(!cp))
        return bu_strdup(".");

    if (BU_STR_EQUAL(cp, DSLASH))
        return bu_strdup(DSLASH);
    if (BU_STR_EQUAL(cp, FSLASH))
        return bu_strdup(FSLASH);

    if (BU_STR_EQUAL(cp, DOT)
        || BU_STR_EQUAL(cp, DOTDOT)
        || (strrchr(cp, BU_DIR_SEPARATOR) == NULL
            && strrchr(cp, '/') == NULL))
        return bu_strdup(DOT);

    /* Make a duplicate copy of the string, and shorten it in place */
    ret = bu_strdup(cp);

    /* A sequence of trailing slashes don't count */
    len = strlen(ret);
    while (len > 1
           && (ret[len-1] == BU_DIR_SEPARATOR
               || ret[len-1] == '/')) {
        ret[len-1] = '\0';
        len--;
    }

    /* If no slashes remain, return "." */
    found_dslash = strrchr(ret, BU_DIR_SEPARATOR);
    found_fslash = strrchr(ret, '/');
    if (!found_dslash && !found_fslash) {
        bu_free(ret, "bu_dirname");
        return bu_strdup(DOT);
    }

    /* Remove trailing slash, unless it's at front */
    if (found_dslash == ret || found_fslash == ret) {
        ret[1] = '\0';          /* ret == BU_DIR_SEPARATOR || "/" */
    } else {
        if (found_dslash)
            *found_dslash = '\0';
        if (found_fslash)
            *found_fslash = '\0';
    }

    return ret;
}


void
bu_basename(char *basename, const char *path)
{
    const char *p;
    size_t len;

    if (UNLIKELY(!path)) {
        bu_strlcpy(basename, ".", strlen(".")+1);
        return;
    }

    /* skip the filesystem disk/drive name if we're on a DOS-capable
     * platform that uses '\' for paths, e.g., C:\ -> \
     */
    if (BU_DIR_SEPARATOR == '\\' && isalpha((int)(path[0])) && path[1] == ':') {
        path += 2;
    }

    /* Skip leading separators, e.g., ///foo/bar -> foo/bar */
    for (p = path; *p != '\0'; p++) {
        /* check native separator as well as / so we can use this
         * routine for geometry paths too.
         */
        if ((p[0] == BU_DIR_SEPARATOR && p[1] != BU_DIR_SEPARATOR && p[1] != '\0')
            || (p[0] == '/' && p[1] != '/' && p[1] != '\0')) {
            path = p+1;
        }
    }

    len = strlen(path);

    /* Remove trailing separators */
    while (len > 1 && (path[len - 1] == BU_DIR_SEPARATOR || path[len - 1] == '/'))
        len--;

    if (len > 0) {
        bu_strlcpy(basename, path, len+1);
    } else {
        basename[0] = '.';
    }
}


int
bu_path_component(struct bu_vls *component, const char *path, path_component_t type)
{
    int ret = 0;
    char *basename = NULL;
    char *basename_free = NULL;
    char *dirname = NULL;
    char *period_pos = NULL;
    struct bu_vls working_path = BU_VLS_INIT_ZERO;

    if (UNLIKELY(!path) || UNLIKELY(strlen(path)) == 0) goto cleanup;

    if (type != PATH_DIRECTORY) {
	/* get the base name */
	basename = (char *)bu_calloc(strlen(path) + 2, sizeof(char), "basename");
	bu_basename(basename, path);

	/* Make sure we can manipulate the results and
	 * still properly free memory */
	basename_free = basename;
    }

    switch (type) {
	case PATH_DIRECTORY:
	    /* get the directory */
	    dirname = bu_dirname(path);
	    if (!(!dirname || strlen(dirname) == 0)) {
		ret = 1;
		if (component) {
		    bu_vls_sprintf(component, "%s", dirname);
		}
	    }
	    break;
	case PATH_FILENAME:
	    bu_basename(basename, path);
	    if (strlen(basename) > 0) {
		ret = 1;
		if (component) {
		    bu_vls_sprintf(component, "%s", basename);
		}
	    }
	    break;
	case PATH_ROOT_FILENAME:
	    /* get the base name */
	    bu_basename(basename, path);
	    if (strlen(basename) > 0) {
		ret = 1;
		period_pos = strrchr(basename, '.');
		bu_vls_sprintf(component, "%s", basename);
		bu_vls_trunc(component, -1 * strlen(period_pos));
	    }
	    break;
	case PATH_FILE_EXTENSION:
	    /* get the base name */
	    bu_basename(basename, path);
	    if (strlen(basename) > 0) {
		period_pos = strrchr(basename, '.');
		if (period_pos && strlen(period_pos) > 1) {
		    ret = 1;
		    if (component) {
			bu_vls_strncpy(component, period_pos, strlen(period_pos)+1);
			bu_vls_nibble(component, 1);
		    }
		}
	    }
	default:
	    break;
    }

cleanup:
    if (basename_free) bu_free(basename_free, "basename");
    if (dirname) bu_free(dirname, "dirname");
    bu_vls_free(&working_path);
    return ret;
}



char **
bu_argv_from_path(const char *path, int *ac)
{
    char **av;
    char *begin;
    char *end;
    char *newstr;
    char *headpath;
    register int i;

    if (UNLIKELY(path == (char *)0 || path[0] == '\0'))
        return (char **)0;

    newstr = bu_strdup(path);

    /* skip leading /'s */
    i = 0;
    while (newstr[i] == '/')
        ++i;

    if (UNLIKELY(newstr[i] == '\0')) {
        bu_free((void *)newstr, "bu_argv_from_path");
        return (char **)0;
    }

    /* If we get here, there is at least one path element */
    *ac = 1;
    headpath = &newstr[i];

    /* First count the number of '/' */
    begin = headpath;
    while ((end = strchr(begin, '/')) != (char *)0) {
        if (begin != end)
            ++*ac;

        begin = end + 1;
    }
    av = (char **)bu_calloc((unsigned int)(*ac)+1, sizeof(char *), "bu_argv_from_path");

    begin = headpath;
    i = 0;
    while ((end = strchr(begin, '/')) != (char *)0) {
        if (begin != end) {
            *end = '\0';
            av[i++] = bu_strdup(begin);
        }

        begin = end + 1;
    }

    if (begin[0] != '\0') {
        av[i++] = bu_strdup(begin);
        av[i] = (char *)0;
    } else {
        av[i] = (char *)0;
        --*ac;
    }
    bu_free((void *)newstr, "bu_argv_from_path");

    return av;
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
