/*                       P I X D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file util/pixdiff.c
 *
 * Compute the difference between two .pix files.  To establish
 * context, a half-intensity monochrome image is produced when there
 * are no differences; otherwise the channels that differ are
 * highlighted for differing pixels.
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

int
main(int argc, char *argv[])
{
    long matching = 0;
    long off1 = 0;
    long offmany = 0;

    FILE *f1, *f2;

    if (argc != 3 || isatty(fileno(stdout))) {
	bu_exit(1, "Usage: pixdiff f1.pix f2.pix >file.pix\n");
    }

    if (BU_STR_EQUAL(argv[1], "-"))
	f1 = stdin;
    else if ((f1 = fopen(argv[1], "r")) == NULL) {
	perror(argv[1]);
	return 1;
    }
    if (BU_STR_EQUAL(argv[2], "-"))
	f2 = stdin;
    else if ((f2 = fopen(argv[2], "r")) == NULL) {
	perror(argv[2]);
	return 1;
    }

    while (1) {
	int r1, g1, b1;
	int r2, g2, b2;

	r1 = fgetc(f1);
	g1 = fgetc(f1);
	b1 = fgetc(f1);
	r2 = fgetc(f2);
	g2 = fgetc(f2);
	b2 = fgetc(f2);
	if (feof(f1) || feof(f2)) break;

	if (r1 != r2 || g1 != g2 || b1 != b2) {
	    int i;

	    /* Highlight differing channels */
	    if (r1 != r2) {
		if ((i = r1 - r2) < 0) i = -i;
		if (i > 1) {
		    putc(0xFF, stdout);
		    offmany++;
		} else {
		    putc(0xC0, stdout);
		    off1++;
		}
	    } else {
		putc(0, stdout);
		matching++;
	    }
	    if (g1 != g2) {
		if ((i = g1 - g2) < 0) i = -i;
		if (i > 1) {
		    putc(0xFF, stdout);
		    offmany++;
		} else {
		    putc(0xC0, stdout);
		    off1++;
		}
	    } else {
		putc(0, stdout);
		matching++;
	    }
	    if (b1 != b2) {
		if ((i = b1 - b2) < 0) i = -i;
		if (i > 1) {
		    putc(0xFF, stdout);
		    offmany++;
		} else {
		    putc(0xC0, stdout);
		    off1++;
		}
	    } else {
		putc(0, stdout);
		matching++;
	    }
	} else {
	    /* Common case: equal.  Give B&W NTSC average of 0.35 R +
	     * 0.55 G + 0.10 B, calculated in fixed-point, output at
	     * half intensity.
	     */
	    long i;
	    i = ((22937 * r1 + 36044 * g1 + 6553 * b1)>>17);
	    if (i < 0) i = 0;
	    i /= 2;
	    putc(i, stdout);
	    putc(i, stdout);
	    putc(i, stdout);
	    matching += 3;
	}
    }
    fprintf(stderr,
	    "pixdiff bytes: %7ld matching, %7ld off by 1, %7ld off by many\n",
	    matching, off1, offmany);

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
