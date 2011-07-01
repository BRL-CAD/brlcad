/*                        B W - P I X . C
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
/** @file util/bw-pix.c
 *
 * Convert an 8-bit black and white file to a 24-bit
 * color one by replicating each value three times.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


int
main(int argc, char **argv)
{
    unsigned char ibuf[1024], obuf[3*1024];
    size_t in, out, num;
    FILE *finp, *foutp;

    /* check for input file */
    if (argc > 1) {
	if ((finp = fopen(argv[1], "rb")) == NULL) {
	    bu_exit(1, "bw-pix: can't open \"%s\"\n", argv[1]);
	}
    } else
	finp = stdin;

    /* check for output file */
    if (argc > 2) {
	if ((foutp = fopen(argv[2], "wb")) == NULL) {
	    bu_exit(2, "bw-pix: can't open \"%s\"\n", argv[2]);
	}
    } else
	foutp = stdout;

    if (argc > 3 || isatty(fileno(finp)) || isatty(fileno(foutp))) {
	bu_exit(3, "Usage: bw-pix [in.bw] [out.pix]\n");
    }

    while ((num = fread(ibuf, sizeof(char), 1024, finp)) > 0) {
	size_t ret;
	for (in = out = 0; in < num; in++, out += 3) {
	    obuf[out] = ibuf[in];
	    obuf[out+1] = ibuf[in];
	    obuf[out+2] = ibuf[in];
	}
	ret = fwrite(obuf, sizeof(char), 3*num, foutp);
	if (ret == 0) {
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
