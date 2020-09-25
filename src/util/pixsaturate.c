/*                   P I X S A T U R A T E . C
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
/** @file util/pixsaturate.c
 *
 * Change the saturation of a row of pixels.  If sat is
 * set to 0.0 the result will be monochromatic, if sat is made
 * 1.0, the color will not change, if sat is made greater than 1.0,
 * the color intensity is increased.
 *
 * The input and output pixel values are in the range 0..255.
 *
 * This implementation, based on a subroutine by Paul Haeberli,
 * requires 6 multiplies, 5 adds, and 3 bound checks per pixel.
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/str.h"
#include "bu/exit.h"


#define RINTLUM (79)
#define GINTLUM (156)
#define BINTLUM (21)

char buf[3*16*1024];

void
printusage ()
{
	bu_exit(1, "Usage: pixsaturate saturation < infile.pix > outfile.pix\n");
}

int
main(int argc, char **argv)
{
    double sat;			/* saturation */
    int bw;			/* monochrome intensity */
    int rwgt, gwgt, bwgt;
    int rt, gt, bt;
    int n;
    size_t nby;
    unsigned char *cp;
    size_t ret;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (argc != 2)
	printusage ();
    if ( BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?") )
	printusage ();
    if (argv[1][0] == '-') {
    	fprintf(stderr,"pixsaturate: no options except -h or -?\n");
	printusage ();
    }
    if ( isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
    	fprintf(stderr,"pixsaturate: need pipes for stdin and stdout\n");
	printusage ();
    }
    sat = atof(argv[1]);

    rwgt = RINTLUM*(1.0-sat);
    gwgt = GINTLUM*(1.0-sat);
    bwgt = BINTLUM*(1.0-sat);

    while ((nby = fread(buf, 1, sizeof(buf), stdin)) > 0) {
	cp = (unsigned char *)buf;
	for (n = (int)nby; n > 0; n -= 3) {
	    rt = cp[0];
	    gt = cp[1];
	    bt = cp[2];
	    bw = (rwgt*rt + gwgt*gt + bwgt*bt)>>8;
	    rt = bw+sat*rt;
	    gt = bw+sat*gt;
	    bt = bw+sat*bt;
	    if (rt<0) rt = 0; else if (rt>255) rt=255;
	    if (gt<0) gt = 0; else if (gt>255) gt=255;
	    if (bt<0) bt = 0; else if (bt>255) bt=255;
	    *cp++ = rt;
	    *cp++ = gt;
	    *cp++ = bt;
	}
	ret = fwrite(buf, 1, nby, stdout);
	if (ret != nby) {
	    perror("fwrite");
	    return 1;
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
