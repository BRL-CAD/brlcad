/*
 *			F O P E N _ U N I Q . C
 *
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
 *  Copyright Notice -
 *	This software is Copyright (C) 2001-2004 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#ifndef WIN32
#ifdef BSD
#undef BSD
#include <sys/param.h>
#else
#include <sys/param.h>
#endif
#endif

#include <ctype.h>
#include <math.h>
#ifdef USE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
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
