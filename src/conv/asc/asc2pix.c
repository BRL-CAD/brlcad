/*                       A S C 2 P I X . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
 *
 */

/** @file asc2pix.c
 *
 *  Convert ASCII (hex) pixel files to the binary form.
 *  For portable images.
 *  Can also be used for .bw files, and random file conversion.
 *  White space in the file is ignored.
 *  The input is processed as a byte stream, and need not have a multiple
 *  of three bytes.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bio.h"

int lmap[256];		/* Map HEX ASCII to binary in left nybble  */
int rmap[256];		/* Map HEX ASCII to binary in right nybble */

int
main(void)
{
    int	a, b;
    int	i;

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    /* Init rmap */

    /* set all to "unused" */
    for (i = 0; i < 256; i++)
	rmap[i] = -1;

    /* note that all input chars of interest use only low 4 bits */
    /* set chars '0' - '9' */
    for (i = 0; i < 10; i++)
	rmap['0' + i] = i;

    /* set chars 'A' - 'F' */
    for (i = 10; i < 16; i++)
	rmap['A' - 10 + i] = i;

    /* set chars 'a' - 'f' */
    for (i = 10; i < 16; i++)
	rmap['a' - 10 + i] = i;

    /* Init lmap */
    /* copy defined chars in rmap to lmap's corresponding int but
       shifted 4 bits left */
    for (i = 0; i < 256; i++) {
	if (rmap[i] >= 0)
	    lmap[i] = rmap[i] << 4;
	else
	    lmap[i] = -1;
    }

    for (;;) {
	/* get a valid hex char in i */
	do {
	    a = getchar();
	    if (a == EOF || a < 0 || a > 255) {
		fflush(stdout);
		return 0;
	    }
	} while ((i = lmap[a]) < 0);

	/* get the next hex char */
	b = getchar();
	if (b == EOF || b < 0 || b > 255) {
	    fprintf(stderr, "asc2pix: unexpected EOF in middle of hex number\n");
	    return 1;
	}

	if ((b = rmap[b]) < 0) {
	    fprintf(stderr, "asc2pix: illegal hex code in file, aborting\n");
	    return 1;
	}

	/* now output the two 4-bit chars combined as a single byte  */
	putc((i | b), stdout);
    }

    fflush(stdout);

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
