/*                         P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#include "bu/path.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"


char *
bu_path_dirname(const char *cp)
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
	bu_free(ret, "bu_path_dirname");
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


char *
bu_path_basename(const char *path, char *basename)
{
    const char *p;
    char *base;
    size_t len;

    if (UNLIKELY(!path)) {
	if (basename) {
	    bu_strlcpy(basename, ".", strlen(".")+1);
	    return basename;
	}
	return bu_strdup(".");
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

    if (basename) {
	if (len > 0) {
	    bu_strlcpy(basename, path, len+1);
	} else {
	    basename[0] = '.';
	}
	return basename;
    }

    /* Create a new string */
    base = (char *)bu_calloc(len + 2, sizeof(char), "bu_path_basename alloc");
    if (len > 0) {
	bu_strlcpy(base, path, len+1);
    } else {
	base[0] = '.';
    }
    return base;
}


int
bu_path_component(struct bu_vls *component, const char *path, bu_path_component_t type)
{
    int ret = 0;
    char *basename = NULL;
    char *dirname = NULL;
    const char *period_pos = NULL;

    /* nothing to match is a match */
    if (UNLIKELY(!path) || UNLIKELY(strlen(path)) == 0)
	return 0;

    switch (type) {
	case BU_PATH_DIRNAME:
	    dirname = bu_path_dirname(path);
	    if (!(!dirname || strlen(dirname) == 0)) {
		ret = 1;
		if (component) {
		    bu_vls_sprintf(component, "%s", dirname);
		}
	    }
	    break;
	case BU_PATH_EXTLESS:
	    ret = 1;
	    if (component) {
		period_pos = strrchr(path, '.');
		bu_vls_sprintf(component, "%s", path);
		if (period_pos && strlen(period_pos) > 0)
		    bu_vls_trunc(component, -1 * (int)strlen(period_pos));
	    }
	    break;
	case BU_PATH_BASENAME:
	    basename = (char *)bu_calloc(strlen(path) + 2, sizeof(char), "basename");
	    bu_path_basename(path, basename);
	    if (strlen(basename) > 0) {
		ret = 1;
		if (component) {
		    bu_vls_sprintf(component, "%s", basename);
		}
	    }
	    break;
	case BU_PATH_BASENAME_EXTLESS:
	    basename = (char *)bu_calloc(strlen(path) + 2, sizeof(char), "basename");
	    bu_path_basename(path, basename);
	    if (strlen(basename) > 0) {
		ret = 1;
		if (component) {
		    period_pos = strrchr(basename, '.');
		    bu_vls_sprintf(component, "%s", basename);
		    if (period_pos) {
			bu_vls_trunc(component, -1 * (int)strlen(period_pos));
		    }
		}
	    }
	    break;
	case BU_PATH_EXT:
	    basename = (char *)bu_calloc(strlen(path) + 2, sizeof(char), "basename");
	    bu_path_basename(path, basename);
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
	    break;
	default:
	    break;
    }

    bu_free(basename, "basename");
    bu_free(dirname, "dirname");

    return ret;
}


char **
bu_path_to_argv(const char *path, int *ac)
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
	bu_free((void *)newstr, "bu_path_to_argv");
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
    av = (char **)bu_calloc((unsigned int)(*ac)+1, sizeof(char *), "bu_path_to_argv");

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
    bu_free((void *)newstr, "bu_path_to_argv");

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
