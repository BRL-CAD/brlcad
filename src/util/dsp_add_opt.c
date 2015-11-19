/*                       D S P _ A D D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

#include "vmath.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/opt.h"
#include "bu/cv.h"
#include "bn.h"



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
 * Handle command line arguments first, then process input.
 */
int
main(int ac, char *av[])
{
    FILE *in1, *in2;
    unsigned short *buf1, *buf2;
    size_t count;
    int in_cookie, out_cookie;
    int conv;
    struct stat sb;
    size_t ret;
    const char *f1, *f2;
    static int print_help = 0;
    int non_opt_argc = 0;

    static const char usage[] = "Usage: dsp_add [opts] dsp_1 dsp_2 > dsp_3\n";
    struct bu_opt_desc dsp_opt_desc[3] = {
	{"h", "help", "", NULL, (void *)&print_help, "Print help and exit"},
	{"?", "",     "", NULL, (void *)&print_help, ""},
	BU_OPT_DESC_NULL
    };

    if (ac < 2) print_help = 1;

    if (isatty(fileno(stdout))) {
	bu_log("Error: Must redirect standard output\n");
	print_help = 1;
    }

    if (!print_help)
	non_opt_argc = bu_opt_parse(NULL, ac, (const char **)av, dsp_opt_desc);

    if (print_help || non_opt_argc < 2) {
	const char *help = bu_opt_describe(dsp_opt_desc, NULL);
	bu_log(usage);
	bu_log("Options:\n%s\n", help);
	bu_free((char *)help, "help str");
	bu_exit (1, NULL);
    }

    /* Open the files */
    f1 = av[0];
    f2 = av[1];

    in1 = fopen(f1, "r");
    if (!in1) {
	perror(f1);
	return -1;
    }

    if (fstat(fileno(in1), &sb)) {
	perror(f1);
	fclose(in1);
	return -1;
    }

    count = sb.st_size;
    buf1 = (unsigned short *)bu_malloc((size_t)sb.st_size, "buf1");

    in2 = fopen(f2, "r");
    if (!in2) {
	perror(f2);
	fclose(in1);
	return -1;
    }

    if (fstat(fileno(in2), &sb)) {
	perror(f2);
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
