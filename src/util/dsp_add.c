/*                       D S P _ A D D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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
/** @file util/dsp_add.c
 *
 * add 2 files of network unsigned shorts
 *
 * Options
 * h help
 */

#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"


/* declarations to support use of bu_getopt() system call */
static const char optstring[] = "h?";
static const char progname[] = "dsp_add";
static const char usage[] = "Usage: %s dsp_1 dsp_2 > dsp_3\n";

/* purpose: combine two dsp files
 *
 * description: Combines two dsp files (which are binary files
 * comprised of network unsigned shorts).  The two files must be of
 * identical size.  The result, written to stdout, is a file where
 * each cell's height is the total of the heights of the same cell
 * in the input files.
 *
 * See the BRL-CAD wiki for a tutorial on using dsp's.
 *
 * see_also: dsp(5) asc2dsp(1) cv(1)
 *
 * opt: -h brief help
 *
 * opt: -? brief help
 *
 */

#define ADD_STYLE_INT 0
#define ADD_STYLE_FLOAT 1

static int style = ADD_STYLE_INT;

/*
 * tell user how to invoke this program, then exit
 */
static void
print_usage(const char *s)
{
    if (s) (void)fputs(s, stderr);

    bu_log(usage, progname);
    bu_exit (1, NULL);
}


/*
 * Parse command line flags
 */
static int
parse_args(int ac, char *av[])
{
    int c;

    /* get all the option flags from the command line */
    while ((c = bu_getopt(ac, av, optstring)) != -1)
	switch (c) {
	    default:
		print_usage("");
	}

    return bu_optind;
}


static void
swap_bytes(unsigned short *buf, unsigned long count)
{
    unsigned short *p;

    for (p = &buf[count-1]; p >= buf; p--)
	*p = ((*p << 8) & 0x0ff00) | (*p >> 8);
}


/*
 * Perform floating point addition and re-normalization of the data.
 */
static void
add_float(unsigned short *buf1, unsigned short *buf2, unsigned long count)
{
    unsigned short *p, *q, *e;
    double *dbuf, *d;
    double min, max, k;

    dbuf = (double *)bu_malloc(sizeof(double) * count, "buffer of double");

    min = MAX_FASTF;
    max = -MAX_FASTF;
    e = &buf1[count];

    /* add everything, keeping track of the min/max values found */
    for (d = dbuf, p = buf1, q = buf2; p < e; p++, q++, d++) {
	*d = *p + *q;
	if (*d > max) max = *d;
	if (*d < min) min = *d;
    }

    /* now we convert back to unsigned shorts in the range 1 .. 65535 */

    k = 65534.0 / (max - min);

    bu_log("min: %g scale: %g\n", min - k, k);

    for (d = dbuf, p = buf1, q = buf2; p < e; p++, q++, d++)
	*p = (unsigned short)  ((*d - min) * k) + 1;

    bu_free(dbuf, "buffer of double");
}


/*
 * Perform simple integer addition to the input streams.
 * Issue warning on overflow.
 *
 * Result:	buf1 contents modified
 */
static void
add_int(unsigned short *buf1, unsigned short *buf2, unsigned long count)
{
    int int_value;
    unsigned long i;
    unsigned short s;

    for (i = 0; i < count; i++) {
	int_value = buf1[i] + buf2[i];
	s = (unsigned short)int_value;

	if (s != int_value) {
	    bu_log("overflow (%d+%d) == %d at %lu\n",
		   buf1[i], buf2[i], int_value, i);
	}
	buf1[i] = s;
    }

}


/*
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int
main(int ac, char *av[])
{
    int next_arg;
    FILE *in1, *in2;
    unsigned short *buf1, *buf2;
    size_t count;
    int in_cookie, out_cookie;
    int conv;
    struct stat sb;
    size_t ret;

    if (ac < 2)
	print_usage("");

    if (isatty(fileno(stdout)))
	print_usage("Must redirect standard output\n");

    next_arg = parse_args(ac, av);

    if (next_arg >= ac)
	print_usage("No files specified\n");

    /* Open the files */

    in1 = fopen(av[next_arg], "r");
    if (!in1) {
	perror(av[next_arg]);
	return -1;
    }

    if (fstat(fileno(in1), &sb)) {
	perror(av[next_arg]);
	fclose(in1);
	return -1;
    }

    count = sb.st_size;
    buf1 = (unsigned short *)bu_malloc((size_t)sb.st_size, "buf1");

    next_arg++;

    in2 = fopen(av[next_arg], "r");
    if (!in2) {
	perror(av[next_arg]);
	fclose(in1);
	return -1;
    }

    if (fstat(fileno(in2), &sb)) {
	perror(av[next_arg]);
	fclose(in1);
	fclose(in2);
	return -1;
    }

    if ((size_t)sb.st_size != count) {
	fclose(in1);
	fclose(in2);
	bu_exit(EXIT_FAILURE, "**** ERROR **** file size mis-match\n");
    }

    buf2 = (unsigned short *)bu_malloc((size_t)sb.st_size, "buf2");

    count = count >> 1; /* convert count of char to count of short */

    /* Read the terrain data */
    ret = fread(buf1, sizeof(short), count, in1);
    if (ret < count)
	perror("fread");
    fclose(in1);

    ret = fread(buf2, sizeof(short), count, in2);
    if (ret < count)
	perror("fread");
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
	default			: bu_log("Error: Unknown add style\n");
	    break;
    }

    /* convert back to network format & write out */
    if (conv) {
	swap_bytes(buf1, count);
	swap_bytes(buf2, count);
    }

    if (fwrite(buf1, sizeof(short), count, stdout) != count) {
	bu_log("Error writing data\n");
	return -1;
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
