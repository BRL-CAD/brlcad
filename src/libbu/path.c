/*                         P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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


char *
bu_basename(const char *path, char *basename)
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
    base = (char *)bu_calloc(len + 2, sizeof(char), "bu_basename alloc");
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
    char *period_pos = NULL;

    /* nothing to match is a match */
    if (UNLIKELY(!path) || UNLIKELY(strlen(path)) == 0)
	return 0;

    switch (type) {
	case BU_PATH_DIRNAME:
	    dirname = bu_dirname(path);
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
		    bu_vls_trunc(component, -1 * strlen(period_pos));
	    }
	    break;
	case BU_PATH_BASENAME:
	    basename = (char *)bu_calloc(strlen(path) + 2, sizeof(char), "basename");
	    bu_basename(path, basename);
	    if (strlen(basename) > 0) {
		ret = 1;
		if (component) {
		    bu_vls_sprintf(component, "%s", basename);
		}
	    }
	    break;
	case BU_PATH_BASENAME_EXTLESS:
	    basename = (char *)bu_calloc(strlen(path) + 2, sizeof(char), "basename");
	    bu_basename(path, basename);
	    if (strlen(basename) > 0) {
		ret = 1;
		if (component) {
		period_pos = strrchr(basename, '.');
		bu_vls_sprintf(component, "%s", basename);
		    if (period_pos) {
		bu_vls_trunc(component, -1 * strlen(period_pos));
	    }
		}
	    }
	    break;
	case BU_PATH_EXT:
	    basename = (char *)bu_calloc(strlen(path) + 2, sizeof(char), "basename");
	    bu_basename(path, basename);
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

    if (basename)
	bu_free(basename, "basename");
    if (dirname)
	bu_free(dirname, "dirname");

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

/* Most of bu_normalize is a subset of NetBSD's realpath function:
 *
 * Copyright (c) 1989, 1991, 1993, 1995
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
const char *
bu_normalize(const char *path)
{
    static char resolved[MAXPATHLEN] = {0};
    const char *q;
    char *p;
    if (!path) return (NULL);

    /*
     * `p' is where we'll put a new component with prepending
     * a delimiter.
     */
    p = resolved;

    if (*path == 0) return (NULL);

loop:
    /* Skip any slash. */
    while (*path == '/')
        path++;

    if (*path == 0) {
        if (p == resolved)
            *p++ = '/';
        *p = 0;
        return (resolved);
    }

    /* Find the end of this component. */
    q = path;
    do
        q++;
    while (*q != '/' && *q != 0);

    /* Test . or .. */
    if (path[0] == '.') {
        if (q - path == 1) {
            path = q;
            goto loop;
        }
        if (path[1] == '.' && q - path == 2) {
            /* Trim the last component. */
            if (p != resolved)
                while (*--p != '/')
                    ;
            path = q;
            goto loop;
        }
    }

    /* Append this component. */
    if (p - resolved + 1 + q - path + 1 > MAXPATHLEN) {
        if (p == resolved)
            *p++ = '/';
        *p = 0;
        return (NULL);
    }
    p[0] = '/';
    memcpy(&p[1], path,
           /* LINTED We know q > path. */
           q - path);
    p[1 + q - path] = 0;

    /* Advance both resolved and unresolved path. */
    p += 1 + q - path;
    path = q;
    goto loop;
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
