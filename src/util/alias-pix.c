/*                     A L I A S - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file util/alias-pix.c
 *
 * Convert ALIAS(tm) PIX format image files to BRL PIX fomat files.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include "bio.h"

#include "bu.h"


char *progname = "(noname)";


void usage(void)
{
    bu_exit(1, "Usage: %s [ -v ] < ALIASpixfile >BRLpixfile\n", progname);
}


struct aliashead {
    short x, y;		/* dimensions of this image in X and Y */
    short xoff, yoff;	/* offsets of pixels */
    short bitplanes;	/* the number of bits per pixel */
} hdr;

#define BUFCNT 256
struct pixel {
    unsigned char len, b, g, r;
} buf[BUFCNT];

int verbose = 0;

char *image;

int
main(int ac, char **av)
{
    int pixcnt, i;
    char *p;

    progname = *av;

    if (ac > 2 || isatty(fileno(stdin))) usage();

    if (ac > 1) {
	p = av[1];
	if (*p == '-' && *(p+1) == 'v') verbose = 1;
	else usage();
    }


    /* read header in architecture-independent fashion */
    hdr.x = (getchar() & 0x0ff) << 8;
    hdr.x += (getchar() & 0x0ff);

    hdr.y = (getchar() & 0x0ff) << 8;
    hdr.y += (getchar() & 0x0ff);

    hdr.xoff = (getchar() & 0x0ff) << 8;
    hdr.xoff += (getchar() & 0x0ff);

    hdr.yoff = (getchar() & 0x0ff) << 8;
    hdr.yoff += (getchar() & 0x0ff);

    hdr.bitplanes = (getchar() & 0x0ff) << 8;
    hdr.bitplanes += (getchar() & 0x0ff);

    if (hdr.bitplanes != 24) {
	(void) fprintf(stderr, "Weird image file:\n");
	(void) fprintf(stderr,
		       "X: %d Y: %d xoff: %d yoff: %d bits/pixel: %d\n",
		       hdr.x, hdr.y, hdr.xoff, hdr.yoff, hdr.bitplanes);
	bu_exit(-1, "ERROR: unexpected image file\n");
    }

    if (verbose) {
	(void) fprintf(stderr,
		       "X: %d Y: %d xoff: %d yoff: %d bits/pixel: %d\n",
		       hdr.x, hdr.y, hdr.xoff, hdr.yoff, hdr.bitplanes);
    }


    image=(char *)bu_malloc(hdr.x*hdr.y*3, "image buffer");

    /* read and "unpack" the image */
    p = image;
    while ((pixcnt=fread(buf, sizeof(struct pixel), BUFCNT, stdin)) > 0) {
	for (i=0; i < pixcnt; ++i)
	    do {
		*p++ = buf[i].r;
		*p++ = buf[i].g;
		*p++ = buf[i].b;
	    } while (--buf[i].len > 0);
    }

    /* write out the image scanlines, correcting for different origin */

    for (i=hdr.y-1; i >= 0; --i) {
	if (fwrite(&image[i*hdr.x*3], hdr.x*3, 1, stdout) != 1) {
	    bu_exit(-1, "%s: Error writing image\n", *av);
	}
    }


    bu_free(image, "image buffer");
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
