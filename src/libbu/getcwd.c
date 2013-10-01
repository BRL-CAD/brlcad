/*                        G E T C W D . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2013 United States Government as represented by
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
/** @file libbu/getcwd.c
 *
 * Routine(s) for getting the current working directory full pathname.
 *
 */

#include "common.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_SYS_PARAM_H /* for MAXPATHLEN */
#  include <sys/param.h>
#endif
#include <string.h>

#include "bu.h"


char *
bu_getcwd(char *buf, size_t size)
{
    char *cwd = NULL;
    char *pwd = NULL;
    char cbuf[MAXPATHLEN] = {0};
    size_t sz = size;

    /* NULL buf means allocate */
    if (!buf) {
	sz = MAXPATHLEN;
	buf = bu_calloc(1, sz, "alloc bu_getcwd");
    }

    /* FIRST: try getcwd */
#ifdef HAVE_GETCWD
    cwd = getcwd(cbuf, MAXPATHLEN);
    if (cwd
	&& strlen(cwd) > 0
	&& bu_file_exists(cwd, NULL))
    {
#if defined(HAVE_WORKING_REALPATH_FUNCTION)
	/* FIXME: shouldn't have gotten here with -std=c99 (HAVE_REALPATH test not working right?) */
	char rbuf[MAXPATHLEN] = {0};
	char *rwd = bu_realpath(cbuf, rbuf);
	if (rwd
	    && strlen(rwd) > 0
	    && bu_file_exists(rwd, NULL))
	{
	    BU_ASSERT(sz > strlen(rwd)+1);
	    bu_strlcpy(buf, rwd, strlen(rwd)+1);
	    return buf;
	}
#endif /* HAVE_WORKING_REALPATH_FUNCTION */
	BU_ASSERT(sz > strlen(cwd)+1);
	bu_strlcpy(buf, cwd, strlen(cwd)+1);
	return buf;
    }
#else
    /* quellage */
    cwd = memset(cbuf, 0, MAXPATHLEN);
#endif /* HAVE_GETCWD */


    /* SECOND: try environment */
    pwd = getenv("PWD");
    if (pwd
	&& strlen(pwd) > 0
	&& bu_file_exists(pwd, NULL))
    {
#if defined(HAVE_WORKING_REALPATH_FUNCTION)
	char rbuf[MAXPATHLEN] = {0};
	char *rwd = realpath(pwd, rbuf);
	if (rwd
	    && strlen(rwd) > 0
	    && bu_file_exists(rwd, NULL))
	{
	    BU_ASSERT(sz > strlen(rwd)+1);
	    bu_strlcpy(buf, rwd, strlen(rwd)+1);
	    return buf;
	}
#endif /* HAVE_WORKING_REALPATH_FUNCTION */
	BU_ASSERT(sz > strlen(pwd)+1);
	bu_strlcpy(buf, pwd, strlen(pwd)+1);
	return buf;
    }

    /* LAST: punt (but do not return NULL) */
    BU_ASSERT(sz > strlen(".")+1);
    bu_strlcpy(buf, ".", strlen(".")+1);
    return buf;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
