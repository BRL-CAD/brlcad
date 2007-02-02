/*                       D S P _ A D D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file dsp_add.c
 *
 */

/*	D S P _ A D D . C --- add 2 files of network unsigned shorts
 *
 *	Options
 *	h	help
 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"


/* declarations to support use of bu_getopt() system call */
char *options = "h";
extern char *bu_optarg;
extern int bu_optind, bu_opterr;

/* , bu_getopt(int, char *const *, const char *);*/

char *progname = "(noname)";

#define ADD_STYLE_INT 0
#define ADD_STYLE_FLOAT 1
int style = ADD_STYLE_INT;

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(char *s)
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr, "Usage: %s [ -%s ] dsp_1 dsp_2 > dsp_3\n",
			progname, options);
	exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(int ac, char *av[])
{
	int  c;
	char *strrchr(const char *, int);

	if (  ! (progname=strrchr(*av, '/'))  )
		progname = *av;
	else
		++progname;

	/* Turn off bu_getopt's error messages */
	bu_opterr = 0;

	/* get all the option flags from the command line */
	while ((c=bu_getopt(ac,av,options)) != EOF)
		switch (c) {
		case '?'	:
		case 'h'	:
		default		: usage("Bad or help flag specified\n"); break;
		}

	return(bu_optind);
}


void
swap_bytes(unsigned short *buf, unsigned long count)
{
	register unsigned short *p;

	for (p = &buf[count-1] ; p >= buf ; p--)
		*p = ((*p << 8) & 0x0ff00) | (*p >> 8);
}


/*
 *  A D D _ F L O A T
 *
 *  Perform floating point addition and re-normalization of the data.
 *
 */
void
add_float(unsigned short *buf1, unsigned short *buf2, unsigned long count)
{
	register unsigned short *p, *q, *e;
	register double *dbuf, *d;
	double min, max, k;

	dbuf = bu_malloc(sizeof(double) * count, "buffer of double");

	min = MAX_FASTF;
	max = -MAX_FASTF;
	e = &buf1[count];

	/* add everything, keeping track of the min/max values found */
	for (d=dbuf, p=buf1, q=buf2 ; p < e ; p++, q++, d++) {
		*d = *p + *q;
		if (*d > max) max = *d;
		if (*d < min) min = *d;
	}

	/* now we convert back to unsigned shorts in the range 1 .. 65535 */

	k = 65534.0 / (max - min);

	bu_log("min: %g scale: %g\n", min - k, k);

	for (d=dbuf, p=buf1, q=buf2 ; p < e ; p++, q++, d++)
		*p = (unsigned short)  ((*d - min) * k) + 1;

	bu_free(dbuf, "buffer of double");
}

/*
 *  A D D _ I N T
 *
 *  Perform simple integer addition to the input streams.
 *  Issue warning on overflow.
 *
 *  Result:	buf1 contents modified
 */
void
add_int(unsigned short *buf1, unsigned short *buf2, unsigned long count)
{
	register int int_value;
	int i;
	unsigned short s;

	for (i=0; i < count ; i++) {
		int_value = buf1[i] + buf2[i];
		s = (unsigned short)int_value;

		if (s != int_value) {
			bu_log("overflow (%d+%d) == %d at %d\n",
			       buf1[i], buf2[i], int_value, i );
		}
		buf1[i] = s;
	}

}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int
main(int ac, char *av[])
{
	int next_arg;
	FILE *in1, *in2;
	unsigned short *buf1, *buf2;
	unsigned long count;
	int in_cookie, out_cookie;
	int conv;
	struct stat sb;

	next_arg = parse_args(ac, av);

	if (isatty(fileno(stdout))) usage("Redirect standard output\n");

	if (next_arg >= ac) usage("No files specified\n");


	/* Open the files */

	if (stat(av[next_arg], &sb) ||
	    (in1 = fopen(av[next_arg], "r"))  == (FILE *)NULL) {
		perror(av[next_arg]);
		return -1;
	}

	count = (unsigned long)sb.st_size;
	buf1 = bu_malloc((size_t)sb.st_size, "buf1");

	next_arg++;

	if (stat(av[next_arg], &sb) ||
	    (in2 = fopen(av[next_arg], "r"))  == (FILE *)NULL) {
		perror(av[next_arg]);
		return -1;
	}

	if (sb.st_size != count)
		bu_bomb("**** ERROR **** file size mis-match\n");

	buf2 = bu_malloc((size_t)sb.st_size, "buf2");

	count = count >> 1; /* convert count of char to count of short */

	/* Read the terrain data */
	fread(buf1, sizeof(short), count, in1);
	fclose(in1);

	fread(buf2, sizeof(short), count, in2);
	fclose(in2);


	/* Convert from network to host format */
	in_cookie = bu_cv_cookie("nus");
	out_cookie = bu_cv_cookie("hus");
	conv = (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie));

	if (conv) {
		swap_bytes(buf1, count);
		swap_bytes(buf2, count);
	}


	/* add the two datasets together */

	switch (style) {
	case ADD_STYLE_FLOAT	: add_float(buf1, buf2, count); break;
	case ADD_STYLE_INT	: add_int(buf1, buf2, count); break;
	default			: fprintf(stderr,
					"Error: Unknown add style\n");
				break;
	}


	/* convert back to network format & write out */
	if (conv) {
		swap_bytes(buf1, count);
		swap_bytes(buf2, count);
	}

	if (fwrite(buf1, sizeof(short), count, stdout) != count) {
		fprintf(stderr, "Error writing data\n");
		return -1;
	}

	return 0;
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
