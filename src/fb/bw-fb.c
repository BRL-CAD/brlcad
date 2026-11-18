/*                         B W - F B . C
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
 */
/** @file bw-fb.c
 *
 * Write a black and white (.bw) image to the framebuffer.
 * From an 8-bit/pixel, pix order file (i.e. Bottom UP, left to right).
 *
 * This allows an offset into both the display and source file.
 * The color planes to be loaded are also selectable.
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

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/malloc.h"
#include "bu/file.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "vmath.h"
#include "dm.h"


int skipbytes(int fd, b_off_t num);

#define MAX_LINE (16*1024)	/* Largest output scan line length */

static char ibuf[MAX_LINE];
static RGBpixel obuf[MAX_LINE];

static int fileinput = 0;		/* file of pipe on input? */
static int autosize = 0;		/* !0 to autosize input */

static size_t file_width = 512;	/* default input width */
static size_t file_height = 512;	/* default input height */
static int scr_width = 0;		/* screen tracks file if not given */
static int scr_height = 0;
static int file_xoff, file_yoff;
static int scr_xoff, scr_yoff;
static int clear = 0;
static int zoom = 0;
static int inverse = 0;
static int redflag   = 0;
static int greenflag = 0;
static int blueflag  = 0;

static const char *framebuffer = NULL;
static const char *file_name;
static int infd;
static struct fb *fbp;

struct bw_fb_args {
    struct bw_fb_args *self;
    int help;
    int autosize;
    int inverse;
    int clear;
    int zoom;
    int red;
    int green;
    int blue;
    const char *framebuffer;
    size_t file_width;
    size_t file_height;
    int file_xoff;
    int file_yoff;
    int screen_width;
    int screen_height;
    int screen_xoff;
    int screen_yoff;
};

enum bw_fb_dimension {
    BW_FB_FILE_SIZE,
    BW_FB_FILE_WIDTH,
    BW_FB_FILE_HEIGHT,
    BW_FB_SCREEN_SIZE
};

static struct bw_fb_args *
bw_fb_args_from_storage(void *storage)
{
    return storage ? *(struct bw_fb_args **)storage : NULL;
}

static int
bw_fb_autosize(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    struct bw_fb_args *args = bw_fb_args_from_storage(storage);
    if (args)
	args->autosize = 1;
    return 0;
}

static int
bw_fb_dimension_value(struct bu_vls *msg, const char *arg, void *storage,
	enum bw_fb_dimension dimension)
{
    struct bw_fb_args *args = bw_fb_args_from_storage(storage);

    if (dimension == BW_FB_SCREEN_SIZE) {
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
	if (dimension == BW_FB_FILE_SIZE)
	    args->file_width = args->file_height = value;
	else if (dimension == BW_FB_FILE_WIDTH)
	    args->file_width = value;
	else
	    args->file_height = value;
	args->autosize = 0;
    }
    return 0;
}

#define BW_FB_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return bw_fb_dimension_value(msg, arg, storage, _dimension); \
    }

BW_FB_DIMENSION_CALLBACK(bw_fb_file_size, BW_FB_FILE_SIZE)
BW_FB_DIMENSION_CALLBACK(bw_fb_file_width, BW_FB_FILE_WIDTH)
BW_FB_DIMENSION_CALLBACK(bw_fb_file_height, BW_FB_FILE_HEIGHT)
BW_FB_DIMENSION_CALLBACK(bw_fb_screen_size, BW_FB_SCREEN_SIZE)

static const struct bu_arg_option bw_fb_options[] = {
    BU_ARG_FLAG("h", "help", struct bw_fb_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("a", "autosize", "autosize", struct bw_fb_args,
	self, bw_fb_autosize, "Derive dimensions from a named input file"),
    BU_ARG_FLAG("i", "invert", struct bw_fb_args, inverse,
	"Display scanlines from top to bottom"),
    BU_ARG_FLAG("c", "clear", struct bw_fb_args, clear,
	"Clear the framebuffer before writing"),
    BU_ARG_FLAG("z", "zoom", struct bw_fb_args, zoom,
	"Zoom and center the displayed image"),
    BU_ARG_FLAG("R", "red", struct bw_fb_args, red,
	"Write the BW values to the red plane"),
    BU_ARG_FLAG("G", "green", struct bw_fb_args, green,
	"Write the BW values to the green plane"),
    BU_ARG_FLAG("B", "blue", struct bw_fb_args, blue,
	"Write the BW values to the blue plane"),
    BU_ARG_VALUE("F", "framebuffer", struct bw_fb_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer instead of the FB_FILE environment default"),
    BU_ARG_CUSTOM("s", "input-size", struct bw_fb_args, self,
	bw_fb_file_size, "pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "input-width", struct bw_fb_args, self,
	bw_fb_file_width, "pixels", "Set the input scanline width"),
    BU_ARG_CUSTOM("n", "input-height", struct bw_fb_args, self,
	bw_fb_file_height, "scanlines", "Set the input image height"),
    BU_ARG_INTEGER_RANGE("x", "input-x-offset", struct bw_fb_args,
	file_xoff, 1, 0, 1, INT_MAX, "pixels",
	"Skip pixels from the left of each input scanline"),
    BU_ARG_INTEGER_RANGE("y", "input-y-offset", struct bw_fb_args,
	file_yoff, 1, 0, 1, INT_MAX, "scanlines",
	"Skip scanlines from the bottom of the input"),
    BU_ARG_VALUE("X", "screen-x-offset", struct bw_fb_args, screen_xoff,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "pixels",
	"Offset the image horizontally on the framebuffer"),
    BU_ARG_VALUE("Y", "screen-y-offset", struct bw_fb_args, screen_yoff,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "scanlines",
	"Offset the image vertically on the framebuffer"),
    BU_ARG_CUSTOM("S", "framebuffer-size", struct bw_fb_args, self,
	bw_fb_screen_size, "pixels", "Set both framebuffer dimensions"),
    BU_ARG_INTEGER_RANGE("W", "framebuffer-width", struct bw_fb_args,
	screen_width, 1, 0, 1, INT_MAX, "pixels",
	"Set the framebuffer width"),
    BU_ARG_INTEGER_RANGE("N", "framebuffer-height", struct bw_fb_args,
	screen_height, 1, 0, 1, INT_MAX, "scanlines",
	"Set the framebuffer height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bw_fb_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"BW input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bw_fb_schema = BU_ARG_SCHEMA(
    "bw-fb", "Display a BRL-CAD BW image on a framebuffer", bw_fb_options,
    bw_fb_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bw_fb_grammar = {
    &bw_fb_schema, NULL, NULL
};

static int
bw_fb_help(FILE *stream, int status)
{
    char *schema_help = bu_arg_schema_help(&bw_fb_schema, "bw-fb");
    bu_flog(stream, "%s", schema_help ? schema_help : "");
    bu_free(schema_help, "bw-fb help");
    return status;
}


int
main(int argc, char **argv)
{
    struct bw_fb_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int x=0, y=0, n=0;
    int operand_index;
    long xout=1, yout=1;		/* number of screen output lines */
    long xstart=0, xskip=0;
    args.self = &args;
    args.file_width = 512;
    args.file_height = 512;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &bw_fb_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&bw_fb_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return bw_fb_help(stdout, 0);

    if (argc - 1 - operand_index == 1) {
	char *canonical;
	file_name = argv[operand_index + 1];
	canonical = bu_file_realpath(file_name, NULL);
	infd = open(canonical, O_RDONLY|O_BINARY);
	if (infd < 0) {
	    bu_flog(stderr,
		"bw-fb: cannot open \"%s\" (canonical \"%s\") for reading\n",
		file_name, canonical);
	    bu_free(canonical, "canonical input path");
	    return 1;
	}
	bu_free(canonical, "canonical input path");
	fileinput = 1;
    } else {
	if (isatty(fileno(stdin))) {
	    bu_flog(stderr, "bw-fb requires an input file or redirected input\n");
	    return bw_fb_help(stderr, 1);
	}
	file_name = "-";
	infd = fileno(stdin);
	setmode(infd, O_BINARY);
    }

    autosize = args.autosize;
    inverse = args.inverse;
    clear = args.clear;
    zoom = args.zoom;
    redflag = args.red;
    greenflag = args.green;
    blueflag = args.blue;
    framebuffer = args.framebuffer;
    file_width = args.file_width;
    file_height = args.file_height;
    file_xoff = args.file_xoff;
    file_yoff = args.file_yoff;
    scr_width = args.screen_width;
    scr_height = args.screen_height;
    scr_xoff = args.screen_xoff;
    scr_yoff = args.screen_yoff;

    /* autosize input? */
    if (fileinput && autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, file_name, 1)) {
	    file_width = w;
	    file_height = h;
	} else {
	    fprintf(stderr, "bw-fb: unable to autosize\n");
	}
    }

    /* If no color planes were selected, load them all */
    if (redflag == 0 && greenflag == 0 && blueflag == 0)
	redflag = greenflag = blueflag = 1;

    /* If screen size was not set, track the file size */
    if (scr_width == 0)
	scr_width = file_width;
    if (scr_height == 0)
	scr_height = file_height;

    /* Open Display Device */
    if ((fbp = fb_open(framebuffer, scr_width, scr_height)) == NULL) {
	fprintf(stderr, "fb_open failed\n");
	bu_exit(3, NULL);
    }

    /* Get the screen size we were given */
    scr_width = fb_getwidth(fbp);
    scr_height = fb_getheight(fbp);

    /* compute pixels output to screen */
    if (scr_xoff < 0) {
	xout = scr_width + scr_xoff;
	xskip = (-scr_xoff);
	xstart = 0;
    } else {
	xout = scr_width - scr_xoff;
	xskip = 0;
	xstart = scr_xoff;
    }
    CLAMP(xout, 0, (long)(file_width-file_xoff));

    if (inverse)
	scr_yoff = (-scr_yoff);

    yout = scr_height - scr_yoff;
    CLAMP(yout, 0, (long)(file_height-file_yoff));

    if (xout > MAX_LINE) {
	fprintf(stderr, "bw-fb: can't output %ld pixel lines.\n", xout);
	return 2;
    }

    if (clear) {
	fb_clear(fbp, PIXEL_NULL);
    }
    if (zoom && xout && yout) {
	/* Zoom in, and center the file */
	fb_zoom(fbp, scr_width/xout, scr_height/yout);
	if (inverse)
	    fb_window(fbp, scr_xoff+xout/2, scr_height-1-(scr_yoff+yout/2));
	else
	    fb_window(fbp, scr_xoff+xout/2, scr_yoff+yout/2);
    }

    /* Test for simplest case */
    if (inverse == 0 && file_xoff == 0 && file_yoff == 0 &&
	scr_xoff >= 0 && scr_yoff >= 0 &&
	scr_xoff+file_width <= (unsigned)fb_getwidth(fbp)) {
	unsigned char *buf;
	int npix = file_width * yout;

	if ((buf = (unsigned char *)malloc(npix)) == NULL) {
	    perror("bw-fb malloc");
	    goto general;
	}
	n = bu_mread(infd, (char *)buf, npix);
	if (n != npix) {
	    fprintf(stderr, "bw-fb: read got %d, s/b %d\n", n, npix);
	    if (n <= 0)
		return 7;
	    npix = n;	/* show what we got */
	}
	n = (npix+file_width-1)/file_width;	/* num lines got */
	n = fb_bwwriterect(fbp, scr_xoff, scr_yoff, file_width, n, buf);
	if (npix != n) {
	    fprintf(stderr, "bw-fb: fb_bwwriterect() got %d, s/b %d\n", n, npix);
	    bu_exit(8, NULL);
	}
	fb_close(fbp);
	return 0;
    }

    /* Begin general case */
general:
    if (file_yoff != 0) skipbytes(infd, file_yoff*file_width);

    for (y = scr_yoff; y < scr_yoff + yout; y++) {
	if (y < 0 || y >= scr_height) {
	    skipbytes(infd, file_width);
	    continue;
	}
	if (file_xoff+xskip != 0)
	    skipbytes(infd, file_xoff+xskip);
	n = bu_mread(infd, &ibuf[0], xout);
	if (n <= 0) break;
	/*
	 * If we are not loading all color planes, we have
	 * to do a pre-read.
	 */
	if (redflag == 0 || greenflag == 0 || blueflag == 0) {
	    if (inverse)
		n = fb_read(fbp, scr_xoff, scr_height-1-y,
			    (unsigned char *)obuf, xout);
	    else
		n = fb_read(fbp, scr_xoff, y,
			    (unsigned char *)obuf, xout);
	    if (n < 0) break;
	}
	for (x = 0; x < xout; x++) {
	    if (redflag)
		obuf[x][RED] = ibuf[x];
	    if (greenflag)
		obuf[x][GRN] = ibuf[x];
	    if (blueflag)
		obuf[x][BLU] = ibuf[x];
	}
	if (inverse)
	    fb_write(fbp, xstart, scr_height-1-y, (unsigned char *)obuf, xout);
	else
	    fb_write(fbp, xstart, y, (unsigned char *)obuf, xout);

	/* slop at the end of the line? */
	if ((unsigned)(file_xoff+xskip+xout) < file_width)
	    skipbytes(infd, file_width-file_xoff-xskip-xout);
    }

    fb_close(fbp);
    return 0;
}


/*
 * Throw bytes away.  Use reads into ibuf buffer if a pipe, else seek.
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
	tries = num > MAX_LINE ? MAX_LINE : num;
	n = read(fd, ibuf, tries);
	if (n <= 0) {
	    return -1;
	}
	num -= n;
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
