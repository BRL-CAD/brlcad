/*
 * Copyright (C) 1988 Research Institute for Advanced Computer Science.
 * All rights reserved.  The RIACS Software Policy contains specific
 * terms and conditions on the use of this software, and must be
 * distributed with any copies.  This file may be redistributed.  This
 * copyright and notice must be preserved in all copies made of this file.
 */
static char rcsid[] = "$Header$";

/*
 * Interface routines for Utah Raster Toolkit
 */

#include <stdio.h>
#include "rle.h"

/*
 * Globals are stored in a structure.
 */

rle_hdr hdr;

static struct {
    int             width;
    int             height;
    unsigned char **scan;
    int		    row;
} Globals;

void
rasterInit(fd, width, height)
int fd;
int width;
int height;
{
    FILE           *rleFile;
    int             i;

    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    /* Fake the names, since we don't have argv. */
    rle_names( &hdr, "rletoabA62", fd == 0 ? NULL : "RLE file", 0 );

    Globals.width = width;
    Globals.height = height;
    if (fd == 0) {
	rleFile = stdin;
    } else {
	rleFile = fdopen(fd, "r");
    }
    hdr.rle_file = rleFile;
    rle_get_setup_ok(&hdr, NULL, NULL);

    if (hdr.xmax > width) {
	fprintf(stderr, "Warning: RLE width (%d) exceeds maximum (%d)\n",
	    hdr.xmax, width);
    }
    if (hdr.ymax > height) {
	fprintf(stderr, "Warning: RLE height (%d) exceeds maximum (%d)\n",
	    hdr.ymax, height);
    }
    Globals.row = 0;
    Globals.scan = (unsigned char **) malloc((hdr.ncolors +
				      hdr.alpha) *
				     sizeof(unsigned char *));
    for (i = 0; i < hdr.ncolors + hdr.alpha; i++)
	Globals.scan[i] =
	    (unsigned char *)malloc(hdr.xmax+1);

    if (hdr.alpha) {
	Globals.scan++;
    }
}

void
rasterRowGet(red, green, blue)
unsigned char *red, *green, *blue;
{
    int             i, max;

    if (Globals.row < hdr.ymin || Globals.row > hdr.ymax) {
	for (i = 0; i < Globals.width; i++) {
	    red[i] = 0;
	    green[i] = 0;
	    blue[i] = 0;
	}
    } else {
	rle_getrow(&hdr, Globals.scan);
	max = hdr.xmax < Globals.width ?
	    hdr.xmax : Globals.width;
	for (i = 0 ; i < max; i++) {
	    red[i] = Globals.scan[0][i];
	    green[i] = Globals.scan[1][i];
	    blue[i] = Globals.scan[2][i];
	}
	for (; i < Globals.width; i++) {
	    red[i] = green[i] = blue[i] = 0;
	}
    }
    Globals.row++;
}

void
rasterDone()
{
    rle_puteof(&hdr);
}
