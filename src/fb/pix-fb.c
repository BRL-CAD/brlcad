/*                        P I X - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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

#include <limits.h>
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

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/file.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/snooze.h"
#include "vmath.h"
#include "dm.h"

static unsigned char *scanline;	/* 1 scanline pixel buffer */
static int scanbytes;		/* # of bytes of scanline */
static int scanpix;		/* # of pixels of scanline */

static int multiple_lines = 0;	/* Streamlined operation */

static const char *framebuffer = NULL;
static const char *file_name;
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

struct pix_fb_args {
    struct pix_fb_args *self;
    int help;
    int one_line_only;
    int multiple_lines;
    int autosize;
    int inverse;
    int clear;
    int zoom;
    const char *framebuffer;
    int pause_sec;
    size_t file_width;
    size_t file_height;
    int file_xoff;
    int file_yoff;
    int screen_xoff;
    int screen_yoff;
    int screen_width;
    int screen_height;
};

enum pix_fb_dimension {
    PIX_FB_FILE_SIZE,
    PIX_FB_FILE_WIDTH,
    PIX_FB_FILE_HEIGHT,
    PIX_FB_SCREEN_SIZE
};

static struct pix_fb_args *
pix_fb_args_from_storage(void *storage)
{
    return storage ? *(struct pix_fb_args **)storage : NULL;
}

static int
pix_fb_autosize(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    struct pix_fb_args *args = pix_fb_args_from_storage(storage);
    if (args)
	args->autosize = 1;
    return 0;
}

static int
pix_fb_dimension_value(struct bu_vls *msg, const char *arg, void *storage,
	enum pix_fb_dimension dimension)
{
    struct pix_fb_args *args = pix_fb_args_from_storage(storage);

    if (dimension == PIX_FB_SCREEN_SIZE) {
	int value;
	if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 0, INT_MAX))
	    return -1;
	if (args)
	    args->screen_width = args->screen_height = value;
	return 0;
    }

    size_t value;
    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, SIZE_MAX))
	return -1;
    if (args) {
	if (dimension == PIX_FB_FILE_SIZE)
	    args->file_width = args->file_height = value;
	else if (dimension == PIX_FB_FILE_WIDTH)
	    args->file_width = value;
	else
	    args->file_height = value;
	args->autosize = 0;
    }
    return 0;
}

#define PIX_FB_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return pix_fb_dimension_value(msg, arg, storage, _dimension); \
    }

PIX_FB_DIMENSION_CALLBACK(pix_fb_file_size, PIX_FB_FILE_SIZE)
PIX_FB_DIMENSION_CALLBACK(pix_fb_file_width, PIX_FB_FILE_WIDTH)
PIX_FB_DIMENSION_CALLBACK(pix_fb_file_height, PIX_FB_FILE_HEIGHT)
PIX_FB_DIMENSION_CALLBACK(pix_fb_screen_size, PIX_FB_SCREEN_SIZE)

static const struct bu_arg_option pix_fb_options[] = {
    BU_ARG_FLAG("h", "help", struct pix_fb_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("1", "one-line", struct pix_fb_args, one_line_only,
	"Force one scanline per framebuffer write"),
    BU_ARG_INTEGER_RANGE("m", "multiple-lines", struct pix_fb_args,
	multiple_lines, 1, 0, 1, INT_MAX, "scanlines",
	"Write blocks containing this many scanlines"),
    BU_ARG_CUSTOM_FLAG("a", "autosize", "autosize", struct pix_fb_args,
	self, pix_fb_autosize, "Derive dimensions from a named input file"),
    BU_ARG_FLAG("i", "invert", struct pix_fb_args, inverse,
	"Display scanlines from top to bottom"),
    BU_ARG_FLAG("c", "clear", struct pix_fb_args, clear,
	"Clear the framebuffer before writing"),
    BU_ARG_FLAG("z", "zoom", struct pix_fb_args, zoom,
	"Zoom and center the displayed image"),
    BU_ARG_VALUE("F", "framebuffer", struct pix_fb_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer instead of the FB_FILE environment default"),
    BU_ARG_INTEGER_RANGE("p", "pause", struct pix_fb_args, pause_sec,
	1, 0, 1, INT_MAX, "seconds",
	"Wait before closing the framebuffer"),
    BU_ARG_CUSTOM("s", "input-size", struct pix_fb_args, self,
	pix_fb_file_size, "pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "input-width", struct pix_fb_args, self,
	pix_fb_file_width, "pixels", "Set the input scanline width"),
    BU_ARG_CUSTOM("n", "input-height", struct pix_fb_args, self,
	pix_fb_file_height, "scanlines", "Set the input image height"),
    BU_ARG_INTEGER_RANGE("x", "input-x-offset", struct pix_fb_args,
	file_xoff, 1, 0, 1, INT_MAX, "pixels",
	"Skip pixels from the left of each input scanline"),
    BU_ARG_INTEGER_RANGE("y", "input-y-offset", struct pix_fb_args,
	file_yoff, 1, 0, 1, INT_MAX, "scanlines",
	"Skip scanlines from the bottom of the input"),
    BU_ARG_VALUE("X", "screen-x-offset", struct pix_fb_args, screen_xoff,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "pixels",
	"Offset the image horizontally on the framebuffer"),
    BU_ARG_VALUE("Y", "screen-y-offset", struct pix_fb_args, screen_yoff,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "scanlines",
	"Offset the image vertically on the framebuffer"),
    BU_ARG_CUSTOM("S", "framebuffer-size", struct pix_fb_args, self,
	pix_fb_screen_size, "pixels", "Set both framebuffer dimensions"),
    BU_ARG_INTEGER_RANGE("W", "framebuffer-width", struct pix_fb_args,
	screen_width, 1, 0, 1, INT_MAX, "pixels",
	"Set the framebuffer width"),
    BU_ARG_INTEGER_RANGE("N", "framebuffer-height", struct pix_fb_args,
	screen_height, 1, 0, 1, INT_MAX, "scanlines",
	"Set the framebuffer height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pix_fb_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PIX input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pix_fb_schema = BU_ARG_SCHEMA(
    "pix-fb", "Display a BRL-CAD PIX image on a framebuffer", pix_fb_options,
    pix_fb_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pix_fb_grammar = {
    &pix_fb_schema, NULL, NULL
};

static int
pix_fb_help(FILE *stream, int status)
{
    char *schema_help = bu_arg_schema_help(&pix_fb_schema, "pix-fb");
    bu_flog(stream, "%s", schema_help ? schema_help : "");
    bu_free(schema_help, "pix-fb help");
    return status;
}


/*
 * Throw bytes away.  Use reads into scanline buffer if a pipe, else seek.
 */
int
skipbytes(int fd, b_off_t num)
{
    int n, tries;

    if (fileinput) {
	(void)bu_lseek(fd, num, 1);
	return 0;
    }

    while (num > 0) {
	tries = num > scanbytes ? scanbytes : num;
	n = read(fd, scanline, tries);
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
    struct pix_fb_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int y;
    struct fb *fbp;
    int xout, yout, n, m, xstart, xskip;
    int operand_index;
    args.self = &args;
    args.file_width = 512;
    args.file_height = 512;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &pix_fb_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&pix_fb_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return pix_fb_help(stdout, 0);

    if (argc - 1 - operand_index == 1) {
	char *canonical;
	file_name = argv[operand_index + 1];
	canonical = bu_file_realpath(file_name, NULL);
	infd = open(canonical, O_RDONLY|O_BINARY);
	if (infd < 0) {
	    bu_flog(stderr,
		"pix-fb: cannot open \"%s\" (canonical \"%s\") for reading\n",
		file_name, canonical);
	    bu_free(canonical, "canonical input path");
	    return 1;
	}
	bu_free(canonical, "canonical input path");
	fileinput = 1;
    } else {
	if (isatty(fileno(stdin))) {
	    bu_flog(stderr, "pix-fb requires an input file or redirected input\n");
	    return pix_fb_help(stderr, 1);
	}
	file_name = "-";
	infd = fileno(stdin);
	setmode(infd, O_BINARY);
    }

    one_line_only = args.one_line_only;
    multiple_lines = args.multiple_lines;
    autosize = args.autosize;
    inverse = args.inverse;
    clear = args.clear;
    zoom = args.zoom;
    framebuffer = args.framebuffer;
    pause_sec = args.pause_sec;
    file_width = args.file_width;
    file_height = args.file_height;
    file_xoff = args.file_xoff;
    file_yoff = args.file_yoff;
    scr_xoff = args.screen_xoff;
    scr_yoff = args.screen_yoff;
    scr_width = args.screen_width;
    scr_height = args.screen_height;

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
	&& scr_xoff >= 0
	&& scr_yoff >= 0
	&& (size_t)xout == file_width
	&& file_width <= (size_t)scr_width)
    {
	scanpix *= multiple_lines;
    } else {
	multiple_lines = 0;
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
	V_MIN(zoomit, scr_height/yout);

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

    if (file_yoff != 0) skipbytes(infd, (b_off_t)file_yoff*(b_off_t)file_width*sizeof(RGBpixel));

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
		skipbytes(infd, (b_off_t)file_width*sizeof(RGBpixel));
		continue;
	    }
	    if (file_xoff+xskip != 0)
		skipbytes(infd, (b_off_t)(file_xoff+xskip)*sizeof(RGBpixel));
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
		skipbytes(infd, (b_off_t)(file_width-file_xoff-xskip-scanpix)*sizeof(RGBpixel));
	}
    } else {
	/* Inverse -- top to bottom */
	for (y = scr_height-1-scr_yoff; y >= scr_height-scr_yoff-yout; y--) {
	    if (y < 0 || y >= scr_height) {
		skipbytes(infd, (b_off_t)file_width*sizeof(RGBpixel));
		continue;
	    }
	    if (file_xoff+xskip != 0)
		skipbytes(infd, (b_off_t)(file_xoff+xskip)*sizeof(RGBpixel));
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
		skipbytes(infd, (b_off_t)(file_width-file_xoff-xskip-scanpix)*sizeof(RGBpixel));
	}
    }
    bu_snooze(BU_SEC2USEC(pause_sec));
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
