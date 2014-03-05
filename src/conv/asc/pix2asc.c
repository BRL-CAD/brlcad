/*                       P I X 2 A S C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2014 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <limits.h>

#include "bio.h"
#include "bu.h"

int
main(int UNUSED(ac), char **UNUSED(argv))
{
    unsigned char pix[3]; /* RGB of one pixel */

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    while (!feof(stdin)
	   && fread((void *)pix, sizeof(unsigned char) * 3, 1, stdin) == 1) {
	/* Input validation of the individual R, G, and B bytes
	   (required by Coverity) */
	int i;
	for (i = 0; i < 3; ++i) {
	    /* the cast to int is necessary to avoid a gcc warning of
	       an always true test */
	    int d = (int)pix[i];
	    if (d < 0 || d > UCHAR_MAX) {
		bu_bomb("Corrupt file!");
	    }
	    printf("%02X", pix[i]);
	}
	printf("\n");
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
