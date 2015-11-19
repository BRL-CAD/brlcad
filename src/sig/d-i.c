/*                           D - I . C
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
/** @file d-i.c
 *
 * Convert doubles to 16bit ints
 *
 *	% d-i [-n || scale]
 *
 *	-n will normalize the data (scale -1.0 to +1.0
 *		between -32767 and +32767).
 *
 */

#include "common.h"

#include <stdlib.h> /* for atof() */
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/log.h"
#include "bu/str.h"
#include "vmath.h"


int
main(int argc, char *argv[])
{
    double ibuf[512];
    short obuf[512];

    int i, num;
    double scale = 1.0;
    double value;
    int clip_high, clip_low;
    size_t ret;

    if (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-n"))
	    scale = 32767.0;
	else
	    scale = atof(argv[1]);
	argc--;
    }

    if (argc > 1 || ZERO(scale) || isatty(fileno(stdin))) {
	bu_exit(1, "Usage: d-i [-n || scale] < doubles > shorts\n");
    }

    clip_high = clip_low = 0;

    while ((num = fread(&ibuf[0], sizeof(ibuf[0]), 512, stdin)) > 0) {
	for (i = 0; i < num; i++) {
	    value = ibuf[i] * scale;
	    if (value > 32767.0) {
		obuf[i] = 32767;
		clip_high++;
	    } else if (value < -32768.0) {
		obuf[i] = -32768;
		clip_low++;
	    } else
		obuf[i] = (int)value;
	}

	ret = fwrite(&obuf[0], sizeof(obuf[0]), num, stdout);
	if (ret != (size_t)num)
	    perror("fwrite");
    }

    if (clip_low != 0 || clip_high != 0)
	fprintf(stderr, "%s: warning: clipped %d high, %d low\n",
		argv[0], clip_high, clip_low);
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
