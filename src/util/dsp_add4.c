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

/* #include "bu.h" */
#include "vmath.h"
#include "bn.h"

#include "bu_arg_parse.h" /* includes bu.h */

/* declarations to support use of TCLAP arg parsing */
static const char usage[] = "Example: dsp_add  dsp1.dsp  dsp2.dsp  dsp12added.dsp\n";

/* purpose: combine two dsp files
 *
 * description: Combines two dsp files (which are binary files
 * comprised of network unsigned shorts).  The two files must be of
 * identical size.  The result is a third file where
 * each cell's height is the total of the heights of the same cell
 * in the input files.
 *
 * See the BRL-CAD wiki for a tutorial on using dsp's.
 *
 * see_also: dsp(5) asc2dsp(1) cv(1)
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


int
main(int ac, char *av[])
{
    /*  int next_arg; *//* <= not needed */
    FILE *in1 = NULL;
    FILE *in2 = NULL;
    FILE *out1 = NULL;
    int conv;
    int in_cookie, out_cookie;
    size_t count = 0;
    size_t ret;
    struct stat sb;
    unsigned short *buf1 = NULL;
    unsigned short *buf2 = NULL;

    /* for arg processing */
    /* vars expected from cmd line parsing */
    int arg_err            = 0;
    int has_force          = 0;
    int has_help           = 0;
    char dsp1_fname[BU_ARG_PARSE_BUFSZ] = {0};
    char dsp2_fname[BU_ARG_PARSE_BUFSZ] = {0};
    char dsp3_fname[BU_ARG_PARSE_BUFSZ] = {0};

    /* FIXME: this '-?' arg doesn't work correctly due to some TCLAPisms */
    static bu_arg_switch_t h_arg;
    /* define a force option to allow user to shoot himself in the foot */
    static bu_arg_switch_t f_arg;
    /* need two file names */
    static bu_arg_unlabeled_value_t dsp1_arg;
    /* need two file names */
    static bu_arg_unlabeled_value_t dsp2_arg;
    /* the output file name */
    static bu_arg_unlabeled_value_t dsp3_arg;

    /* place the arg pointers in an array (note the array is of
     * type void* to hold the heterogeneous arg type structs)
     */
    static void *args[] = {
      &h_arg, &f_arg,
      &dsp1_arg, &dsp2_arg, &dsp3_arg,
      NULL
    };


    BU_ARG_SWITCH_INIT(
      h_arg,
      "?",
      "short-help",
      "Same as '-h' or '--help'"
      );

    BU_ARG_SWITCH_INIT(
      f_arg,
      "f",
      "force",
      "Allow overwriting existing files."
      );

    /* need two file names */
    BU_ARG_UNLABELED_VALUE_INIT(
      dsp1_arg,
      "",
      "dsp_infile1",
      "first dsp input file name",
      BU_ARG_REQUIRED,
      BU_ARG_STRING,
      ""
      );

    /* need two file names */
    BU_ARG_UNLABELED_VALUE_INIT(
      dsp2_arg,
      "",
      "dsp_infile2",
      "second dsp input file name",
      BU_ARG_REQUIRED,
      BU_ARG_STRING,
      ""
      );

    /* the output file name */
    BU_ARG_UNLABELED_VALUE_INIT(
      dsp3_arg,
      "",
      "dsp_outfile",
      "dsp output file name",
      BU_ARG_REQUIRED,
      BU_ARG_STRING,
      ""
      );

    /* parse the args */
    arg_err = bu_arg_parse(args, ac, av);

    if (arg_err == BU_ARG_PARSE_ERR) {
        /* the TCLAP exception handler has fired with its own message
         * so need no message here
         */
        bu_exit(EXIT_SUCCESS, NULL);
    }

    /* Get the value parsed by each arg. */
    has_force  = bu_arg_get_bool(&f_arg);
    has_help   = bu_arg_get_bool(&h_arg);
    bu_arg_get_string(&dsp1_arg, dsp1_fname);
    bu_arg_get_string(&dsp2_arg, dsp2_fname);
    bu_arg_get_string(&dsp3_arg, dsp3_fname);

    /* take appropriate action... */

    /* note this exit is SUCCESS because it is expected
     * behavior--important for good auto-man-page handling
     */
    if (has_help) {
      bu_exit(EXIT_SUCCESS, usage);
    }

    /* TCLAP doesn't check for confusion in file names */
    if (BU_STR_EQUAL(dsp3_fname, dsp1_fname)
        || BU_STR_EQUAL(dsp3_fname, dsp2_fname)) {
      bu_exit(EXIT_FAILURE, "overwriting an input file\n");
    }

    /* TCLAP doesn't check for existing files (FIXME: add to TCLAP) */
    if (!stat(dsp3_fname, &sb)) {
      if (has_force) {
        printf("WARNING: overwriting an existing file...\n");
        bu_file_delete(dsp3_fname);
      }
      else {
        bu_exit(EXIT_FAILURE, "overwriting an existing file (use the '-f' option to continue)\n");
      }
    }

    /* open files */
    in1 = fopen(dsp1_fname, "r");
    if (!in1) {
      perror(dsp1_fname);
      bu_exit(EXIT_FAILURE, "ERROR: input file open failure\n");
    }

    if (fstat(fileno(in1), &sb)) {
      perror(dsp1_fname);
      fclose(in1);
      bu_exit(EXIT_FAILURE, "ERROR: input file stat failure\n");
    }

    /* save size of first input file for comparison with other two */
    count = sb.st_size;
    /* check for zero-size file */
    if (!count) {
      perror(dsp1_fname);
      fclose(in1);
      bu_exit(EXIT_FAILURE, "zero-length input file\n");
    }

    buf1 = (unsigned short *)bu_malloc((size_t)sb.st_size, "buf1");

    in2 = fopen(dsp2_fname, "r");
    if (!in2) {
      perror(dsp2_fname);
      fclose(in1);
      bu_exit(EXIT_FAILURE, "ERROR: input file open failure\n");
    }

    if (fstat(fileno(in2), &sb)) {
      perror(dsp2_fname);
      fclose(in1);
      fclose(in2);
      bu_exit(EXIT_FAILURE, "ERROR: input file stat failure\n");
    }

    /* check for zero-size file */
    if (!sb.st_size) {
      perror(dsp2_fname);
      fclose(in1);
      fclose(in2);
      bu_exit(EXIT_FAILURE, "ERROR: zero-length input file\n");
    }

    if ((size_t)sb.st_size != count) {
      fclose(in1);
      fclose(in2);
      bu_exit(EXIT_FAILURE, "ERROR: input file size mis-match\n");
    }

    /* the output file is now named instead of being redirected */
    out1 = fopen(dsp3_fname, "w");
    if (!out1) {
      perror(dsp3_fname);
      fclose(in1);
      fclose(in2);
      fclose(out1);
      bu_exit(EXIT_FAILURE, "ERROR: output file open failure\n");
    }

    buf2 = (unsigned short *)bu_malloc((size_t)sb.st_size, "buf2");

    count = count >> 1; /* convert count of char to count of short */

    /* Read the terrain data */
    ret = fread(buf1, sizeof(short), count, in1);
    if (ret < count) {
	perror(dsp1_fname);
        fclose(in1);
        fclose(in2);
        fclose(out1);
	bu_exit(EXIT_FAILURE, "ERROR: input file short read count\n");
    }

    ret = fread(buf2, sizeof(short), count, in2);
    if (ret < count) {
	perror(dsp2_fname);
        fclose(in1);
        fclose(in2);
        fclose(out1);
	bu_exit(EXIT_FAILURE, "ERROR: input file short read count\n");
    }

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

    if (fwrite(buf1, sizeof(short), count, out1) != count) {
	perror(dsp3_fname);
        fclose(in1);
        fclose(in2);
        fclose(out1);
	bu_exit(EXIT_FAILURE, "ERROR: count error writing data\n");
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
