/*                    F O P E N _ U N I Q . C
 * BRL-CAD
 *
 * Copyright (C) 2001-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup libbu */
/*@{*/
/** @file fopen_uniq.c
 *  Routine to open a unique filename.
 *
 *  Authors -
 *	Lee A. Butler
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
/*@}*/

#include "common.h"



#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <string.h>	/* for strerror() */

#ifndef _WIN32
#  ifdef BSD
#    undef BSD
#    include <sys/param.h>
#  else
#    include <sys/param.h>
#  endif
#endif

#include <ctype.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"

/*  B U _ F O P E N _ U N I Q
 *
 *  Open a file for output.  Assures that the file did not previously exist.
 *
 *  Typical Usages:
 *	static int n = 0;
 *	FILE *fp;
 *
 *	fp = bu_fopen_uniq("writing to %s for results", "output%d.pl", n++);
 *	...
 *	fclose(fp);
 *
 *
 *	fp = bu_fopen_uniq((char *)NULL, "output%d.pl", n++);
 *	...
 *	fclose(fp);
 */
FILE *
bu_fopen_uniq(const char *outfmt, const char *namefmt, int n)
{
    char filename[MAXPATHLEN];
    int fd;
    FILE *fp;

    if ( ! namefmt || ! *namefmt)
	bu_bomb("bu_uniq_file called with null string\n");

    bu_semaphore_acquire( BU_SEM_SYSCALL);
    sprintf(filename, namefmt, n);
    if ((fd = open(filename, O_RDWR|O_CREAT|O_EXCL, 0600)) < 0) {
	fprintf(stderr, "Cannot open %s, %s\n", filename, strerror(errno));
	exit(-1);
    }
    if ( (fp=fdopen(fd, "w")) == (FILE *)NULL) {
	fprintf(stderr, strerror(errno));
	exit(-1);
    }

    if (outfmt)
	fprintf(stderr, outfmt, filename);

    bu_semaphore_release( BU_SEM_SYSCALL);

    return fp;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
