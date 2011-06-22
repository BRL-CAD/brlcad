/*                       P I X - R L E . C
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
/** @file util/pix-rle.c
 *
 * Encode a .pix file using the Utah Raster Toolkit RLE library
 *
 */

#include "common.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "fb.h"

/* 
 * system installed RLE reports a re-define, so undef it to quell the
 * warning
 */
#include "rle.h"


static rle_hdr outrle;
#define outfp outrle.rle_file
static char comment[128];
#if HAVE_GETHOSTNAME
static char host[128];
#endif
static rle_pixel **rows;
static time_t now;
static char *who;

static FILE *infp;
static char *infile;

static int background[3];

static size_t file_width = 512;
static size_t file_height = 512;

static char usage[] = "\
Usage: pix-rle [-h] [-s squarefilesize]  [-C r/g/b]\n\
	[-w file_width] [-n file_height] [file.pix] [file.rle]\n\
\n\
If omitted, the .pix file is taken from stdin\n\
and the .rle file is written to stdout\n";


/*
 * G E T _ A R G S
 */
static int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "hs:w:n:C:")) != -1) {
	switch (c) {
	    case 'h':
		/* high-res */
		file_height = file_width = 1024;
		break;
	    case 's':
		/* square file size */
		file_height = file_width = atoi(bu_optarg);
		break;
	    case 'w':
		file_width = atoi(bu_optarg);
		break;
	    case 'n':
		file_height = atoi(bu_optarg);
		break;
	    case 'C':
		{
		    char *cp = bu_optarg;
		    int *conp = background;

		    /* premature null => atoi gives zeros */
		    for (c=0; c < 3; c++) {
			*conp++ = atoi(cp);
			while (*cp && *cp++ != '/')
			    ;
		    }
		}
		break;
	    default:
	    case '?':
		return 0;
	}
    }
    if (argv[bu_optind] != NULL) {
	if ((infp = fopen((infile=argv[bu_optind]), "r")) == NULL) {
	    perror(infile);
	    return 0;
	}
	bu_optind++;
    } else {
	infile = "-";
    }
    if (argv[bu_optind] != NULL) {
	if (bu_file_exists(argv[bu_optind])) {
	    (void) fprintf(stderr,
			   "\"%s\" already exists.\n",
			   argv[bu_optind]);
	    bu_exit (1, NULL);
	}
	if ((outfp = fopen(argv[bu_optind], "w")) == NULL) {
	    perror(argv[bu_optind]);
	    return 0;
	}
    }
    if (argc > ++bu_optind)
	(void) fprintf(stderr, "pix-rle: Excess arguments ignored\n");

    if (isatty(fileno(infp)) || isatty(fileno(outfp)))
	return 0;
    return 1;
}


/*
 * M A I N
 */
int
main(int argc, char **argv)
{
    RGBpixel *scan_buf;
    size_t y;

    infp = stdin;
    outfp = stdout;
    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }
    scan_buf = (RGBpixel *)malloc(sizeof(RGBpixel) * file_width);

    outrle.ncolors = 3;
    RLE_SET_BIT(outrle, RLE_RED);
    RLE_SET_BIT(outrle, RLE_GREEN);
    RLE_SET_BIT(outrle, RLE_BLUE);
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
    rle_putcom(strdup(comment), &outrle);
    now = time(0);
    sprintf(comment, "converted_date=%24.24s", ctime(&now));
    rle_putcom(strdup(comment), &outrle);
    if ((who = getenv("USER")) != (char *)0) {
	snprintf(comment, 128, "converted_by=%s", who);
	rle_putcom(strdup(comment), &outrle);
    } else {
	if ((who = getenv("LOGNAME")) != (char *)0) {
	    snprintf(comment, 128, "converted_by=%s", who);
	    rle_putcom(strdup(comment), &outrle);
	}
    }
#ifdef HAVE_GETHOSTNAME
    gethostname(host, sizeof(host));
    snprintf(comment, 128, "converted_host=%s", host);
    rle_putcom(strdup(comment), &outrle);
#endif

    rle_put_setup(&outrle);
    rle_row_alloc(&outrle, &rows);

    /* Read image a scanline at a time, and encode it */
    for (y = 0; y < file_height; y++) {
	size_t ret = fread((char *)scan_buf, sizeof(RGBpixel), (size_t)file_width, infp);
	if (ret != file_width) {
	    (void) fprintf(stderr, "pix-rle: read of %lu pixels on line %lu failed!\n", (long unsigned)file_width, (long unsigned)y);
	    bu_exit (1, NULL);
	}

	/* Grumble, convert to Utah layout */
	{
	    unsigned char *pp = (unsigned char *)scan_buf;
	    rle_pixel *rp = rows[0];
	    rle_pixel *gp = rows[1];
	    rle_pixel *bp = rows[2];
	    size_t i;

	    for (i=0; i<file_width; i++) {
		*rp++ = *pp++;
		*gp++ = *pp++;
		*bp++ = *pp++;
	    }
	}
	rle_putrow(rows, file_width, &outrle);
    }
    rle_puteof(&outrle);

    fclose(infp);
    fclose(outfp);

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
