/*                           C - D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @file c-d.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/exit.h"
#include "bu/snooze.h"
#include "vmath.h"


#if defined(HAVE_HYPOT) && !defined(HAVE_DECL_HYPOT)
extern double hypot(double x, double y);
#endif


int
main(int argc, char *argv[])
{
    static const char usage[] = "Usage: c-d -r -i -m -p -z < complex_data > doubles\n";

    int rflag = 0;
    int iflag = 0;
    int mflag = 0;
    int pflag = 0;
    int zflag = 0;

    double ibuf[512];
    double obuf[512];
    double *obp;

    size_t i, num, onum;
    size_t ret;

    bu_setprogname(argv[0]);

    fprintf(stderr,"DEPRECATION WARNING:  This command is scheduled for removal.  Please contact the developers if you use this command.\n\n");
    bu_snooze(BU_SEC2USEC(1));

    if (argc <= 1 || isatty(fileno(stdin))) {
	bu_exit(1, "%s", usage);
    }

    while (argc > 1 && argv[1][0] == '-') {
	switch (argv[1][1]) {
	    case 'r':
		rflag++;
		break;
	    case 'i':
		iflag++;
		break;
	    case 'm':
		mflag++;
		break;
	    case 'p':
		pflag++;
		break;
	    case 'z':
		zflag++;
		break;
	}
	argc--;
	argv++;
    }

    while ((num = fread(&ibuf[0], sizeof(ibuf[0]), 512, stdin)) > 0) {
	onum = 0;
	obp = obuf;
	for (i = 0; i < num; i += 2) {
	    if (rflag) {
		*obp++ = ibuf[i];
		onum++;
	    }
	    if (iflag) {
		*obp++ = ibuf[i+1];
		onum++;
	    }
	    if (mflag) {
		*obp++ = hypot(ibuf[i], ibuf[i+1]);
		onum++;
	    }
	    if (pflag) {
		if (ZERO(ibuf[i]) && ZERO(ibuf[i+1]))
		    *obp++ = 0;
		else
		    *obp++ = atan2(ibuf[i], ibuf[i+1]);
		onum++;
	    }
	    if (zflag) {
		*obp++ = 0.0;
		onum++;
	    }
	}
	ret = fwrite(&obuf[0], sizeof(obuf[0]), onum, stdout);
	if (ret != onum)
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
