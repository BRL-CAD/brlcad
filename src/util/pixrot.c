/*                        P I X R O T . C
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
/** @file util/pixrot.c
 *
 * Rotate, Invert, and/or Reverse the pixels in a RGB (.pix) file.
 *
 * The rotation logic was worked out for data ordered with "upper
 * left" first.  It is being used on files in first quadrant order
 * (lower left first).  Thus the "forward", "backward" flags are
 * reversed.
 *
 * This is a generalization of bwrot and can in fact handle "pixels"
 * of any size.  Thus this routine could be used to say, rotate a
 * matrix of floating point values, etc.
 *
 */

#include "common.h"

#include <limits.h>
#include <stdlib.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"


/* 4 times bigger than typ. screen */
/*#define MAXBUFBYTES (1280*1024*3*4) */
#define MAXBUFBYTES (12288*16384*2)
ssize_t buflines, scanbytes;
int firsty = -1;	/* first "y" scanline in buffer */
int lasty = -1;	/* last "y" scanline in buffer */
unsigned char *buffer;
unsigned char *bp;
unsigned char *obuf;
unsigned char *obp;

int nxin = 512;
int nyin = 512;
int yin, xout, yout;
int plus90, minus90, reverse, invert;
int pixbytes = 3;

void fill_buffer(void);
void reverse_buffer(void);

FILE *ifp, *ofp;

struct pixrot_args {
    struct pixrot_args *self;
    int help;
    int plus90;
    int minus90;
    int reverse;
    int invert;
    int width;
    int height;
    int pixbytes;
};

static struct pixrot_args *
pixrot_args_from_storage(void *storage)
{
    return storage ? *(struct pixrot_args **)storage : NULL;
}

static int
pixrot_square(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pixrot_args *args = pixrot_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args)
	args->width = args->height = value;
    return 0;
}

static const struct bu_arg_option pixrot_options[] = {
    BU_ARG_FLAG("h", "help", struct pixrot_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("f", "forward", struct pixrot_args, minus90,
	"Rotate 90 degrees clockwise"),
    BU_ARG_FLAG("b", "backward", struct pixrot_args, plus90,
	"Rotate 90 degrees counterclockwise"),
    BU_ARG_FLAG("r", "reverse", struct pixrot_args, reverse,
	"Reverse each scanline horizontally"),
    BU_ARG_FLAG("i", "invert", struct pixrot_args, invert,
	"Invert scanline order vertically"),
    BU_ARG_INTEGER_RANGE("#", "pixel-bytes", struct pixrot_args, pixbytes,
	1, 1, 1, INT_MAX, "count", "Set the bytes per pixel"),
    BU_ARG_CUSTOM("s", "size", struct pixrot_args, self, pixrot_square,
	"pixels", "Set both input dimensions"),
    BU_ARG_ALIAS("S", NULL, "size", 1),
    BU_ARG_INTEGER_RANGE("w", "width", struct pixrot_args, width,
	1, 1, 1, INT_MAX, "pixels", "Set the input width"),
    BU_ARG_ALIAS("W", NULL, "width", 1),
    BU_ARG_INTEGER_RANGE("n", "height", struct pixrot_args, height,
	1, 1, 1, INT_MAX, "scanlines", "Set the input height"),
    BU_ARG_ALIAS("N", NULL, "height", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixrot_operands[] = {
    BU_ARG_OPERAND("input-or-width", BU_ARG_VALUE_STRING, 0, 1,
	"Input file, or legacy input width when followed by height", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("legacy-height", 0, 1,
	1, 1, 1, INT_MAX, "Legacy input height used with a positional width", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixrot_schema = BU_ARG_SCHEMA(
    "pixrot", "Rotate, invert, or reverse raster pixels",
    pixrot_options, pixrot_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixrot_grammar = {
    &pixrot_schema, NULL, NULL
};

static int
get_args(int argc, char **argv, struct pixrot_args *args)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char *input = "-";
    int operand_index = bu_arg_schema_parse_complete(&pixrot_schema, args,
	&msg, argc - 1, (const char **)(argv + 1));
    int operand_count;

    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 0;
    }
    bu_vls_free(&msg);
    if (args->help)
	return 1;

    operand_count = argc - operand_index - 1;
    if (operand_count == 2) {
	if (!bu_arg_integer_range_from_str(&msg, &args->width,
		argv[operand_index + 1], 10, 1, INT_MAX) ||
	    !bu_arg_integer_range_from_str(&msg, &args->height,
		argv[operand_index + 2], 10, 1, INT_MAX)) {
	    bu_log("pixrot: invalid legacy dimensions: %s\n", bu_vls_cstr(&msg));
	    bu_vls_free(&msg);
	    return 0;
	}
	bu_vls_free(&msg);
    } else if (operand_count == 1) {
	input = argv[operand_index + 1];
    }

    nxin = args->width;
    nyin = args->height;
    pixbytes = args->pixbytes;
    plus90 = args->plus90;
    minus90 = args->minus90;
    reverse = args->reverse;
    invert = args->invert;

    if (!BU_STR_EQUAL(input, "-")) {
	ifp = fopen(input, "rb");
	if (ifp == NULL) {
	    bu_log("pixrot: cannot open \"%s\" for reading\n", input);
	    return 0;
	}
    }
    if (isatty(fileno(ifp))) {
	bu_log("pixrot will not read pixel data from a terminal\n");
	return 0;
    }
    if (isatty(fileno(ofp))) {
	bu_log("pixrot will not write pixel data to a terminal\n");
	return 0;
    }

    return 1;
}


int
main(int argc, char **argv)
{
    struct pixrot_args args = {NULL, 0, 0, 0, 0, 0, 512, 512, 3};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int x, y, j;
    b_off_t outbyte, outplace;
    size_t ret;
    int introspection;

    args.self = &args;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    introspection = bu_arg_introspect(&msg, &pixrot_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    bu_vls_free(&msg);

    ifp = stdin;
    ofp = stdout;
    if (!get_args(argc, argv, &args)) {
	char *help = bu_arg_schema_help(&pixrot_schema, "pixrot");
	bu_flog(stderr, "%s", help ? help : "");
	bu_free(help, "pixrot help");
	return 1;
    }
    if (args.help) {
	char *help = bu_arg_schema_help(&pixrot_schema, "pixrot");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixrot help");
	return 0;
    }

    scanbytes = nxin * pixbytes;
    buflines = MAXBUFBYTES / scanbytes;
    if (buflines <= 0) {
	bu_exit(2, "pixrot: I'm not compiled to do a scanline that long!\n");
    }
    if (buflines > nyin) buflines = nyin;
    buffer = (unsigned char *)malloc(buflines * scanbytes);
    obuf = (nyin > nxin) ? (unsigned char *)malloc(nyin * pixbytes) : (unsigned char *)malloc(nxin * pixbytes);
    if (buffer == (unsigned char *)0 || obuf == (unsigned char *)0) {
	bu_exit(3, "pixrot: malloc failed\n");
    }

    /*
     * Clear our "file pointer." We need to maintain this
     * In order to tell if seeking is required.  bu_ftell() always
     * fails on pipes, so we can't use it.
     */
    outplace = 0;

    yin = 0;
    while (yin < nyin) {
	/* Fill buffer */
	fill_buffer();
	if (reverse)
	    reverse_buffer();
	if (plus90) {
	    for (x = 0; x < nxin; x++) {
		obp = obuf;
		bp = &buffer[ (lasty-firsty)*scanbytes + x*pixbytes ];
		for (y = lasty; y >= yin; y--) {
		    /* firsty? */
		    for (j = 0; j < pixbytes; j++)
			*obp++ = *bp++;
		    bp = bp - scanbytes - pixbytes;
		}
		yout = x;
		xout = (nyin - 1) - lasty;
		outbyte = ((yout * nyin) + xout) * pixbytes;
		if (outplace != outbyte) {
		    if (bu_fseek(ofp, outbyte, 0) < 0) {
			bu_exit(3, "pixrot: Can't seek on output, yet I need to!\n");
		    }
		    outplace = outbyte;
		}
		ret = fwrite(obuf, pixbytes, buflines, ofp);
		if (ret == 0) {
		    perror("fwrite");
		    break;
		}
		outplace += (pixbytes * buflines);
	    }
	} else if (minus90) {
	    for (x = nxin-1; x >= 0; x--) {
		obp = obuf;
		bp = &buffer[ x*pixbytes ];
		for (y = firsty; y <= lasty; y++) {
		    for (j = 0; j < pixbytes; j++)
			*obp++ = *bp++;
		    bp = bp + scanbytes - pixbytes;
		}
		yout = (nxin - 1) - x;
		xout = yin;
		outbyte = ((yout * nyin) + xout) * pixbytes;
		if (outplace != outbyte) {
		    if (bu_fseek(ofp, outbyte, 0) < 0) {
			bu_exit(3, "pixrot: Can't seek on output, yet I need to!\n");
		    }
		    outplace = outbyte;
		}
		ret = fwrite(obuf, pixbytes, buflines, ofp);
		if (ret == 0) {
		    perror("fwrite");
		    break;
		}
		outplace += (pixbytes * buflines);
	    }
	} else if (invert) {
	    for (y = lasty; y >= firsty; y--) {
		yout = (nyin - 1) - y;
		outbyte = yout * scanbytes;
		if (outplace != outbyte) {
		    if (bu_fseek(ofp, outbyte, 0) < 0) {
			bu_exit(3, "pixrot: Can't seek on output, yet I need to!\n");
		    }
		    outplace = outbyte;
		}
		ret = fwrite(&buffer[(y-firsty)*scanbytes], 1, scanbytes, ofp);
		if (ret == 0) {
		    perror("fwrite");
		    break;
		}
		outplace += scanbytes;
	    }
	} else {
	    /* Reverse only */
	    for (y = 0; y < buflines; y++) {
		ret = fwrite(&buffer[y*scanbytes], 1, scanbytes, ofp);
		if (ret == 0) {
		    perror("fwrite");
		    break;
		}
	    }
	}

	yin += buflines;
    }
    return 0;
}


void
fill_buffer(void)
{
    buflines = fread(buffer, scanbytes, buflines, ifp);

    firsty = lasty + 1;
    lasty = firsty + (buflines - 1);
}


void
reverse_buffer(void)
{
    int i, j;
    unsigned char *p1, *p2, temp;

    for (i = 0; i < buflines; i++) {
	p1 = &buffer[ i * scanbytes ];
	p2 = p1 + (scanbytes - pixbytes);
	while (p1 < p2) {
	    for (j = 0; j < pixbytes; j++) {
		temp = *p1;
		*p1++ = *p2;
		*p2++ = temp;
	    }
	    p2 -= 2*pixbytes;
	}
    }
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
