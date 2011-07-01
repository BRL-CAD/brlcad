/*                       B W 3 - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @file util/bw3-pix.c
 *
 * Merge three BW files into one RGB pix file.
 * (i.e. combine the colors)
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


void
open_file(FILE **fp, char *name)
{
    /* check for special names */
    if (BU_STR_EQUAL(name, "-")) {
	*fp = stdin;
	return;
    } else if (BU_STR_EQUAL(name, ".")) {
	*fp = fopen("/dev/null", "r");
	return;
    }

    if ((*fp = fopen(name, "r")) == NULL) {
	bu_exit(2, "bw3-pix: Can't open \"%s\"\n", name);
    }
}


int
main(int argc, char **argv)
{
    unsigned char obuf[3*1024];
    unsigned char red[1024], green[1024], blue[1024];

    int i;
    int nr, ng, nb, num;
    unsigned char *obufp;
    FILE *rfp, *bfp, *gfp;

    if (argc != 4 || isatty(fileno(stdout))) {
	bu_exit(1, "usage: bw3-pix redin greenin bluein > file.pix (- stdin, . skip)\n");
    }

    open_file(&rfp, argv[1]);
    open_file(&gfp, argv[2]);
    open_file(&bfp, argv[3]);

    while (1) {
	nr = fread(red, sizeof(char), 1024, rfp);
	ng = fread(green, sizeof(char), 1024, gfp);
	nb = fread(blue, sizeof(char), 1024, bfp);
	if (nr <= 0 && ng <= 0 && nb <= 0)
	    break;

	/* find max */
	num = (nr > ng) ? nr : ng;
	if (nb > num) num = nb;
	if (nr < num)
	    memset((char *)&red[nr], 0, num-nr);
	if (ng < num)
	    memset((char *)&green[ng], 0, num-ng);
	if (nb < num)
	    memset((char *)&blue[nb], 0, num-nb);

	obufp = &obuf[0];
	for (i = 0; i < num; i++) {
	    *obufp++ = red[i];
	    *obufp++ = green[i];
	    *obufp++ = blue[i];
	}
	num = fwrite(obuf, sizeof(char), num*3, stdout);
	if (num <= 0) {
	    perror("fwrite");
	    break;
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
