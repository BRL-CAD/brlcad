/*                        P N G - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2026 United States Government as represented by
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
/** @file png-fb.c
 *
 * Program to take PNG (Portable Network Graphics) files and send them to a framebuffer.
 *
 */

#include "common.h"

#include <limits.h>
#include <stdlib.h>

#include "bio.h"

#include "png.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "dm.h"
static png_color_16 def_backgrd={ 0, 0, 0, 0, 0 };
static unsigned char **scanline;	/* image row pointers */

static int multiple_lines = 0;	/* Streamlined operation */

static const char *framebuffer = NULL;
static FILE *fp_in;

static int scr_width = 0;		/* screen tracks file if not given */
static int scr_height = 0;
static int file_xoff=0, file_yoff=0;
static int scr_xoff=0, scr_yoff=0;
static int clear = 0;
static int zoom = 0;
static int inverse = 0;		/* Draw upside-down */
static int one_line_only = 0;	/* insist on 1-line writes */
static int verbose = 0;
static int header_only = 0;

static double def_screen_gamma=1.0;	/* Don't add more gamma, by default */
/* Particularly because on SGI, the system provides gamma correction,
 * so programs like this one don't have to.
 */

struct png_fb_args {
    struct png_fb_args *self;
    int help;
    int one_line_only;
    int multiple_lines;
    double screen_gamma;
    int header_only;
    int inverse;
    int clear;
    int verbose;
    int zoom;
    const char *framebuffer;
    int file_xoff;
    int file_yoff;
    int screen_xoff;
    int screen_yoff;
    int screen_width;
    int screen_height;
};

static int
png_fb_screen_size(struct bu_vls *msg, const char *arg, void *storage)
{
    struct png_fb_args *args = storage ? *(struct png_fb_args **)storage : NULL;
    int value;
    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 0, INT_MAX))
	return -1;
    if (args)
	args->screen_width = args->screen_height = value;
    return 0;
}

static const struct bu_arg_option png_fb_options[] = {
    BU_ARG_FLAG("h", "help", struct png_fb_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("1", "one-line", struct png_fb_args, one_line_only,
	"Force one scanline per framebuffer write"),
    BU_ARG_INTEGER_RANGE("m", "multiple-lines", struct png_fb_args,
	multiple_lines, 1, 0, 1, INT_MAX, "scanlines",
	"Write blocks containing this many scanlines"),
    BU_ARG_VALUE_VALIDATE("g", "screen-gamma", struct png_fb_args,
	screen_gamma, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"gamma", "Set additional display gamma correction"),
    BU_ARG_FLAG("H", "header-only", struct png_fb_args, header_only,
	"Print image dimensions without displaying the image"),
    BU_ARG_FLAG("i", "invert", struct png_fb_args, inverse,
	"Display scanlines from top to bottom"),
    BU_ARG_FLAG("c", "clear", struct png_fb_args, clear,
	"Clear the framebuffer before writing"),
    BU_ARG_FLAG("v", "verbose", struct png_fb_args, verbose,
	"Report PNG metadata while reading"),
    BU_ARG_FLAG("z", "zoom", struct png_fb_args, zoom,
	"Zoom and center the displayed image"),
    BU_ARG_VALUE("F", "framebuffer", struct png_fb_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer instead of the FB_FILE environment default"),
    BU_ARG_INTEGER_RANGE("x", "input-x-offset", struct png_fb_args,
	file_xoff, 1, 0, 1, INT_MAX, "pixels",
	"Skip pixels from the left of each input scanline"),
    BU_ARG_INTEGER_RANGE("y", "input-y-offset", struct png_fb_args,
	file_yoff, 1, 0, 1, INT_MAX, "scanlines",
	"Skip scanlines from the bottom of the input"),
    BU_ARG_VALUE("X", "screen-x-offset", struct png_fb_args, screen_xoff,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "pixels",
	"Offset the image horizontally on the framebuffer"),
    BU_ARG_VALUE("Y", "screen-y-offset", struct png_fb_args, screen_yoff,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "scanlines",
	"Offset the image vertically on the framebuffer"),
    BU_ARG_CUSTOM("S", "framebuffer-size", struct png_fb_args, self,
	png_fb_screen_size, "pixels", "Set both framebuffer dimensions"),
    BU_ARG_INTEGER_RANGE("W", "framebuffer-width", struct png_fb_args,
	screen_width, 1, 0, 1, INT_MAX, "pixels",
	"Set the framebuffer width"),
    BU_ARG_INTEGER_RANGE("N", "framebuffer-height", struct png_fb_args,
	screen_height, 1, 0, 1, INT_MAX, "scanlines",
	"Set the framebuffer height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand png_fb_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PNG input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema png_fb_schema = BU_ARG_SCHEMA(
    "png-fb", "Display a PNG image on a framebuffer", png_fb_options,
    png_fb_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar png_fb_grammar = {
    &png_fb_schema, NULL, NULL
};

static int
png_fb_help(FILE *stream, int status)
{
    char *schema_help = bu_arg_schema_help(&png_fb_schema, "png-fb");
    bu_flog(stream, "%s", schema_help ? schema_help : "");
    bu_free(schema_help, "png-fb help");
    return status;
}


int
main(int argc, char **argv)
{
    struct png_fb_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int y;
    struct fb *fbp;
    int i;
    int xout, yout, m, xstart;
    int block_writes;
    png_structp png_p;
    png_infop info_p;
    char header[8];
    int bit_depth;
    int color_type;
    png_color_16p input_backgrd;
    double gammaval=1.0;
    int file_width, file_height;
    unsigned char *image;
    int operand_index;
    args.self = &args;
    args.screen_gamma = 1.0;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &png_fb_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&png_fb_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return png_fb_help(stdout, 0);

    if (argc - 1 - operand_index == 1) {
	const char *input_name = argv[operand_index + 1];
	fp_in = fopen(input_name, "rb");
	if (!fp_in) {
	    bu_flog(stderr, "png-fb: cannot open \"%s\" for reading\n",
		input_name);
	    return 1;
	}
    } else {
	if (isatty(fileno(stdin))) {
	    bu_flog(stderr, "png-fb requires an input file or redirected input\n");
	    return png_fb_help(stderr, 1);
	}
	fp_in = stdin;
	setmode(fileno(stdin), O_BINARY);
    }

    one_line_only = args.one_line_only;
    multiple_lines = args.multiple_lines;
    def_screen_gamma = args.screen_gamma;
    header_only = args.header_only;
    inverse = args.inverse;
    clear = args.clear;
    verbose = args.verbose;
    zoom = args.zoom;
    framebuffer = args.framebuffer;
    file_xoff = args.file_xoff;
    file_yoff = args.file_yoff;
    scr_xoff = args.screen_xoff;
    scr_yoff = args.screen_yoff;
    scr_width = args.screen_width;
    scr_height = args.screen_height;

    if (fread(header, 8, 1, fp_in) != 1) {
	bu_exit(EXIT_FAILURE,  "png-fb: ERROR: Failed while reading file header!!!\n");
    }

    if (png_sig_cmp((png_bytep)header, 0, 8)) {
	bu_exit(EXIT_FAILURE,  "png-fb: This is not a PNG file!!!\n");
    }

    png_p = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	bu_exit(EXIT_FAILURE,  "png-fb: png_create_read_struct() failed!!\n");
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	bu_exit(EXIT_FAILURE,  "png-fb: png_create_info_struct() failed!!\n");
    }

    png_init_io(png_p, fp_in);

    png_set_sig_bytes(png_p, 8);

    png_read_info(png_p, info_p);

    color_type = png_get_color_type(png_p, info_p);

    png_set_expand(png_p);
    bit_depth = png_get_bit_depth(png_p, info_p);
    if (bit_depth == 16)
	png_set_strip_16(png_p);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
	color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
	png_set_gray_to_rgb(png_p);

    file_width = png_get_image_width(png_p, info_p);
    file_height = png_get_image_height(png_p, info_p);

    if (verbose) {
	switch (color_type) {
	    case PNG_COLOR_TYPE_GRAY:
		bu_log("color type: b/w (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_GRAY_ALPHA:
		bu_log("color type: b/w with alpha channel (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_PALETTE:
		bu_log("color type: color palette (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_RGB:
		bu_log("color type: RGB (bit depth=%d)\n", bit_depth);
		break;
	    case PNG_COLOR_TYPE_RGB_ALPHA:
		bu_log("color type: RGB with alpha channel (bit depth=%d)\n", bit_depth);
		break;
	    default:
		bu_log("Unrecognized color type (bit depth=%d)\n", bit_depth);
		break;
	}
	bu_log("Image size: %d X %d\n", file_width, file_height);
    }

    if (header_only) {
	fprintf(stdout, "WIDTH=%d HEIGHT=%d\n", file_width, file_height);
	bu_exit(0, NULL);
    }

    if (png_get_bKGD(png_p, info_p, &input_backgrd)) {
	if (verbose && (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
			color_type == PNG_COLOR_TYPE_RGB_ALPHA))
	    bu_log("background color: %d %d %d\n", input_backgrd->red, input_backgrd->green, input_backgrd->blue);
	png_set_background(png_p, input_backgrd, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
    } else
	png_set_background(png_p, &def_backgrd, PNG_BACKGROUND_GAMMA_FILE, 0, 1.0);

    if (!png_get_gAMA(png_p, info_p, &gammaval))
	gammaval = 0.5;
    png_set_gamma(png_p, def_screen_gamma, gammaval);
    if (verbose)
	bu_log("file gamma: %f, additional screen gamma: %f\n",
	       gammaval, def_screen_gamma);

    if (verbose) {
	if (png_get_interlace_type(png_p, info_p) == PNG_INTERLACE_NONE)
	    bu_log("not interlaced\n");
	else
	    bu_log("interlaced\n");
    }

    png_read_update_info(png_p, info_p);

    /* allocate memory for image */
    image = (unsigned char *)bu_calloc(1,
	file_width*file_height*sizeof(RGBpixel), "image");

    /* create rows array */
    scanline = (unsigned char **)bu_calloc(file_height, sizeof(unsigned char *), "scanline");
    for (i=0; i<file_height; i++)
	scanline[i] = image+((file_height-i-1)*file_width*sizeof(RGBpixel));

    png_read_image(png_p, scanline);

    if (verbose) {
	png_timep mod_time;
	png_textp text;
	int num_text;

	png_read_end(png_p, info_p);
	if (png_get_text(png_p, info_p, &text, &num_text)) {
	    for (i=0; i<num_text; i++)
		bu_log("%s: %s\n", text[i].key, text[i].text);
	}
	if (png_get_tIME(png_p, info_p, &mod_time))
	    bu_log("Last modified: %d/%d/%d %d:%d:%d\n", mod_time->month, mod_time->day,
		   mod_time->year, mod_time->hour, mod_time->minute, mod_time->second);
    }

    /* If screen size was not set, track the file size */
    if (scr_width == 0)
	scr_width = file_width;
    if (scr_height == 0)
	scr_height = file_height;

    if ((fbp = fb_open(framebuffer, scr_width, scr_height)) == NULL)
	bu_exit(12, NULL);

    /* Get the screen size we were given */
    scr_width = fb_getwidth(fbp);
    scr_height = fb_getheight(fbp);

    /* compute number of pixels to be output to screen */
    if (scr_xoff < 0) {
	xout = scr_width + scr_xoff;
	xstart = 0;
    } else {
	xout = scr_width - scr_xoff;
	xstart = scr_xoff;
    }

    if (xout < 0)
	bu_exit(0, NULL);			/* off screen */
    V_MIN(xout, (file_width-file_xoff));

    if (inverse)
	scr_yoff = (-scr_yoff);

    yout = scr_height - scr_yoff;
    if (yout < 0)
	bu_exit(0, NULL);			/* off screen */
    V_MIN(yout, (file_height-file_yoff));

    /* Only in the simplest case use multi-line writes */
    block_writes = !one_line_only && multiple_lines > 0 && !inverse && !zoom &&
	scr_xoff >= 0 && scr_yoff >= 0 &&
	xout == file_width && file_xoff == 0 &&
	file_width <= scr_width;

    if (clear) {
	fb_clear(fbp, PIXEL_NULL);
    }
    if (zoom) {
	/* Zoom in, and center the display.  Use square zoom. */
	int newzoom;
	newzoom = scr_width/xout;
	V_MIN(newzoom, scr_height/yout);

	if (inverse) {
	    fb_view(fbp,
		    scr_xoff+xout/2, scr_height-1-(scr_yoff+yout/2),
		    newzoom, newzoom);
	} else {
	    fb_view(fbp,
		    scr_xoff+xout/2, scr_yoff+yout/2,
		    newzoom, newzoom);
	}
    }

    if (block_writes) {
	/* Bottom to top with multi-line writes */
	unsigned char *rows = image+(file_yoff*file_width*sizeof(RGBpixel));
	for (y = scr_yoff; y < scr_yoff + yout; y += multiple_lines) {
	    int height = multiple_lines;
	    /* Don't over-write */
	    if (y + height > scr_yoff + yout)
		height = scr_yoff + yout - y;
	    if (height <= 0) break;
	    m = fb_writerect(fbp, scr_xoff, y,
			     file_width, height,
			     rows);
	    if (m != file_width*height) {
		fprintf(stderr,
			"png-fb: fb_writerect(x=%d, y=%d, w=%d, h=%d) failure, ret=%d, s/b=%d\n",
			scr_xoff, y,
			file_width, height, m, file_width*height);
	    }
	    rows += height*file_width*sizeof(RGBpixel);
	}
    } else if (!inverse) {
	/* Normal way -- bottom to top */
	int line=file_height-file_yoff-1;
	for (y = scr_yoff; y < scr_yoff + yout; y++) {
	    m = fb_write(fbp, xstart, y, scanline[line--]+(3*file_xoff), xout);
	    if (m != xout) {
		fprintf(stderr,
			"png-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
			scr_xoff, y, xout,
			m, xout);
	    }
	}
    } else {
	/* Inverse -- top to bottom */
	int line=file_height-file_yoff-1;
	for (y = scr_height-1-scr_yoff; y >= scr_height-scr_yoff-yout; y--) {
	    m = fb_write(fbp, xstart, y, scanline[line--]+(3*file_xoff), xout);
	    if (m != xout) {
		fprintf(stderr,
			"png-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
			scr_xoff, y, xout,
			m, xout);
	    }
	}
    }
    if (fb_close(fbp) < 0) {
	fprintf(stderr, "png-fb: Warning: fb_close() error\n");
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
