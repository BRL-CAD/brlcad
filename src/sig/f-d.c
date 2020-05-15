/*                           F - D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/** @file f-d.c
 *
 * Convert floats to doubles.
 *
 *	% f-d [-n || scale]
 *
 *	-n will normalize the data (scale -1.0 to +1.0
 *		between -1.0 and +1.0 in this case!).
 *
 */

#include "common.h"

#include <stdlib.h> /* for atof() */
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/str.h"
#include "bu/exit.h"
#include "bu/snooze.h"
#include "vmath.h"


int
main(int argc, char *argv[])
{
    int i, num;
    double scale = 1.0;
    size_t ret;

    float ibuf[512];
    double obuf[512];

    bu_setprogname(argv[0]);

    fprintf(stderr,"DEPRECATION WARNING:  This command is scheduled for removal.  Please contact the developers if you use this command.\n\n");
    bu_snooze(BU_SEC2USEC(1));

    if (argc > 1) {
	if (!BU_STR_EQUAL(argv[1], "-n"))
	    scale = atof(argv[1]);
	argc--;
    }

    if (argc > 1 || ZERO(scale) || isatty(fileno(stdin)) || isatty(fileno(stdout)))
	bu_exit(1, "Usage: f-d [-n || scale] < floats > doubles\n");

    while ((num = fread(&ibuf[0], sizeof(ibuf[0]), 512, stdin)) > 0) {
	if (EQUAL(scale, 1.0)) {
	    for (i = 0; i < num; i++)
		obuf[i] = ibuf[i];
	} else {
	    for (i = 0; i < num; i++)
		obuf[i] = ibuf[i] * scale;
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
