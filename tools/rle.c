/*
 * Copyright (C) 1988 Research Institute for Advanced Computer Science.
 * All rights reserved.  The RIACS Software Policy contains specific
 * terms and conditions on the use of this software, and must be
 * distributed with any copies.  This file may be redistributed.  This
 * copyright and notice must be preserved in all copies made of this file.
 */

/*
 * Interface routines for Utah Raster Toolkit
 */

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"
#include "rle.h"

/*
 * Globals are stored in a structure.
 */

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

    Globals.width = width;
    Globals.height = height;
    if (fd == 0) {
	rleFile = stdin;
    } else {
	rleFile = fdopen(fd, "r");
    }
    rle_dflt_hdr.rle_file = rleFile;
    rle_get_setup(&rle_dflt_hdr);

    if (rle_dflt_hdr.xmax > width) {
	fprintf(stderr, "Warning: RLE width (%d) exceeds maximum (%d)\n", 
	    rle_dflt_hdr.xmax, width);
    }
    if (rle_dflt_hdr.ymax > height) {
	fprintf(stderr, "Warning: RLE height (%d) exceeds maximum (%d)\n", 
	    rle_dflt_hdr.ymax, height);
    }
    Globals.row = 0;
    Globals.scan = (unsigned char **) malloc((rle_dflt_hdr.ncolors +
				      rle_dflt_hdr.alpha) *
				     sizeof(unsigned char *));
    for (i = 0; i < rle_dflt_hdr.ncolors + rle_dflt_hdr.alpha; i++)
	Globals.scan[i] = 
	    (unsigned char *)malloc(rle_dflt_hdr.xmax+1);

    if (rle_dflt_hdr.alpha) {
	Globals.scan++;
    }
}

void
rasterRowGet(red, green, blue)
unsigned char *red, *green, *blue;
{
    int             i, max;

    if (Globals.row < rle_dflt_hdr.ymin || Globals.row > rle_dflt_hdr.ymax) {
	for (i = 0; i < Globals.width; i++) {
	    red[i] = 0;
	    green[i] = 0;
	    blue[i] = 0;
	}
    } else {
	rle_getrow(&rle_dflt_hdr, Globals.scan);
	max = rle_dflt_hdr.xmax < Globals.width ?
	    rle_dflt_hdr.xmax : Globals.width;
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
    rle_puteof(&rle_dflt_hdr);
}
