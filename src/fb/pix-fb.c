/*                        P I X - F B . C
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
 *
 */
/** @file pix-fb.c
 *
 * Program to take bottom-up pixel files and send them to a framebuffer.
 *
 */

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_WINSOCK_H
#  include <winsock.h>
#endif
#include "bio.h"

#include "bu.h"
#include "fb.h"

#include "pkg.h"


static unsigned char *scanline;	/* 1 scanline pixel buffer */
static int scanbytes;		/* # of bytes of scanline */
static int scanpix;		/* # of pixels of scanline */

static int multiple_lines = 0;	/* Streamlined operation */

static char *framebuffer = NULL;
static char *file_name;
static int infd;

static int fileinput = 0;	/* file of pipe on input? */
static int autosize = 0;	/* !0 to autosize input */

static size_t file_width = 512;	/* default input width */
static size_t file_height = 512;/* default input height */
static int scr_width = 0;	/* screen tracks file if not given */
static int scr_height = 0;
static int file_xoff, file_yoff;
static int scr_xoff, scr_yoff;
static int clear = 0;
static int zoom = 0;
static int inverse = 0;		/* Draw upside-down */
static int one_line_only = 0;	/* insist on 1-line writes */
static int pause_sec = 0; 	/* Pause that many seconds before
				   closing the FB and exiting */

static char usage[] = "\
Usage: pix-fb [-a -h -i -c -z -1] [-m #lines] [-F framebuffer]\n\
	[-s squarefilesize] [-w file_width] [-n file_height]\n\
	[-x file_xoff] [-y file_yoff] [-X scr_xoff] [-Y scr_yoff]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height] [-p seconds]\n\
	[file.pix]\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "1m:ahiczF:p:s:w:n:x:y:X:Y:S:W:N:")) != -1) {
	switch (c) {
	    case '1':
		one_line_only = 1;
		break;
	    case 'm':
		multiple_lines = atoi(bu_optarg);
		break;
	    case 'a':
		autosize = 1;
		break;
	    case 'h':
		/* high-res */
		file_height = file_width = 1024;
		scr_height = scr_width = 1024;
		autosize = 0;
		break;
	    case 'i':
		inverse = 1;
		break;
	    case 'c':
		clear = 1;
		break;
	    case 'z':
		zoom = 1;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 's':
		/* square file size */
		file_height = file_width = atoi(bu_optarg);
		autosize = 0;
		break;
	    case 'w':
		file_width = atoi(bu_optarg);
		autosize = 0;
		break;
	    case 'n':
		file_height = atoi(bu_optarg);
		autosize = 0;
		break;
	    case 'x':
		file_xoff = atoi(bu_optarg);
		break;
	    case 'y':
		file_yoff = atoi(bu_optarg);
		break;
	    case 'X':
		scr_xoff = atoi(bu_optarg);
		break;
	    case 'Y':
		scr_yoff = atoi(bu_optarg);
		break;
	    case 'S':
		scr_height = scr_width = atoi(bu_optarg);
		break;
	    case 'W':
		scr_width = atoi(bu_optarg);
		break;
	    case 'N':
		scr_height = atoi(bu_optarg);
		break;
	    case 'p':
		pause_sec=atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "-";
	infd = 0;
    } else {
	file_name = argv[bu_optind];
	if ((infd = open(file_name, 0)) < 0) {
	    perror(file_name);
	    (void)fprintf(stderr,
			  "pix-fb: cannot open \"%s\" for reading\n",
			  file_name);
	    bu_exit(1, NULL);
	}
#ifdef _WIN32
	setmode(infd, O_BINARY);
#endif
	fileinput++;
    }

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "pix-fb: excess argument(s) ignored\n");

    return 1;		/* OK */
}


/*
 * Throw bytes away.  Use reads into scanline buffer if a pipe, else seek.
 */
int
skipbytes(int fd, off_t num)
{
    int n, try;

    if (fileinput) {
	(void)lseek(fd, (off_t)num, 1);
	return 0;
    }

    while (num > 0) {
	try = num > scanbytes ? scanbytes : num;
	n = read(fd, scanline, try);
	if (n <= 0) {
	    return -1;
	}
	num -= n;
    }
    return 0;
}


int
main(int argc, char **argv)
{
    int y;
    FBIO *fbp;
    int xout, yout, n, m, xstart, xskip;

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    /* autosize input? */
    if (fileinput && autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, file_name, 3)) {
	    file_width = w;
	    file_height = h;
	} else {
	    fprintf(stderr, "pix-fb: unable to autosize\n");
	}
    }

    /* If screen size was not set, track the file size */
    if (scr_width == 0)
	scr_width = file_width;
    if (scr_height == 0)
	scr_height = file_height;

    if ((fbp = fb_open(framebuffer, scr_width, scr_height)) == NULL) {
	bu_exit(12, NULL);
    }

    /* Get the screen size we were given */
    scr_width = fb_getwidth(fbp);
    scr_height = fb_getheight(fbp);

    /* compute number of pixels to be output to screen */
    if (scr_xoff < 0) {
	xout = scr_width + scr_xoff;
	xskip = (-scr_xoff);
	xstart = 0;
    } else {
	xout = scr_width - scr_xoff;
	xskip = 0;
	xstart = scr_xoff;
    }

    if (xout < 0)
	bu_exit(0, NULL);			/* off screen */
    if ((size_t)xout > (file_width-file_xoff))
	xout = (file_width-file_xoff);
    scanpix = xout;				/* # pixels on scanline */

    if (inverse)
	scr_yoff = (-scr_yoff);

    yout = scr_height - scr_yoff;
    if (yout < 0)
	bu_exit(0, NULL);			/* off screen */
    if ((size_t)yout > (file_height-file_yoff))
	yout = (file_height-file_yoff);

    /* Only in the simplest case use multi-line writes */
    if (!one_line_only
	&& multiple_lines > 0
	&& !inverse
	&& !zoom
	&& (size_t)xout == file_width
	&& file_width <= (size_t)scr_width)
    {
	scanpix *= multiple_lines;
    }

    scanbytes = scanpix * sizeof(RGBpixel);
    if ((scanline = (unsigned char *)malloc(scanbytes)) == RGBPIXEL_NULL) {
	fprintf(stderr,
		"pix-fb:  malloc(%d) failure for scanline buffer\n",
		scanbytes);
	bu_exit(2, NULL);
    }

    if (clear) {
	fb_clear(fbp, PIXEL_NULL);
    }
    if (zoom) {
	/* Zoom in, and center the display.  Use square zoom. */
	int zoomit;
	zoomit = scr_width/xout;
	if (scr_height/yout < zoomit) zoomit = scr_height/yout;
	if (inverse) {
	    fb_view(fbp,
		    scr_xoff+xout/2, scr_height-1-(scr_yoff+yout/2),
		    zoomit, zoomit);
	} else {
	    fb_view(fbp,
		    scr_xoff+xout/2, scr_yoff+yout/2,
		    zoomit, zoomit);
	}
    }

    if (file_yoff != 0) skipbytes(infd, (off_t)file_yoff*(off_t)file_width*sizeof(RGBpixel));

    if (multiple_lines) {
	/* Bottom to top with multi-line reads & writes */
	unsigned long height;
	for (y = scr_yoff; y < scr_yoff + yout; y += multiple_lines) {
	    n = bu_mread(infd, (char *)scanline, scanbytes);
	    if (n <= 0) break;
	    height = multiple_lines;
	    if (n != scanbytes) {
		height = (n/sizeof(RGBpixel)+xout-1)/xout;
		if (height <= 0) break;
	    }
	    /* Don't over-write */
	    if ((size_t)(y + height) > (size_t)(scr_yoff + yout))
		height = scr_yoff + yout - y;
	    if (height <= 0) break;
	    m = fb_writerect(fbp, scr_xoff, y,
			     file_width, height,
			     scanline);
	    if ((size_t)m != file_width*height) {
		fprintf(stderr,
			"pix-fb: fb_writerect(x=%d, y=%d, w=%lu, h=%lu) failure, ret=%d, s/b=%d\n",
			scr_xoff, y,
			(unsigned long)file_width, height, m, scanbytes);
	    }
	}
    } else if (!inverse) {
	/* Normal way -- bottom to top */
	for (y = scr_yoff; y < scr_yoff + yout; y++) {
	    if (y < 0 || y > scr_height) {
		skipbytes(infd, (off_t)file_width*sizeof(RGBpixel));
		continue;
	    }
	    if (file_xoff+xskip != 0)
		skipbytes(infd, (off_t)(file_xoff+xskip)*sizeof(RGBpixel));
	    n = bu_mread(infd, (char *)scanline, scanbytes);
	    if (n <= 0) break;
	    m = fb_write(fbp, xstart, y, scanline, xout);
	    if (m != xout) {
		fprintf(stderr,
			"pix-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
			scr_xoff, y, xout,
			m, xout);
	    }
	    /* slop at the end of the line? */
	    if ((size_t)file_xoff+xskip+scanpix < file_width)
		skipbytes(infd, (off_t)(file_width-file_xoff-xskip-scanpix)*sizeof(RGBpixel));
	}
    } else {
	/* Inverse -- top to bottom */
	for (y = scr_height-1-scr_yoff; y >= scr_height-scr_yoff-yout; y--) {
	    if (y < 0 || y >= scr_height) {
		skipbytes(infd, (off_t)file_width*sizeof(RGBpixel));
		continue;
	    }
	    if (file_xoff+xskip != 0)
		skipbytes(infd, (off_t)(file_xoff+xskip)*sizeof(RGBpixel));
	    n = bu_mread(infd, (char *)scanline, scanbytes);
	    if (n <= 0) break;
	    m = fb_write(fbp, xstart, y, scanline, xout);
	    if (m != xout) {
		fprintf(stderr,
			"pix-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
			scr_xoff, y, xout,
			m, xout);
	    }
	    /* slop at the end of the line? */
	    if ((size_t)file_xoff+xskip+scanpix < file_width)
		skipbytes(infd, (off_t)(file_width-file_xoff-xskip-scanpix)*sizeof(RGBpixel));
	}
    }
    sleep(pause_sec);
    if (fb_close(fbp) < 0) {
	fprintf(stderr, "pix-fb: Warning: fb_close() error\n");
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
