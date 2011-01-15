/*                    F O P E N _ U N I Q . C
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

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>	/* for strerror() */
#include <ctype.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#include "bio.h"
#include "bu.h"


/* c99 doesn't declare these */
#if !defined(_WIN32) || defined(__CYGWIN__)
extern FILE *fdopen(int, const char *);
#endif


FILE *
bu_fopen_uniq(const char *outfmt, const char *namefmt, int n)
{
    char filename[MAXPATHLEN];
    int fd;
    FILE *fp;

    if (UNLIKELY(!namefmt || !*namefmt))
	bu_bomb("bu_uniq_file called with null string\n");

    bu_semaphore_acquire(BU_SEM_SYSCALL);

    printf("DEVELOPER DEPRECATION NOTICE: bu_fopen_uniq is deprecated, use fopen instead\n");

    /* NOTE: can't call bu_log because of the semaphore */

    snprintf(filename, MAXPATHLEN, namefmt, n);
    fd = open(filename, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (UNLIKELY(fd < 0)) {
	fprintf(stderr, "Cannot open %s, %s\n", filename, strerror(errno));
	return NULL;
    }
    fp=fdopen(fd, "w");
    if (UNLIKELY(fp == (FILE *)NULL)) {
	fprintf(stderr, "%s", strerror(errno));
    }

    if (LIKELY(outfmt != NULL))
	fprintf(stderr, outfmt, filename);

    bu_semaphore_release(BU_SEM_SYSCALL);

    return fp;
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
