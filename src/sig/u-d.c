/*                           U - D . C
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
/** @file u-d.c
 *
 * Convert unsigned shorts to doubles.
 *
 *	% i-d [-n || scale]
 *
 *	-n will normalize the data (scale between -1.0 and +1.0).
 *
 */

#include "common.h"

#include <stdlib.h> /* for atof() */
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"


int
main(int argc, char *argv[])
{
    unsigned short ibuf[512];
    double obuf[512];

    int i, num;
    double scale = 1.0;
    size_t ret;

    if (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-n"))
	    scale = 1.0 / 65536.0;
	else
	    scale = atof(argv[1]);
	argc--;
    }

    if (argc > 1 || ZERO(scale) || isatty(fileno(stdin))) {
	bu_exit(1, "Usage: u-d [-n || scale] < shorts > doubles\n");
    }

    while ((num = fread(&ibuf[0], sizeof(ibuf[0]), 512, stdin)) > 0) {
	if (EQUAL(scale, 1.0)) {
	    for (i = 0; i < num; i++)
		obuf[i] = ibuf[i];
	} else {
	    for (i = 0; i < num; i++)
		obuf[i] = (double)ibuf[i] * scale;
	}

	ret = fwrite(&obuf[0], sizeof(obuf[0]), num, stdout);
	if (ret != (size_t)num)
	    perror("fwrite");
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
