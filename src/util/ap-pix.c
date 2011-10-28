/*                        A P - P I X . C
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
/** @file util/ap-pix.c
 *
 * Applicon color ink jet printer to .pix file converter.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu.h"


/* Dots are least most signifigant bit first in increasing index */
struct app_record {
    unsigned char ml[432];
    unsigned char yl[432];
    unsigned char cl[432];
} magline, yelline, cyaline;

struct {
    char red, green, blue;
} out;

FILE *magfp, *yelfp, *cyafp;
int verbose = 0;


int
main(int argc, char **argv)
{
    size_t ret;
    int i, bit;
    int line;

    const char *argv0 = argv[0];

    if (argc > 1 && BU_STR_EQUAL(argv[1], "-v")) {
	verbose++;
	argc--;
	argv++;
    }

    if (argc != 2) {
	bu_exit(1, "usage: %s [-v] file.ap > file.pix (3456 x ?)\n", argv0);
    }

    magfp = fopen(argv[1], "r");
    if (magfp == NULL) {
	bu_exit(2, "%s: can't open \"%s\"\n", argv0, argv[1]);
    }
    yelfp = fopen(argv[1], "r");
    fseek(yelfp, (long)(50*sizeof(yelline)), 0);
    cyafp = fopen(argv[1], "r");
    fseek(cyafp, (long)(100*sizeof(cyaline)), 0);

    line = 0;
    while ((int)fread(&cyaline, sizeof(cyaline), 1, cyafp) > 0) {
	ret = fread(&magline, sizeof(magline), 1, magfp);
	ret += fread(&yelline, sizeof(yelline), 1, yelfp);
	if (ret == 0) {
	    perror("fread");
	    bu_exit(1, "%s: read failure\n", argv0);
	}

	line++;

	for (i = 0; i < 432; i++) {
	    for (bit = 7; bit >= 0; bit--) {
		out.red = ((cyaline.cl[i]>>bit)&1) ? 0 : 255;
		out.green = ((magline.ml[i]>>bit)&1) ? 0 : 255;
		out.blue = ((yelline.yl[i]>>bit)&1) ? 0 : 255;
		ret = fwrite(&out, sizeof(out), 1, stdout);
		if (ret == 0) {
		    perror("fwrite");
		    bu_exit(1, "%s: read failure\n", argv0);
		}
	    }
	}
	if (verbose)
	    fprintf(stderr, "wrote line %d\n", line);
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
