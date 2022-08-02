/*                      P I X M A T T E . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2022 United States Government as represented by
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
/** @file util/pixmatte.c
 *
 * Given four streams of data elements,
 * where element is of arbitrary width,
 * typically pix(5) or bw(5) images,
 * output a stream of the same number of data elements.
 * The value of the output stream is determined element-by-element,
 * by comparing the
 * first (foreground) input stream with the
 * the second (background, or matte) input stream.
 * If the formula holds true, the element from the
 * true-output stream is written,
 * otherwise, the element from the false-output stream is written.
 * Each of these streams comes from a file, or is given as a constant.
 * A particular file may be used to feed more than one stream,
 * and the name '-' specifies stdin.
 * For example, the foreground file may also be the true-output file.
 *
 * This routine operates on an element-by-element basis, and thus
 * is independent of the resolution of the image.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/str.h"

#define NFILES 4 /* Two in, two out */
#define EL_WIDTH 32 /* Max width of one element */
#define CHUNK (32*1024)	/* # elements per I/O */

static int width = 3;

static char *file_name[NFILES];
static FILE *fp[NFILES];	/* NULL => use f_const */
static unsigned char f_const[NFILES][EL_WIDTH];
static char *buf[NFILES];	/* I/O buffers, size width*CHUNK */
static char *obuf;		/* output buffer */

#define LT 1
#define EQ 2
#define GT 4
#define NE 8
#define APPROX 16
static int wanted = 0;		/* LT|EQ|GT conditions */

static long true_cnt = 0;
static long false_cnt = 0;

static int twoconstants;
static int limit = 2;

static const char usage_msg[] = "\
Usage: pixmatte [-w width_in_bytes] {-g -l -e -n -a}\n\
	in1 in2 true_out false_out > output\n\
\n\
where each of the 4 streams is either a file name, a constant of the\n\
form =r/g/b with each byte specified in decimal, or '-' for stdin.\n\
The default width_in_bytes is 3 bytes, suitable for processing .pix files.\n\
";


void
usage(const char *s, int n)
{
    if (s && *s) (void)fputs(s, stderr);

    bu_exit(n, "%s", usage_msg);
}


int
open_file(int i, char *name)
{
    if (name[0] == '=') {
	/* Parse constant */
	char *cp = name+1;
	unsigned char *conp = &f_const[i][0];
	int j;

	/* premature null => atoi gives zeros */
	for (j=0; j < width; j++) {
	    *conp++ = atoi(cp);
	    while (*cp && *cp++ != '/')
		;
	}

	file_name[i] = name+1;	/* skip '=' */
	fp[i] = NULL;
	buf[i] = NULL;
	return 0;		/* OK */
    }

    file_name[i] = name;
    if (BU_STR_EQUAL(name, "-")) {
	fp[i] = stdin;
	if (isatty(fileno(stdin)))
	    return -1;	/* FAIL */
	/* XXX No checking for multiple uses of stdin */
    } else if ((fp[i] = fopen(name, "rb")) == NULL) {
	perror(name);
	bu_log("pixmatte: cannot open \"%s\" for reading\n", name);
	return -1;		/* FAIL */
    }

    /* Obtain buffer */
    if ((buf[i] = (char *)malloc(width*CHUNK)) == (char *)0) {
	bu_exit (3, "pixmatte: input buffer malloc failure\n");
    }

    return 0;			/* OK */
}


void
get_args(int argc, char **argv)
{
    int c;
    int seen_formula = 0;
    int i;

    while ((c = bu_getopt(argc, argv, "glenaw:h?")) != -1) {
	switch (c) {
	    case 'g':
		wanted |= GT;
		seen_formula = 1;
		break;
	    case 'l':
		wanted |= LT;
		seen_formula = 1;
		break;
	    case 'e':
		wanted |= EQ;
		seen_formula = 1;
		break;
	    case 'n':
		wanted |= NE;
		seen_formula = 1;
		break;
	    case 'a':
		wanted |= APPROX;
		/* This does NOT count as a formula, so seen_formula
                 * is left unchanged.
                 */
		break;
	    case 'w':
		width = atoi(bu_optarg);
		if (width < 1 || width >= EL_WIDTH)
		    usage("pixmatte: illegal width specified\n", 1);
		break;
	    default:		/* '?' 'h' */
		usage("", 1);
		break;
	}
    }

    if (!seen_formula){
    	if (argc == 1) usage("", 1);
	usage("No formula specified\n", 1);
    }

    if (bu_optind+NFILES > argc)
	usage("insufficient number of input/output channels\n", 1);


    for (i=0; i < NFILES; i++) {
	if (open_file(i, argv[bu_optind++]) < 0)
	    usage((char *)NULL, 1);
    }

/* If both of the 1st 2 input streams were "=...", this is accounted
 * for in true value of "twoconstants", and the program will later
 * stop when it has created 512x512 file (otherwise, we get stuck
 * in loop).
 */
    twoconstants = (fp[0] == NULL && fp[1] == NULL);

    if (argc > bu_optind)
	bu_log("pixmatte: excess argument(s) ignored\n");

}


int
main(int argc, char **argv)
{

    int chunkcount = 0;

    bu_setprogname(argv[0]);

    get_args(argc, argv);

    if (isatty(fileno(stdout)))
	usage("Cannot write image to tty\n", 1);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);

    bu_log("pixmatte:\tif (%s ", file_name[0]);

    if (wanted & APPROX) fputs("~~", stderr);
    if (wanted & LT) fputs("<", stderr);
    if (wanted & EQ) fputs("=", stderr);
    if (wanted & GT) fputs(">", stderr);
    if (wanted & NE) fputs("!=", stderr);

    bu_log(" %s)\n", file_name[1]);
    bu_log("pixmatte:\t\tthen output %s\n", file_name[2]);
    bu_log("pixmatte:\t\telse output %s\n", file_name[3]);

    if ((obuf = (char *)malloc(width*CHUNK)) == (char *)0) {
	bu_exit (3, "pixmatte: obuf malloc failure\n");
    }

/* If "<>" is detected (in "wanted"), we change to "!=" because it's the same.
 */
    if ( (wanted & LT) && (wanted & GT)) {
    	wanted |= NE;
    	wanted ^= LT;
    	wanted ^= GT;
    }
/* If NE is detected, then there is no point also having either LT or GT.
 */
    else if ( (wanted & NE) ){
    	if (wanted & LT) wanted ^=LT;
    	else
    	if (wanted & GT) wanted ^=GT;
    }

    if (wanted & APPROX) {
	if ( (wanted & GT) && (wanted & EQ) )
	    limit = -1;
    	    /* remember that limit defaulted to 2,
	     * (for the case of APPROX/GT)
	     */
    	else if (wanted & LT) {
    	    if (wanted & EQ)
    		limit = 1;
    	     else
    		limit = -2;
	     }
    /* Remember that some cases are "don't care" w/r to "limit".
     */
    }


    while (1) {
	unsigned char *cb0, *cb1;	/* current input buf ptrs */
	unsigned char *cb2, *cb3;
	unsigned char *obp; 	/* current output buf ptr */
	unsigned char *ebuf;		/* end ptr in buf[0] */
	size_t len;
	int i;

    	chunkcount++;
	if ( chunkcount == 9 && twoconstants )
	    bu_exit (1, NULL);

	len = CHUNK;
	for (i=0; i<NFILES; i++) {
	    size_t got;

	    if (fp[i] == NULL) continue;
	    got = fread(buf[i], width, CHUNK, fp[i]);
	    if (got < len) len = got;
	}
	if (len <= 0)
	    break;

	cb0 = (unsigned char *)buf[0];
	cb1 = (unsigned char *)buf[1];
	cb2 = (unsigned char *)buf[2];
	cb3 = (unsigned char *)buf[3];
	obp = (unsigned char *)obuf;
	ebuf = cb0 + width*len;
	for (; cb0 < ebuf;
	     cb0 += width, cb1 += width, cb2 += width, cb3 += width) {
	    /*
	     * Stated condition must hold for all input bytes
	     * to select the foreground for output
	     */
	    unsigned char *ap, *bp;
	    unsigned char *ep;		/* end ptr */

	    if (buf[0] != NULL)
		ap = cb0;
	    else
		ap = &f_const[0][0];

	    if (buf[1] != NULL)
		bp = cb1;
	    else
		bp = &f_const[1][0];

	    if (wanted & NE) {
/* If both NE and EQ are detected, all elements yield true result,
 * and don't care about APPROX usage.
 */
	    	if (wanted & EQ) goto success;
		if (!(wanted & APPROX)) {
		    for (ep = ap+width; ap < ep;) {
			if (*ap++ == *bp++) goto fail;
		    }
		    goto success;
		}
	    }
	    if (wanted & APPROX) {
		if (wanted & NE) {
		    /* Want not even approx equal */
		    for (ep = ap+width; ap < ep;) {
			if ((i= *ap++ - *bp++) >= -1 &&
			    i <= 1)
			    goto fail;
		    }
		    goto success;
		}
	    	/* Cannot get here with LT and GT both in use, because
		 * if both LT and GT were detected, they would have been
	    	 * turned off and replaced with NE.
	    	 */
		if (wanted & GT) {
		    /* APPROX/GT/EQ (limit = -1):
                     *   Want approx GT/EQ; in addition to ">=",
                     *   we allow 1 on the OTHER side of equality.
		     * APPROX/GT (limit = 2):
		     *   Want approx GT; same as ">", except we
                     *   stay more than 1 away from equality.
		     */
		    for (ep = ap+width; ap < ep;) {
			if ((*ap++ - *bp++) < limit)
			    goto fail;
		    }
		    goto success;
		}
		if (wanted & LT) {
		    /* APPROX/LT/EQ (limit = 1):
		     *   Want approx LT/EQ; in addition to "<=",
                     *   we allow 1 on the OTHER side of equality.
		     * APPROX/LT (limit = -2):
		     *   Want approx LT; same as "<", except we
	             *   stay more than 1 away from equality.
		     */
		    for (ep = ap+width; ap < ep;) {
			if ((*ap++ - *bp++) > limit)
			    goto fail;
		    }
		    goto success;
		}
		if (wanted & EQ) {
		    /* Want approx equal */
		    for (ep = ap+width; ap < ep;) {
			if ((i= *ap++ - *bp++) < -1 ||
			    i > 1)
			    goto fail;
		    }
		    goto success;
		}
	    }

/* (wanted & APPROX) is false if we arrive here.
 */
	    for (ep = ap+width; ap < ep; ap++, bp++) {
		if (*ap > *bp) {
		    if (!(GT & wanted))
			goto fail;
		} else if (*ap == *bp) {
		    if (!(EQ & wanted))
			goto fail;
		} else {
		    if (!(LT & wanted))
			goto fail;
		}
	    }

	success:
	    if (buf[2] != NULL)
		ap = cb2;
	    else
		ap = &f_const[2][0];

	    for (i=0; i<width; i++)
		*obp++ = *ap++;

	    true_cnt++;
	    continue;
	fail:
	    if (buf[3] != NULL)
		bp = cb3;
	    else
		bp = &f_const[3][0];

	    for (i=0; i<width; i++)
		*obp++ = *bp++;

	    false_cnt++;
	}
	if (fwrite(obuf, width, len, stdout) != len) {
	    perror("fwrite");
	    bu_exit (1, "pixmatte: write error\n");
	}
    }

    bu_log("pixmatte: %ld element comparisons true, %ld false (width=%d)\n",
	   true_cnt, false_cnt, width);

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
