/*                        F B - R L E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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
/** @file fb-rle.c
 *
 * Encode a framebuffer image using the Utah Raster Toolkit RLE
 * library
 *
 */

#include "common.h"

#include <time.h>
#include "bio.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "bu.h"
#include "fb.h"
#include "rle.h"


#define COMMENT_SIZE 512

static rle_hdr outrle;
static char comment[COMMENT_SIZE];

#ifdef HAVE_GETHOSTNAME
static char host[COMMENT_SIZE];
#endif

static rle_pixel **rows;
static time_t now;
static char *who;

static rle_map rlemap[3*256];		/* Utah format map */
static ColorMap cmap;			/* libfb format map */

static int crunch;

static int background[3];	/* default background is black */

static int screen_width;
static int screen_height;
static int file_width;
static int file_height;
static int screen_xoff;
static int screen_yoff;

static char *framebuffer;

static char usage[] = "\
Usage: fb-rle [-c -d] [-F framebuffer] [-C r/g/b]\n\
	[-S squarescrsize] [-W screen_width] [-N screen_height]\n\
	[-X screen_xoff] [-Y screen_yoff]\n\
	[-s squarefilesize] [-w file_width] [-n file_height]\n\
	[file.rle]\n\
\n\
If omitted, the .rle file is written to stdout\n";

/* in cmap-crunch.c */
extern void cmap_crunch(RGBpixel (*scan_buf), int pixel_ct, ColorMap *colormap);


/*
 * G E T _ A R G S
 */
static int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "cF:ds:w:n:S:W:N:X:Y:C:h?")) != -1) {
	switch (c) {
	    case 'c':
		crunch = 1;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 's':
		/* square file size */
		file_height = file_width = atoi(bu_optarg);
		break;
	    case 'S':
		screen_height = screen_width = atoi(bu_optarg);
		break;
	    case 'w':
		file_width = atoi(bu_optarg);
		break;
	    case 'W':
		screen_width = atoi(bu_optarg);
		break;
	    case 'n':
		file_height = atoi(bu_optarg);
		break;
	    case 'N':
		screen_height = atoi(bu_optarg);
		break;
	    case 'X':
		screen_xoff = atoi(bu_optarg);
		break;
	    case 'Y':
		screen_yoff = atoi(bu_optarg);
		break;
	    case 'C': {
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
		return 0;
	}
    }
    if (argv[bu_optind] != NULL) {
	if (bu_file_exists(argv[bu_optind], NULL)) {
	    (void) fprintf(stderr,
			   "\"%s\" already exists.\n",
			   argv[bu_optind]);
	    bu_exit(1, NULL);
	}
	if ((outrle.rle_file = fopen(argv[bu_optind], "wb")) == NULL) {
	    perror(argv[bu_optind]);
	    return 0;
	}
    }
    if (argc > ++bu_optind)
	(void) fprintf(stderr, "fb-rle: Excess arguments ignored\n");

    if (isatty(fileno(outrle.rle_file)))
	return 0;
    return 1;
}


/*
 * M A I N
 */
int
main(int argc, char **argv)
{
    FBIO *fbp;
    unsigned char *scan_buf;
    int y;
    int cm_save_needed;

    outrle.rle_file = stdout;
    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    /* If screen size = default & file size is given, track file size */
    if (screen_width == 0 && file_width > 0)
	screen_width = file_width;
    if (screen_height == 0 && file_height > 0)
	screen_height = file_height;

    if ((fbp = fb_open(framebuffer, screen_width, screen_height)) == FBIO_NULL)
	bu_exit(12, NULL);

    /* Honor original screen size desires, if set, unless they shrank */
    if (screen_width == 0 || fb_getwidth(fbp) < screen_width)
	screen_width = fb_getwidth(fbp);
    if (screen_height == 0 || fb_getheight(fbp) < screen_height)
	screen_height = fb_getheight(fbp);

    /* If not specified, output file size tracks screen size */
    if (file_width == 0)
	file_width = screen_width;
    if (file_height == 0)
	file_height = screen_height;

    /* Clip below and to left of (0, 0) */
    if (screen_xoff < 0) {
	file_width += screen_xoff;
	screen_xoff = 0;
    }
    if (screen_yoff < 0) {
	file_height += screen_yoff;
	screen_yoff = 0;
    }

    /* Clip up and to the right */
    if (screen_xoff + file_width > screen_width)
	file_width = screen_width - screen_xoff;
    if (screen_yoff + file_height > screen_height)
	file_height = screen_height - screen_yoff;

    if (file_width <= 0 || file_height <= 0) {
	fprintf(stderr,
		"fb-rle: Error: image rectangle entirely off screen\n");
	bu_exit(1, NULL);
    }

    /* Read color map, see if it is linear */
    cm_save_needed = 1;
    if (fb_rmap(fbp, &cmap) == -1)
	cm_save_needed = 0;
    if (cm_save_needed && fb_is_linear_cmap(&cmap))
	cm_save_needed = 0;
    if (crunch && (cm_save_needed == 0))
	crunch = 0;

    /* Convert to Utah format */
    if (cm_save_needed) for (y=0; y<256; y++) {
	    rlemap[y+0*256] = cmap.cm_red[y];
	    rlemap[y+1*256] = cmap.cm_green[y];
	    rlemap[y+2*256] = cmap.cm_blue[y];
	}

    scan_buf = (unsigned char *)malloc(sizeof(RGBpixel) * screen_width);

    /* Build RLE header */
    outrle.ncolors = 3;
    RLE_SET_BIT(outrle, RLE_RED);
    RLE_SET_BIT(outrle, RLE_GREEN);
    RLE_SET_BIT(outrle, RLE_BLUE);
    outrle.background = 2;		/* use background */
    outrle.bg_color = background;
    outrle.alpha = 0;			/* no alpha channel */
    if (cm_save_needed && !crunch) {
	outrle.ncmap = 3;
	outrle.cmaplen = 8;		/* 1<<8 = 256 */
	outrle.cmap = rlemap;
    } else {
	outrle.ncmap = 0;		/* no color map */
	outrle.cmaplen = 0;
	outrle.cmap = (rle_map *)0;
    }
    outrle.xmin = screen_xoff;
    outrle.ymin = screen_yoff;
    outrle.xmax = screen_xoff + file_width - 1;
    outrle.ymax = screen_yoff + file_height - 1;
    outrle.comments = (const char **)0;

    /* Add comments to the header file, since we have one */
    if (framebuffer == (char *)0)
	framebuffer = fbp->if_name;
    snprintf(comment, COMMENT_SIZE, "encoded_from=%s", framebuffer);
    rle_putcom(bu_strdup(comment), &outrle);
    now = time(0);
    snprintf(comment, COMMENT_SIZE, "encoded_date=%24.24s", ctime(&now));
    rle_putcom(bu_strdup(comment), &outrle);
    if ((who = getenv("USER")) != (char *)0) {
	snprintf(comment, COMMENT_SIZE, "encoded_by=%s", who);
	rle_putcom(bu_strdup(comment), &outrle);
    }
# if HAVE_GETHOSTNAME
    gethostname(host, sizeof(host));
    snprintf(comment, COMMENT_SIZE, "encoded_host=%s", host);
    rle_putcom(bu_strdup(comment), &outrle);
# endif

    rle_put_setup(&outrle);
    rle_row_alloc(&outrle, &rows);

    /* Read the image a scanline at a time, and encode it */
    for (y = 0; y < file_height; y++) {
	if (fb_read(fbp, screen_xoff, y+screen_yoff, scan_buf,
		    file_width) == -1) {
	    (void) fprintf(stderr,
			   "fb-rle: read of %d pixels on line %d failed!\n",
			   file_width, y+screen_yoff);
	    bu_exit(1, NULL);
	}

	if (crunch)
	    cmap_crunch((RGBpixel *)scan_buf, file_width, &cmap);

	/* Grumble, convert to Utah layout */
	{
	    unsigned char *pp = (unsigned char *)scan_buf;
	    rle_pixel *rp = rows[0];
	    rle_pixel *gp = rows[1];
	    rle_pixel *bp = rows[2];
	    int i;

	    for (i=0; i<file_width; i++) {
		*rp++ = *pp++;
		*gp++ = *pp++;
		*bp++ = *pp++;
	    }
	}
	rle_putrow(rows, file_width, &outrle);
    }
    rle_puteof(&outrle);

    fb_close(fbp);
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
