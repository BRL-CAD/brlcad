/*                      P I X M E R G E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @file util/pixmerge.c
 *
 * Given two streams of data, typically pix(5) or bw(5) images,
 * generate an output stream of the same size, where the value of the
 * output is determined by a formula involving the first (foreground)
 * stream and a constant, or the value of the second (background)
 * stream.
 *
 * This routine operates on a pixel-by-pixel basis, and thus is
 * independent of the resolution of the image.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


static char *f1_name;
static char *f2_name;
static FILE *f1;
static FILE *f2;

#define LT 1
#define EQ 2
#define GT 4
#define NE 8
static int wanted;			/* LT|EQ|GT conditions to pick fb */

static int seen_const;
static int seen_formula;

static int width = 3;
static unsigned char pconst[32];

#define CHUNK 1024
static char *b1;			/* fg input buffer */
static char *b2;			/* bg input buffer */
static char *b3;			/* output buffer */

static long fg_cnt;
static long bg_cnt;

static char usage[] = "\
Usage: pixmerge [-g -l -e -n] [-w bytes_wide] [-C r/g/b]\n\
	foreground.pix background.pix > out.pix\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "glenw:C:c:")) != -1) {
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
	    case 'w':
		c = atoi(bu_optarg);
		if (c > 1 && c < (int)sizeof(pconst))
		    width = c;
		break;
	    case 'C':
	    case 'c':	/* backword compatability */
		{
		    char *cp = bu_optarg;
		    unsigned char *conp = pconst;

		    /* premature null => atoi gives zeros */
		    for (c=0; c < width; c++) {
			*conp++ = atoi(cp);
			while (*cp && *cp++ != '/')
			    ;
		    }
		}
		seen_const = 1;
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind+2 > argc)
	return 0;

    f1_name = argv[bu_optind++];
    if (BU_STR_EQUAL(f1_name, "-"))
	f1 = stdin;
    else if ((f1 = fopen(f1_name, "r")) == NULL) {
	perror(f1_name);
	(void)fprintf(stderr,
		      "pixmerge: cannot open \"%s\" for reading\n",
		      f1_name);
	return 0;
    }

    f2_name = argv[bu_optind++];
    if (BU_STR_EQUAL(f2_name, "-"))
	f2 = stdin;
    else if ((f2 = fopen(f2_name, "r")) == NULL) {
	perror(f2_name);
	(void)fprintf(stderr,
		      "pixmerge: cannot open \"%s\" for reading\n",
		      f2_name);
	return 0;
    }

    if (argc > bu_optind)
	(void)fprintf(stderr, "pixmerge: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    size_t ret;

    if (!get_args(argc, argv) || isatty(fileno(stdout))) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    if (!seen_formula) {
	wanted = GT;
	seen_const = 1;		/* Default is const of 0/0/0 */
    }
    fprintf(stderr, "pixmerge: Selecting foreground when fg ");
    if (wanted & LT) putc('<', stderr);
    if (wanted & EQ) putc('=', stderr);
    if (wanted & GT) putc('>', stderr);
    if (wanted & NE) fprintf(stderr, "!=");
    if (seen_const) {
	int i;

	putc(' ', stderr);
	for (i = 0; i < width; i++) {
	    fprintf(stderr, "%d", pconst[i]);
	    if (i < width-1)
		putc('/', stderr);
	}
	putc('\n', stderr);
    } else {
	fprintf(stderr, " bg\n");
    }

    if ((b1 = (char *)malloc(width*CHUNK)) == (char *)0 ||
	(b2 = (char *)malloc(width*CHUNK)) == (char *)0 ||
	(b3 = (char *)malloc(width*CHUNK)) == (char *)0) {
	fprintf(stderr, "pixmerge:  malloc failure\n");
	bu_exit (3, NULL);
    }

    while (1) {
	unsigned char *cb1, *cb2;	/* current input buf ptrs */
	unsigned char *cb3; 	/* current output buf ptr */
	unsigned char *ebuf;		/* end ptr in b1 */
	int r1, r2, len;

	r1 = fread(b1, width, CHUNK, f1);
	r2 = fread(b2, width, CHUNK, f2);
	len = r1;
	if (r2 < len)
	    len = r2;
	if (len <= 0)
	    break;

	cb1 = (unsigned char *)b1;
	cb2 = (unsigned char *)b2;
	cb3 = (unsigned char *)b3;
	ebuf = cb1 + width*len;
	for (; cb1 < ebuf; cb1 += width, cb2 += width) {
	    /*
	     * Stated condition must hold for all input bytes
	     * to select the foreground for output
	     */
	    unsigned char *ap, *bp;
	    unsigned char *ep;		/* end ptr */

	    ap = cb1;
	    if (seen_const)
		bp = pconst;
	    else
		bp = cb2;
	    if (wanted & NE) {
		for (ep = cb1+width; ap < ep; ap++, bp++) {
		    if (*ap != *bp)
			goto success;
		}
		goto fail;
	    } else {
		for (ep = cb1+width; ap < ep; ap++, bp++) {
		    if (*ap > *bp) {
			if (!(GT & wanted)) goto fail;
		    } else if (*ap == *bp) {
			if (!(EQ & wanted)) goto fail;
		    } else {
			if (!(LT & wanted)) goto fail;
		    }
		}
	    }
	success:
	    {
		int i;
		ap = cb1;
		for (i=0; i<width; i++)
		    *cb3++ = *ap++;
	    }
	    fg_cnt++;
	    continue;
	fail:
	    {
		int i;
		bp = cb2;
		for (i=0; i<width; i++)
		    *cb3++ = *bp++;
	    }
	    bg_cnt++;
	}
	ret = fwrite(b3, width, len, stdout);
	if (ret < (size_t)len)
	    perror("fwrite");
    }
    fprintf(stderr, "pixmerge: %ld foreground, %ld background\n",
	    fg_cnt, bg_cnt);

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
