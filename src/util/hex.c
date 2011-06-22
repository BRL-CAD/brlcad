/*                            H E X . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file util/hex.c
 *
 * Give a good ole' CPM style Hex dump of a file or standard input.
 *
 * Options
 * h    help
 * o    offset from begining of data from which to start dump
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"

#include "bu.h"


/* declarations to support use of bu_getopt() system call */
static char *options = "o:";
static char *progname = "(noname)";

static long offset=0;	 /* offset from begining of file from which to start */

#define DUMPLEN 16    /* number of bytes to dump on one line */

/*
 * D U M P --- Dump file in hex
 */
void dump(FILE *fd)
{
    int i;
    char *p;
    int bytes;
    long addr = 0L;
    static char buf[DUMPLEN];    /* input buffer */

    if (offset != 0) {
  	/* skip over "offset" bytes first */
	if (fseek(fd, offset, 0)) {

	    /* If fseek fails, try reading our way to the desired offset.
	     * The fseek will fail if we're reading from a pipe.
	     */

	    addr=0;
	    while (addr < offset) {
		if ((i=fread(buf, 1, sizeof(buf), fd)) == 0) {
		    fprintf(stderr, "%s: offset exceeds end of input!\n", progname);
		    bu_exit (-1, NULL);
		} else addr += i;
	    }
	} else addr = offset;
    }

    /* dump address, Hex of buffer and ASCII of buffer */
    while ((bytes=fread(buf, 1, sizeof(buf), fd)) > 0) {

	/* print the offset into the file */
	printf("%08lx", addr);

	/* produce the hexadecimal dump */
	for (i=0, p=buf; i < DUMPLEN; ++i) {
	    if (i < bytes) {
		if (i%4 == 0) printf("  %02x", *p++ & 0x0ff);
		else printf(" %02x", *p++ & 0x0ff);
	    } else {
		if (i%4 == 0) printf("    ");
		else printf("   ");
	    }
	}

	/* produce the ASCII dump */
	printf(" |");
	for (i=0, p=buf; i < bytes; ++i, ++p) {
	    if (isascii(*p) && isprint(*p)) putchar(*p);
	    else putchar('.');
	}
	printf("|\n");
	addr += DUMPLEN;
    }
}


/*
 * U S A G E --- Print helpful message and bail out
 */
void usage(void)
{
    (void) fprintf(stderr, "Usage: %s [-o offset] [file...]\n", progname);
    bu_exit (1, NULL);
}


/* M A I N
 *
 * Parse arguemnts and call 'dump' to perform primary task.
 */
int
main(int ac, char **av)
{
    int c, optlen, files;
    FILE *fd;
    char *eos;
    long newoffset;

    progname = *av;

    /* Get # of options & turn all the option flags off */
    optlen = strlen(options);

    for (c=0; c < optlen; c++)  /* NIL */;

    /* Turn off bu_getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1)
	if (c == 'o') {
	    newoffset = strtol(bu_optarg, &eos, 0);

	    if (eos != bu_optarg)
		offset = newoffset;
	    else
		fprintf(stderr, "%s: error parsing offset \"%s\"\n",
			progname, bu_optarg);
	} else usage();

    if (offset%DUMPLEN != 0) offset -= offset % DUMPLEN;

    if (bu_optind >= ac) {
	/* no file left, try processing stdin */
	if (isatty(fileno(stdin))) usage();
	else dump(stdin);
    } else {
	/* process each remaining arguments */
	for (files = ac-bu_optind; bu_optind < ac; bu_optind++) {
	    if ((fd=fopen(av[bu_optind], "r")) == (FILE *)NULL) {
		perror(av[bu_optind]);
		bu_exit (-1, NULL);
	    }
	    if (files > 1) printf("/**** %s ****/\n", av[bu_optind]);
	    dump(fd);
	    (void)fclose(fd);
	}
    }
    return 0;
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
