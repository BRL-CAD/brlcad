/*                       R L E - P I X . C
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
/** @file util/rle-pix.c
 *
 * Decode a Utah Raster Toolkit RLE image, and output as a pix(5)
 * file.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "fb.h"
#include "bu.h"

/* 
 * system installed RLE reports a re-define, so undef it to quell the
 * warning
 */
#include "rle.h"

static FILE *infp;
static char *infile;
static FILE *outfp;

static int background[3];
static int override_background;

unsigned char *rows[4];		/* Character pointers for rle_getrow */

static RGBpixel *scan_buf;		/* single scanline buffer */
static RGBpixel *bg_buf;		/* single scanline of background */
static ColorMap cmap;

static int screen_width = 0;
static int screen_height = 0;

static int crunch;
static int r_debug;
static int hflag;			/* print header only */

static char usage[] = "\
Usage: rle-pix [-c -d -h -H] [-C r/g/b]\n\
	[-s|S squareoutsize] [-w|W out_width] [-n|N out_height]\n\
	[file.rle [file.pix]]\n\
";

/*
 * G E T _ A R G S
 */
static int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "cdhHs:S:w:W:n:N:C:")) != -1) {
	switch (c) {
	    case 'd':
		r_debug = 1;
		break;
	    case 'c':
		crunch = 1;
		break;
	    case 'H':
		hflag = 1;
		break;
	    case 'h':
		/* high-res */
		screen_height = screen_width = 1024;
		break;
	    case 'S':
	    case 's':
		/* square screen size */
		screen_height = screen_width = atoi(bu_optarg);
		break;
	    case 'W':
	    case 'w':
		screen_width = atoi(bu_optarg);
		break;
	    case 'N':
	    case 'n':
		screen_height = atoi(bu_optarg);
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
		    override_background = 1;
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
	    bu_exit(1, "rle-pix: \"%s\" already exists.\n", argv[bu_optind]);
	}
	if ((outfp = fopen(argv[bu_optind], "w")) == NULL) {
	    perror(argv[bu_optind]);
	    return 0;
	}
    }
    if (argc > ++bu_optind)
	bu_log("rle-pix:  excess arguments ignored\n");

    if (isatty(fileno(infp)))
	return 0;
    if (!hflag && isatty(fileno(outfp)))
	return 0;
    return 1;				/* OK */
}


/*
 * M A I N
 */
int
main(int argc, char **argv)
{
    int i;
    int file_width;		/* unclipped width of rectangle */
    int file_skiplen;		/* # of pixels to skip on l.h.s. */
    int screen_xbase;		/* screen X of l.h.s. of rectangle */
    int screen_xlen;		/* clipped len of rectangle */
    int ncolors;
    size_t ret;

    infp = stdin;
    outfp = stdout;
    if (!get_args(argc, argv)) {
	bu_exit(1, "%s", usage);
    }

    rle_dflt_hdr.rle_file = infp;
    if (rle_get_setup(&rle_dflt_hdr) < 0) {
	bu_exit(1, "rle-pix: Error reading setup information\n");
    }

    if (r_debug) {
	bu_log("Image bounds\n\tmin %d %d\n\tmax %d %d\n",
	       rle_dflt_hdr.xmin, rle_dflt_hdr.ymin,
	       rle_dflt_hdr.xmax, rle_dflt_hdr.ymax);
	bu_log("%d color channels\n", rle_dflt_hdr.ncolors);
	bu_log("%d color map channels\n", rle_dflt_hdr.ncmap);
	if (rle_dflt_hdr.alpha)
	    bu_log("Alpha Channel present in input, ignored.\n");
	for (i=0; i < rle_dflt_hdr.ncolors; i++)
	    bu_log("Background channel %d = %d\n",
		   i, rle_dflt_hdr.bg_color[i]);
	rle_debug(1);
    }

    if (rle_dflt_hdr.ncmap == 0)
	crunch = 0;

    /* Only interested in R, G, & B */
    RLE_CLR_BIT(rle_dflt_hdr, RLE_ALPHA);
    for (i = 3; i < rle_dflt_hdr.ncolors; i++)
	RLE_CLR_BIT(rle_dflt_hdr, i);
    ncolors = rle_dflt_hdr.ncolors > 3 ? 3 : rle_dflt_hdr.ncolors;

    /* Optional background color override */
    if (override_background) {
	for (i=0; i<ncolors; i++)
	    rle_dflt_hdr.bg_color[i] = background[i];
    }

    file_width = rle_dflt_hdr.xmax - rle_dflt_hdr.xmin + 1;

    /* Default screen (output) size tracks input rectangle upper right corner */
    if (screen_width == 0) {
	screen_width = rle_dflt_hdr.xmax + 1;
    }
    if (screen_height == 0) {
	screen_height = rle_dflt_hdr.ymax + 1;
    }

    /* Report screen (output) size given image size & other options */
    if (hflag) {
	printf("-w%d -n%d\n",
	       screen_width, screen_height);
	return 0;
    }

    /* Discard any scanlines which exceed screen height */
    if (rle_dflt_hdr.ymax > screen_height-1)
	rle_dflt_hdr.ymax = screen_height-1;

    /* Clip left edge */
    screen_xbase = rle_dflt_hdr.xmin;
    screen_xlen = screen_width;
    file_skiplen = 0;
    if (screen_xbase < 0) {
	file_skiplen = -screen_xbase;
	screen_xbase = 0;
	screen_xlen -= file_skiplen;
    }
    /* Clip right edge */
    if (screen_xbase + screen_xlen > screen_width)
	screen_xlen = screen_width - screen_xbase;

    if (screen_xlen <= 0 || rle_dflt_hdr.ymin > screen_height || rle_dflt_hdr.ymax < 0) {
	bu_log("rle-pix:  Warning:  RLE image rectangle entirely off screen\n");
	goto done;
    }

    /* NOTE:  This code can't do repositioning very well.
     * background flooding on the edges is needed, at a minimum.
     */

    scan_buf = (RGBpixel *)bu_malloc(sizeof(RGBpixel) * screen_width, "scan_buf");
    bg_buf = (RGBpixel *)bu_malloc(sizeof(RGBpixel) * screen_width, "bg_buf");

    /* Fill in background buffer */
    if (!rle_dflt_hdr.bg_color) {
	(void)memset((char *)bg_buf, 0, sizeof(RGBpixel) * screen_width);
    } else {
	for (i=0; i<screen_xlen; i++) {
	    bg_buf[i][0] = rle_dflt_hdr.bg_color[0];
	    bg_buf[i][1] = rle_dflt_hdr.bg_color[1];
	    bg_buf[i][2] = rle_dflt_hdr.bg_color[2];
	}
    }

    for (i=0; i < ncolors; i++)
	rows[i] = (unsigned char *)bu_malloc((size_t)file_width, "row[]");
    for (; i < 3; i++)
	rows[i] = rows[0];	/* handle monochrome images */

    /*
     * Import Utah color map, converting to libfb format.
     * Check for old format color maps, where high 8 bits
     * were zero, and correct them.
     * XXX need to handle < 3 channels of color map, by replication.
     */
    if (crunch && rle_dflt_hdr.ncmap > 0) {
	int maplen = (1 << rle_dflt_hdr.cmaplen);
	int all = 0;
	for (i=0; i<256; i++) {
	    cmap.cm_red[i] = rle_dflt_hdr.cmap[i];
	    cmap.cm_green[i] = rle_dflt_hdr.cmap[i+maplen];
	    cmap.cm_blue[i] = rle_dflt_hdr.cmap[i+2*maplen];
	    all |= cmap.cm_red[i] | cmap.cm_green[i] |
		cmap.cm_blue[i];
	}
	if ((all & 0xFF00) == 0 && (all & 0x00FF) != 0) {
	    /* This is an old (Edition 2) color map.
	     * Correct by shifting it left 8 bits.
	     */
	    for (i=0; i<256; i++) {
		cmap.cm_red[i] <<= 8;
		cmap.cm_green[i] <<= 8;
		cmap.cm_blue[i] <<= 8;
	    }
	    bu_log("rle-pix: correcting for old style colormap\n");
	}
    }

    /* Handle any lines below zero in y.  Decode and discard. */
    for (i = rle_dflt_hdr.ymin; i < 0; i++)
	rle_getrow(&rle_dflt_hdr, rows);

    /* Background-fill any lines above 0, below ymin */
    for (i=0; i < rle_dflt_hdr.ymin; i++) {
	ret = fwrite((char *)bg_buf, sizeof(RGBpixel), (size_t)screen_xlen, outfp);
	if (ret == 0)
	    perror("fwrite");
    }

    for (; i <= rle_dflt_hdr.ymax; i++) {
	unsigned char *pp = (unsigned char *)scan_buf;
	rle_pixel *rp = &(rows[0][file_skiplen]);
	rle_pixel *gp = &(rows[1][file_skiplen]);
	rle_pixel *bp = &(rows[2][file_skiplen]);
	int j;

	rle_getrow(&rle_dflt_hdr, rows);

	/* Grumble, convert from Utah layout */
	if (!crunch) {
	    for (j = 0; j < screen_xlen; j++) {
		*pp++ = *rp++;
		*pp++ = *gp++;
		*pp++ = *bp++;
	    }
	} else {
	    for (j = 0; j < screen_xlen; j++) {
		*pp++ = cmap.cm_red[*rp++]>>8;
		*pp++ = cmap.cm_green[*gp++]>>8;
		*pp++ = cmap.cm_blue[*bp++]>>8;
	    }
	}
	ret = fwrite((char *)scan_buf, sizeof(RGBpixel), (size_t)screen_xlen, outfp);
	if (ret == 0)
	    perror("fwrite");
    }


    /* Background-fill any lines above ymax, below screen_height */
    for (; i < screen_height; i++) {
	ret = fwrite((char *)bg_buf, sizeof(RGBpixel), (size_t)screen_xlen, outfp);
	if (ret == 0)
	    perror("fwrite");
    }
 done:

    for (i=0; i < ncolors; i++)
	bu_free(rows[i], "row[]");
    bu_free(scan_buf, "scan_buf");
    bu_free(bg_buf, "bg_buf");

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
