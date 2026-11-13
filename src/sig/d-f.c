/*                           D - F . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file d-f.c
 *
 * Convert doubles to floats.
 *
 *	% d-f [-n || scale]
 *
 *	-n will normalize the data (scale -1.0 to +1.0
 *		between -1.0 and +1.0 in this case!).
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/exit.h"
#include "bu/snooze.h"
#include "vmath.h"

#include "scale_args.h"


int
main(int argc, char *argv[])
{
    double ibuf[512] = { 0.0 };
    float obuf[512] = { 0.0f};

    size_t i, num;
    double scale = 1.0;
    size_t ret;
    struct sig_scale_args args = {0};

    bu_setprogname(argv[0]);

    int parse_status = sig_scale_args_parse(&args, "d-f",
	"Convert double-precision values to floats", argc, argv);
    if (parse_status <= 0)
	return parse_status < 0 ? 1 : 0;
    if (args.has_scale)
	scale = args.scale;

    fprintf(stderr,"DEPRECATION WARNING:  This command is scheduled for removal.  Please contact the developers if you use this command.\n\n");
    bu_snooze(BU_SEC2USEC(1));

    if (isatty(fileno(stdin)) || isatty(fileno(stdout)))
	bu_exit(1, "d-f requires redirected binary input and output\n");

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
