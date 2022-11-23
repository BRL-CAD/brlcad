/*                       P I X D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2022 United States Government as represented by
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
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "bio.h"

#include "bu/app.h"
#include "bu/str.h"
#include "bu/exit.h"


static void
rgb_diff(int c1, int c2, FILE *output, size_t *offmany, size_t *off1, size_t *matching)
{
    int i;

    if (!output)
	output = stdout;
    if (!offmany || !off1 || !matching)
	return;

    if (c1 != c2) {
	i = c1 - c2;
	if (i < 0)
	    i = -i;
	if (i > 1) {
	    putc(0xFF, output);
	    (*offmany)++;
	} else {
	    putc(0xC0, output);
	    (*off1)++;
	}
    } else {
	putc(0, output);
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
    struct stat sf1, sf2;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (argc != 3 || isatty(fileno(stdout))) {
	bu_exit(1, "Usage: pixdiff f1.pix f2.pix >file.pix\n");
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
	return 1;
    }

    stat(argv[1], &sf1);
    stat(argv[2], &sf2);

    if (sf1.st_size != sf2.st_size) {
	bu_exit(1, "Different file sizes found: %s(%jd) and %s(%jd).  Cannot perform pixdiff.\n", argv[1], (intmax_t)sf1.st_size, argv[2], (intmax_t)sf2.st_size);
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
	    putc(i, stdout);
	    putc(i, stdout);
	    putc(i, stdout);
	    matching += 3;
	}
    }
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
