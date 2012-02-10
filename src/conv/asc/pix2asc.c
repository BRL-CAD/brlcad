/*                       P I X 2 A S C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2012 United States Government as represented by
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

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "bio.h"
#include "bu.h"

unsigned char pix[3];		/* RGB of one pixel */

unsigned char map[18] = "0123456789ABCDEFx";

int
main(int UNUSED(ac), char **UNUSED(argv))
{
    int i = 0;
    int ok = 1;
#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
#endif
    while ( !feof(stdin) && fread( (void *)pix, sizeof(unsigned char) * 3, 1, stdin) == 1 )  {
        ok = 1;
        /* Input validation */
	for (i = 0; i < 3; ++i) {
	    /* really the max here should probably be that of the unsigned char, but pix files seem to have higher
             * numbers??  257 observed in moss.pix... */
	    if ((int)(*pix + sizeof(unsigned char) * i) < 0 || (int)(*pix + sizeof(unsigned char) * i) > UCHAR_MAX + 2) {
		ok = 0;
            }
	}
	if (ok) {
	    putc( map[pix[0]>>4], stdout );
	    putc( map[pix[0]&0xF], stdout );
	    putc( map[pix[1]>>4], stdout );
	    putc( map[pix[1]&0xF], stdout );
	    putc( map[pix[2]>>4], stdout );
	    putc( map[pix[2]&0xF], stdout );
	    putc( '\n', stdout );
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
