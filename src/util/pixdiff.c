/*                       P I X D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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

#include "bu/app.h"
#include "bu/str.h"
#include "bu/exit.h"


static void
rgb_diff(int c1, int c2, FILE *output, size_t *offmany, size_t *off1, size_t *matching)
{
    int i;

    if (!output) {
	output = stdout;
    }
    if (!offmany || !off1 || !matching)
	return;

    if (c1 != c2) {
	i = c1 - c2;
	if (i < 0)
	    i = -i;
	if (i > 1) {
	    if (output)
		fputc(0xFF, output);
	    (*offmany)++;
	} else {
	    if (output)
		fputc(0xC0, output);
	    (*off1)++;
	}
    } else {
	if (output)
	    fputc(0, output);
	(*matching)++;
    }
}


int
main(int argc, char *argv[])
{
    size_t matching = 0;
    size_t off1 = 0;
    size_t offmany = 0;

    FILE *f1, *f2;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (argc != 3 || isatty(fileno(stdout))) {
	bu_exit(1, "Usage: pixdiff f1.pix f2.pix >file.pix\n");
    }

    if (BU_STR_EQUAL(argv[1], "-") && BU_STR_EQUAL(argv[2], "-")) {
	bu_exit(1, "pixdiff: standard input cannot supply both images\n");
    }

    if (BU_STR_EQUAL(argv[1], "-"))
	f1 = stdin;
    else if ((f1 = fopen(argv[1], "rb")) == NULL) {
	perror(argv[1]);
	return 1;
    }
    if (BU_STR_EQUAL(argv[2], "-"))
	f2 = stdin;
    else if ((f2 = fopen(argv[2], "rb")) == NULL) {
	perror(argv[2]);
	if (f1 != stdin)
	    fclose(f1);
	return 1;
    }

    while (1) {
	unsigned char p1[3];
	unsigned char p2[3];
	size_t count1 = fread(p1, 1, sizeof(p1), f1);
	size_t count2 = fread(p2, 1, sizeof(p2), f2);
	int r1, g1, b1;
	int r2, g2, b2;

	if (ferror(f1) || ferror(f2))
	    bu_exit(1, "pixdiff: input read error\n");
	if (count1 == 0 && count2 == 0) {
	    break;
	}
	if (count1 != sizeof(p1) || count2 != sizeof(p2)) {
	    bu_exit(1, "pixdiff: input sizes differ or contain an incomplete pixel\n");
	}

	r1 = p1[0];
	g1 = p1[1];
	b1 = p1[2];
	r2 = p2[0];
	g2 = p2[1];
	b2 = p2[2];

	if (r1 != r2 || g1 != g2 || b1 != b2) {
	    rgb_diff(r1, r2, stdout, &offmany, &off1, &matching);
	    rgb_diff(g1, g2, stdout, &offmany, &off1, &matching);
	    rgb_diff(b1, b2, stdout, &offmany, &off1, &matching);
	} else {
	    /* Common case: equal.  Give B&W NTSC average of 0.35 R +
	     * 0.55 G + 0.10 B, calculated in fixed-point, output at
	     * half intensity.
	     */
	    long i;
	    i = ((22937 * r1 + 36044 * g1 + 6553 * b1)>>17);
	    if (i < 0)
		i = 0;
	    i /= 2;
	    fputc(i, stdout);
	    fputc(i, stdout);
	    fputc(i, stdout);
	    matching += 3;
	}
    }

    if (f1 != stdin)
	fclose(f1);
    if (f2 != stdin)
	fclose(f2);

    fprintf(stderr,
	    "pixdiff bytes: %7zu matching, %7zu off by 1, %7zu off by many\n",
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
