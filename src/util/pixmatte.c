/*                      P I X M A T T E . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
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
 * For example, the forground file may also be the true-output file.
 *
 * This routine operates on an element-by-element basis, and thus
 * is independent of the resolution of the image.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


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
static int wanted;		/* LT|EQ|GT conditions */

static long true_cnt = 0;
static long false_cnt = 0;


static const char usage_msg[] = "\
Usage: pixmatte [-w bytes_wide] {-g -l -e -n -a}\n\
	in1 in2 true_out false_out > output\n\
\n\
where each of the 4 streams is either a file name, a constant of the\n\
form =r/g/b with each byte specified in decimal, or '-' for stdin.\n\
The default width is 3 bytes, suitable for processing .pix files.\n\
";


void
usage(char *s, int n)
{
    if (s && *s) (void)fputs(s, stderr);

    bu_exit(n, "%s", usage_msg);
}


/*
 * O P E N _ F I L E
 */
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
#if defined(_WIN32) && !defined(__CYGWIN__)
    } else if ((fp[i] = fopen(name, "rb")) == NULL) {
#else
    } else if ((fp[i] = fopen(name, "r")) == NULL) {
#endif
	perror(name);
	bu_log("pixmatte: cannot open \"%s\" for reading\n", name);
	return -1;		/* FAIL */
    }

    /* Obtain buffer */
    if ((buf[i] = (char *)malloc(width*CHUNK)) == (char *)0) {
	bu_exit (3, "pixmatte:  input buffer malloc failure\n");
    }

    return 0;			/* OK */
}


/*
 * G E T _ A R G S
 */
void
get_args(int argc, char **argv)
{
    int c;
    int seen_formula = 0;
    int i;

    while ((c = bu_getopt(argc, argv, "glenaw:")) != -1) {
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
		/* Formula not seen */
		break;
	    case 'w':
		c = atoi(bu_optarg);
		if (c >= 1 && c < EL_WIDTH)
		    width = c;
		else
		    usage("Illegal width specified\n", 1);
		break;
	    default:		/* '?' */
		usage("unknown option\n", 1);
		break;
	}
    }

    if (!seen_formula)
	usage("No formula specified\n", 1);


    if (bu_optind+NFILES > argc)
	usage("insufficient number of input/output channels\n", 1);


    for (i=0; i < NFILES; i++) {
	if (open_file(i, argv[bu_optind++]) < 0)
	    usage((char *)NULL, 1);
    }

    if (argc > bu_optind)
	bu_log("pixmatte: excess argument(s) ignored\n");

}


int
main(int argc, char **argv)
{

    get_args(argc, argv);

    if (isatty(fileno(stdout)))
	usage("Cannot write image to tty\n", 1);

#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);
#endif

    bu_log("pixmatte:\tif (%s ", file_name[0]);
    if (wanted & LT) {
	if (wanted & EQ)
	    fputs("<=", stderr);
	else
	    fputs("<", stderr);
    }
    if (wanted & GT) {
	if (wanted & EQ)
	    fputs(">=", stderr);
	else
	    fputs(">", stderr);
    }
    if (wanted & APPROX) {
	if (wanted & EQ) fputs("~~==", stderr);
	if (wanted & NE) fputs("~~!=", stderr);
    } else {
	if (wanted & EQ) fputs("==", stderr);
	if (wanted & NE) fputs("!=", stderr);
    }
    bu_log(" %s)\n", file_name[1]);
    bu_log("pixmatte:\t\tthen output %s\n", file_name[2]);
    bu_log("pixmatte:\t\telse output %s\n", file_name[3]);

    if ((obuf = (char *)malloc(width*CHUNK)) == (char *)0) {
	bu_exit (3, "pixmatte:  obuf malloc failure\n");
    }

    while (1) {
	unsigned char *cb0, *cb1;	/* current input buf ptrs */
	unsigned char *cb2, *cb3;
	unsigned char *obp; 	/* current output buf ptr */
	unsigned char *ebuf;		/* end ptr in buf[0] */
	size_t len;
	int i;

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

	    if (wanted == NE) {
		for (ep = ap+width; ap < ep;) {
		    if (*ap++ != *bp++)
			goto success;
		}
		goto fail;
	    } else if (wanted & APPROX) {
		if (wanted & NE) {
		    /* Want not even approx equal */
		    for (ep = ap+width; ap < ep;) {
			if ((i= *ap++ - *bp++) < -1 ||
			    i > 1)
			    goto success;
		    }
		    goto fail;
		} else {
		    /* Want approx equal */
		    for (ep = ap+width; ap < ep;) {
			if ((i= *ap++ - *bp++) < -1 ||
			    i > 1)
			    goto fail;
		    }
		    goto success;
		}
	    } else {
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
	    bu_exit (1, "pixmatte:  write error\n");
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
