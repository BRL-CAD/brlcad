/*                        B W D I F F . C
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
/** @file util/bwdiff.c
 *
 * Take the difference between two BW files.
 * Output is: (file1-file2)/2 + 127
 * or magnitude (-m): abs(file1-file2)
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


#define DIFF	0
#define MAG	1
#define GREATER	2
#define LESS	3
#define EQUAL	4
#define NEQ	5


FILE *fp1, *fp2;

int mode = DIFF;
int backgnd = 0;
unsigned char ibuf1[512], ibuf2[512], obuf[512];

static const char usage[] = "\
Usage: bwdiff [-b -m -g -l -e -n] file1.bw file2.bw (- stdin, . skip)\n";

void
open_file(FILE **fp, char *name)
{
    /* check for special names */
    if (BU_STR_EQUAL(name, "-"))
	*fp = stdin;
    else if (BU_STR_EQUAL(name, "."))
	*fp = fopen("/dev/null", "r");
    else if ((*fp = fopen(name, "r")) == NULL)
	bu_exit(2, "bwdiff: Can't open \"%s\"\n", name);
    return;
}


int
main(int argc, char **argv)
{
    unsigned char *p1, *p2, *op;
    int i, n, m;
    size_t ret;

    while (argc > 3) {
	if (BU_STR_EQUAL(argv[1], "-m")) {
	    mode = MAG;
	} else if (BU_STR_EQUAL(argv[1], "-g")) {
	    mode = GREATER;
	} else if (BU_STR_EQUAL(argv[1], "-l")) {
	    mode = LESS;
	} else if (BU_STR_EQUAL(argv[1], "-e")) {
	    mode = EQUAL;
	} else if (BU_STR_EQUAL(argv[1], "-n")) {
	    mode = NEQ;
	} else if (BU_STR_EQUAL(argv[1], "-b")) {
	    backgnd++;
	} else
	    break;
	argv++;
	argc--;
    }

    if (argc != 3 || isatty(fileno(stdout))) {
	bu_exit(1, "%s", usage);
    }

    open_file(&fp1, argv[1]);
    open_file(&fp2, argv[2]);

    while (1) {
	n = fread(ibuf1, 1, 512, fp1);
	m = fread(ibuf2, 1, 512, fp2);
	if ((n == 0) && (m == 0))
	    break;
	p1 = &ibuf1[0];
	p2 = &ibuf2[0];
	op = &obuf[0];
	if (m < n) {
	    memset((char *)(&ibuf2[m]), 0, (n - m));
	}
	if (m > n) {
	    memset((char *)(&ibuf1[n]), 0, (m - n));
	    n = m;
	}
	/* unrolled for speed */
	switch (mode) {
	    case DIFF:
		for (i = 0; i < n; i++) {
		    *op++ = (((int)*p1 - (int)*p2)>>1) + 128;
		    p1++;
		    p2++;
		}
		break;
	    case MAG:
		for (i = 0; i < n; i++) {
		    *op++ = abs((int)*p1++ - (int)*p2++);
		}
		break;
	    case GREATER:
		for (i = 0; i < n; i++) {
		    if (*p1 > *p2++)
			*op++ = 255;
		    else {
			if (backgnd)
			    *op++ = *p1 >> 2;
			else
			    *op++ = 0;
		    }
		    p1++;
		}
		break;
	    case LESS:
		for (i = 0; i < n; i++) {
		    if (*p1 < *p2++)
			*op++ = 255;
		    else {
			if (backgnd)
			    *op++ = *p1 >> 2;
			else
			    *op++ = 0;
		    }
		    p1++;
		}
		break;
	    case EQUAL:
		for (i = 0; i < n; i++) {
		    if (*p1 == *p2++)
			*op++ = 255;
		    else {
			if (backgnd)
			    *op++ = *p1 >> 2;
			else
			    *op++ = 0;
		    }
		    p1++;
		}
		break;
	    case NEQ:
		for (i = 0; i < n; i++) {
		    if (*p1 != *p2++)
			*op++ = 255;
		    else {
			if (backgnd)
			    *op++ = (*p1) >> 1;
			else
			    *op++ = 0;
		    }
		    p1++;
		}
		break;
	}
	ret = fwrite(&obuf[0], 1, n, stdout);
	if (ret == 0) {
	    perror("fwrite");
	    break;
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
