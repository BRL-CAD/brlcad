/*                    F I L E S - T A P E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file files-tape.c
 *
 * Take a collection of files, and write them to tape.
 * Each file is padded out to an integral number of tape records,
 * and is written with a fixed block size (default is 24k).
 * This program is preferred over dd(1) in several small but
 * important ways:
 *
 * 1) Multiple files may be written to tape in a single operation,
 *	ie, with a single open() of the output device.
 * 2) Files are padded to full length records.
 * 3) Input files are read using bu_mread(), so operation will
 *	proceed even with pipe input (where DD can read and write
 *	short records on a random basis).
 *
 * This program is probably most often useful to
 * take a collection of pix(5) or bw(5) files, and write them to
 * tape using 24k byte records.
 *
 * There is no requirement that the different files be the same size,
 * although it is unlikely to be useful.  This could be a potential
 * source of problems if some files have not been finished yet.
 *
 * At 6250, one reel of tape holds roughly 6144 records of 24k bytes
 * each.  This program will warn when that threshold is reached,
 * but will take no action.
 *
 * UNIX system calls are used, not to foil portability, but in the
 * name of efficiency.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


#define TSIZE (6144*24*1024)	/* # of bytes on 2400' 6250bpi reel */
long byteswritten = 0;	/* Number of bytes written */

int bufsize = 24*1024;	/* Default buffer size */
char *buf;

void fileout(int fd, char *name);


static char usage[] = "\
Usage: files-tape [-b bytes] [-k Kbytes] [files]\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "b:k:")) != EOF) {
	switch (c) {
	    case 'b':
		bufsize = atoi(bu_optarg);	/* bytes */
		break;
	    case 'k':
		bufsize = atoi(bu_optarg) * 1024; /* Kbytes */
		break;

	    default:		/* '?' */
		return(0);	/* BAD */
	}
    }

    if (isatty(fileno(stdout)))
	return(0);		/* BAD */

    return(1);			/* OK */
}


/*
 * M A I N
 */
int
main(int argc, char **argv)
{
    int fd;

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    /* Obtain output buffer */
    if ((buf = (char *)malloc(bufsize)) == NULL) {
	perror("malloc");
	bu_exit (1, NULL);
    }

    if (bu_optind >= argc) {
	/* Perform operation once, from stdin */
	fileout(0, "-");
	bu_exit (0, NULL);
    }

    /* Perform operation on each argument */
    for (; bu_optind < argc; bu_optind++) {
	if ((fd = open(argv[bu_optind], 0)) < 0) {
	    perror(argv[bu_optind]);
	    /*
	     * It is unclear whether an exiting,
	     * or continuing with the next file
	     * is really the right thing here.
	     * If the intended size was known,
	     * a null "file" could be written to tape,
	     * to preserve the image numbering.
	     * For now, punt.
	     */
	    bu_exit (1, NULL);
	}
	fileout(fd, argv[bu_optind]);
	(void)close(fd);
    }
    bu_exit (0, NULL);
}


/*
 * F I L E O U T
 */
void
fileout(int fd, char *name)
{
    int count, out;

    while ((count = bu_mread(fd, buf, bufsize)) > 0) {
	if (count < bufsize) {
	    /* Short read, zero rest of buffer */
	    memset(buf+count, 0, bufsize-count);
	}
	if ((out = write(1, buf, bufsize)) != bufsize) {
	    perror("files-tape: write");
	    fprintf(stderr, "files-tape:  %s, write ret=%d\n", name, out);
	    bu_exit (1, NULL);
	}
	if (byteswritten < TSIZE && byteswritten+bufsize > TSIZE)
	    fprintf(stderr, "files-tape: WARNING:  Tape capacity reached in file %s\n", name);
	byteswritten += bufsize;
    }
    if (count == 0)
	return;

    perror("READ ERROR");

    bu_exit (1, NULL);
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
