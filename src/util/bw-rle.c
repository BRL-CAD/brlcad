/*                        B W - R L E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2016 United States Government as represented by
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
/** @file util/bw-rle.c
 *
 * Encode a .bw file using the Utah Raster Toolkit RLE library
 *
 */

#include "common.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "bu/log.h"
#include "bu/str.h"

#include "fb.h"

/*
 * system installed RLE reports a re-define, so undef it to quell the
 * warning
 */
#include "rle.h"

#include "./rle_args.h"

static rle_hdr outrle;

static char comment[128];
#if HAVE_GETHOSTNAME
static char host[128];
#endif
static rle_pixel **rows;
static time_t now;
static char *who;

static FILE *infp = NULL;
static char *infile = NULL;

static int background[3] = {0, 0, 0};

static size_t file_width = 512;
static size_t file_height = 512;

static char usage[] = "\
Usage: bw-rle [-s squarefilesize]  [-C bg]\n\
	[-w file_width] [-n file_height] [file.bw] [file.rle]\n\
\n\
If omitted, the .bw file is taken from stdin\n\
and the .rle file is written to stdout\n";

int
main(int argc, char **argv)
{
    unsigned char *scan_buf;
    size_t y;

    infp = stdin;
    outrle.rle_file = stdout;
    if (!get_args(argc, argv, &outrle, &infp, &infile, (int **)&background, &file_width, &file_height)) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }
    scan_buf = (unsigned char *)malloc(sizeof(unsigned char) * file_width);

    outrle.ncolors = 1;
    RLE_SET_BIT(outrle, RLE_RED);
    outrle.background = 2;		/* use background */
    outrle.bg_color = background;
    outrle.alpha = 0;		/* no alpha channel */
    outrle.ncmap = 0;		/* no color map */
    outrle.cmaplen = 0;
    outrle.cmap = (rle_map *)0;
    outrle.xmin = outrle.ymin = 0;
    outrle.xmax = file_width-1;
    outrle.ymax = file_height-1;
    outrle.comments = (const char **)0;

    /* Add comments to the header file, since we have one */
    snprintf(comment, 128, "converted_from=%s", infile);
    rle_putcom(bu_strdup(comment), &outrle);
    now = time(0);
    sprintf(comment, "converted_date=%24.24s", ctime(&now));
    rle_putcom(bu_strdup(comment), &outrle);
    if ((who = getenv("USER")) != (char *)0) {
	snprintf(comment, 128, "converted_by=%s", who);
	rle_putcom(bu_strdup(comment), &outrle);
    } else {
	if ((who = getenv("LOGNAME")) != (char *)0) {
	    snprintf(comment, 128, "converted_by=%s", who);
	    rle_putcom(bu_strdup(comment), &outrle);
	}
    }
#if HAVE_GETHOSTNAME
    gethostname(host, sizeof(host));
    snprintf(comment, 128, "converted_host=%s", host);
    rle_putcom(bu_strdup(comment), &outrle);
#endif

    rle_put_setup(&outrle);
    rle_row_alloc(&outrle, &rows);

    /* Read image a scanline at a time, and encode it */
    for (y = 0; y < file_height; y++) {
	if (fread((char *)scan_buf, sizeof(unsigned char), (size_t)file_width, infp) != file_width) {
	    (void) fprintf(stderr,
			   "bw-rle: read of %lu pixels on line %lu failed!\n",
			   (unsigned long)file_width, (unsigned long)y);
	    bu_exit (1, NULL);
	}

	/* Grumble, convert to Utah layout */
	{
	    unsigned char *pp = scan_buf;
	    rle_pixel *rp = rows[0];
	    size_t i;

	    for (i=0; i<file_width; i++) {
		*rp++ = *pp++;
	    }
	}
	rle_putrow(rows, file_width, &outrle);
    }
    rle_puteof(&outrle);

    fclose(infp);
    fclose(outrle.rle_file);

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
