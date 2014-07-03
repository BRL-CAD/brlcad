/*                          D - B W . C
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
/** @file d-bw.c
 *
 * Convert doubles to unsigned bytes.
 *
 *	% d-bw [-n || scale]
 *
 *	-n will normalize the data (scale -1.0 to +1.0
 *		between 0 and 255).
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"


static void
printusage(void)
{
    bu_exit(1, "Usage: d-bw [-n || scale] < doubles > unsigned_chars\n");
}


int
main(int argc, char *argv[])
{
    double ibuf[512];
    unsigned char obuf[512];

    int i, num;
    double scale = 1.0;
    double value;
    int clip_high, clip_low;
    size_t ret;

    if (BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?"))
	printusage();

    if (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-n"))
	    scale = 255.0;
	else
	    scale = atof(argv[1]);
	argc--;
    }

    if (argc > 1 || ZERO(scale) || isatty(fileno(stdin))) {
	fprintf(stderr, "bad argument\n");
	printusage();
    }

    clip_high = clip_low = 0;

    while ((num = fread(&ibuf[0], sizeof(ibuf[0]), 512, stdin)) > 0) {
	for (i = 0; i < num; i++) {
	    value = ibuf[i] * scale;
	    if (value > 255.0) {
		obuf[i] = 255;
		clip_high++;
	    } else if (value < 0.0) {
		obuf[i] = 0;
		clip_low++;
	    } else
		obuf[i] = (int)value;
	}

	ret = fwrite(&obuf[0], sizeof(obuf[0]), num, stdout);
	if (ret != (size_t)num)
	    perror("fwrite");
    }

    if (clip_low != 0 || clip_high != 0)
	fprintf(stderr, "Warning: Clipped %d high, %d low\n",
		clip_high, clip_low);

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
