/*                         B W R O T . C
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
/** @file util/bwrot.c
 *
 * Rotate, Invert, and/or Reverse the pixels in a Black
 * and White (.bw) file.
 *
 * The rotation logic was worked out for data ordered with
 * "upper left" first.  It is being used on files in first
 * quadrant order (lower left first).  Thus the "forward",
 * "backward" flags are reversed.
 *
 * The code was designed to never need to seek on the input,
 * while it *may* need to seek on output (if the max buffer
 * is too small).  It would be nice if we could handle the
 * reverse case also (e.g. pipe on stdout).
 *
 * Note that this program can be applied to any collection
 * of single byte entities.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

#include "bn.h"


ssize_t buflines, scanbytes;
ssize_t firsty = -1;	/* first "y" scanline in buffer */
ssize_t lasty = -1;	/* last "y" scanline in buffer */
unsigned char *bp;
unsigned char *obp;

ssize_t nxin = 512;
ssize_t nyin = 512;
ssize_t yin, xout, yout;
int plus90, minus90, reverse, invert;
size_t pixbytes = 1;

struct bwrot_args {
    struct bwrot_args *self;
    int help;
    int plus90;
    int minus90;
    int reverse;
    int invert;
    int width;
    int height;
    size_t pixbytes;
    fastf_t angle;
    const char *output;
};

static struct bwrot_args *
bwrot_args_from_storage(void *storage)
{
    return storage ? *(struct bwrot_args **)storage : NULL;
}

static int
bwrot_square(struct bu_vls *msg, const char *arg, void *storage)
{
    struct bwrot_args *args = bwrot_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args)
	args->width = args->height = value;
    return 0;
}

static int
bwrot_pixel_bytes(struct bu_vls *msg, const char *arg, void *storage)
{
    struct bwrot_args *args = bwrot_args_from_storage(storage);
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, SIZE_MAX))
	return -1;
    if (args)
	args->pixbytes = value;
    return 0;
}

static const struct bu_arg_option bwrot_options[] = {
    BU_ARG_FLAG("h", "help", struct bwrot_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("f", "forward", struct bwrot_args, minus90,
	"Rotate 90 degrees clockwise"),
    BU_ARG_FLAG("b", "backward", struct bwrot_args, plus90,
	"Rotate 90 degrees counterclockwise"),
    BU_ARG_FLAG("r", "reverse", struct bwrot_args, reverse,
	"Reverse each scanline horizontally"),
    BU_ARG_FLAG("i", "invert", struct bwrot_args, invert,
	"Invert scanline order vertically"),
    BU_ARG_CUSTOM("#", "pixel-bytes", struct bwrot_args, self,
	bwrot_pixel_bytes, "count", "Set the bytes per pixel"),
    BU_ARG_VALUE("a", "angle", struct bwrot_args, angle,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "degrees",
	"Rotate counterclockwise by an arbitrary angle"),
    BU_ARG_CUSTOM("s", "size", struct bwrot_args, self, bwrot_square,
	"pixels", "Set both input dimensions"),
    BU_ARG_ALIAS("S", NULL, "size", 1),
    BU_ARG_INTEGER_RANGE("w", "width", struct bwrot_args, width,
	1, 1, 1, INT_MAX, "pixels", "Set the input width"),
    BU_ARG_ALIAS("W", NULL, "width", 1),
    BU_ARG_INTEGER_RANGE("n", "height", struct bwrot_args, height,
	1, 1, 1, INT_MAX, "scanlines", "Set the input height"),
    BU_ARG_ALIAS("N", NULL, "height", 1),
    BU_ARG_VALUE("o", "output", struct bwrot_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file", "Write output to a file"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bwrot_operands[] = {
    BU_ARG_OPERAND("input-or-width", BU_ARG_VALUE_STRING, 0, 1,
	"Input file, or legacy input width when followed by height", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("legacy-height", 0, 1,
	1, 1, 1, INT_MAX, "Legacy input height used with a positional width", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bwrot_schema = BU_ARG_SCHEMA(
    "bwrot", "Rotate, invert, or reverse raster pixels",
    bwrot_options, bwrot_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bwrot_grammar = {
    &bwrot_schema, NULL, NULL
};

static int
copy_output_to_stdout(FILE *fp)
{
    unsigned char buf[BUFSIZ];
    size_t nread;

    if (fp == NULL)
	return 0;

    if (fflush(fp) != 0) {
	perror("fflush");
	return 0;
    }
    clearerr(fp);
    if (bu_fseek(fp, 0, SEEK_SET) < 0) {
	perror("fseek");
	return 0;
    }

    while ((nread = fread(buf, 1, sizeof(buf), fp)) > 0) {
	if (fwrite(buf, 1, nread, stdout) != nread) {
	    perror("fwrite");
	    return 0;
	}
    }

    return !ferror(fp);
}

static int
get_args(int argc, char **argv, struct bwrot_args *args, FILE **ifp,
	FILE **ofp, const char **out_file_name)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char *in_file_name = "-";
    int operand_index;
    int operand_count;

    operand_index = bu_arg_schema_parse_complete(&bwrot_schema, args, &msg,
	argc - 1, (const char **)(argv + 1));
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
	    bu_log("bwrot: invalid legacy dimensions: %s\n", bu_vls_cstr(&msg));
	    bu_vls_free(&msg);
	    return 0;
	}
	bu_vls_free(&msg);
    } else if (operand_count == 1) {
	in_file_name = argv[operand_index + 1];
    }

    nxin = args->width;
    nyin = args->height;
    pixbytes = args->pixbytes;
    plus90 = args->plus90;
    minus90 = args->minus90;
    reverse = args->reverse;
    invert = args->invert;

    if (!BU_STR_EQUAL(in_file_name, "-")) {
	*ifp = fopen(in_file_name, "rb");
	if (*ifp == NULL) {
	    bu_log("ERROR: %s cannot open \"%s\" for reading\n",
		bu_getprogname(), in_file_name);
	    return 0;
	}
    }

    if (args->output) {
	*ofp = fopen(args->output, "wb+");
	if (*ofp == NULL) {
	    bu_log("ERROR: %s cannot open \"%s\" for writing\n",
		bu_getprogname(), args->output);
	    return 0;
	}
	*out_file_name = args->output;
    }

    if (isatty(fileno(*ifp))) {
	bu_log("ERROR: %s will not read bw data from a tty\n"
	    "Redirect input or specify an input file.\n", bu_getprogname());
	return 0;
    }
    if (isatty(fileno(*ofp))) {
	bu_log("ERROR: %s will not write bw data to a tty\n"
	    "Redirect output or use the -o output option.\n", bu_getprogname());
	return 0;
    }

    return 1;
}


static void
fill_buffer(FILE *ifp, unsigned char *buf)
{
    buflines = fread(buf, scanbytes, buflines, ifp);

    firsty = lasty + 1;
    lasty = firsty + (buflines - 1);
}


static void
reverse_buffer(unsigned char *buf)
{
    ssize_t i;
    size_t j;
    unsigned char *p1, *p2, temp;

    for (i = 0; i < buflines; i++) {
	p1 = &buf[ i * scanbytes ];
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
 * Arbitrary angle rotation.
 *
 * 'a' is rotation angle
 *
 * Currently this needs to be able to buffer the entire image
 * in memory at one time.
 *
 * To rotate a point (x, y) CCW about the origin:
 * x' = x cos(a) - y sin(a)
 * y' = x sin(a) + y cos(a)
 * To rotate it about a point (xc, yc):
 * x' = (x-xc) cos(a) - (y-yc) sin(a) + xc
 *       = x cos(a) - y sin(a) + [xc - xc cos(a) + yc sin(a)]
 * y' = (x-xc) sin(a) + (y-yc) cos(a) + yc
 *	 = x sin(a) + y cos(a) + [yc - yc cos(a) - xc sin(a)]
 * So, to take one step in x:
 * dx' = cos(a)
 * dy' = sin(a)
 * or one step in y:
 * dx' = -sin(a)
 * dy' = cos(a)
 */
static void
arbrot(double a, FILE *ifp, FILE *ofp, unsigned char *buf)
{
#define DtoR(x)	((x) * DEG2RAD)
    size_t x, y;				/* working coord */
    double x2, y2;				/* its rotated position */
    double xc, yc;				/* rotation origin */
    size_t x_min, y_min, x_max, y_max;		/* area to rotate */
    double x_goop, y_goop;
    double sina, cosa;

    if (buflines != nyin) {
	/* I won't all fit in the buffer */
	fprintf(stderr, "bwrot: Sorry but I can't do an arbitrary rotate of an image this large\n");
	bu_exit (1, NULL);
    }
    if (buflines > nyin) buflines = nyin;
    fill_buffer(ifp, buf);

    /*
     * Convert rotation angle to radians.
     * Because we "pull down" the pixel from their rotated positions
     * to their standard ones, the sign of the rotation is reversed.
     */
    a = -DtoR(a);
    sina = sin(a);
    cosa = cos(a);

    /* XXX - Let the user pick the rotation origin? */
    xc = nxin / 2.0;
    yc = nyin / 2.0;

    x_goop = xc - xc * cosa + yc * sina;
    y_goop = yc - yc * cosa - xc * sina;

    x_min = 0;
    y_min = 0;
    x_max = nxin;
    y_max = nyin;

    for (y = y_min; y < y_max; y++) {
	x2 = x_min * cosa - y * sina + x_goop;
	y2 = x_min * sina + y * cosa + y_goop;
	for (x = x_min; x < x_max; x++) {
	    /* check for in bounds */
	    if (x2 > 0.0
		&& ZERO(x2)
		&& x2 < (double)nxin
		&& y2 > 0.0
		&& ZERO(y2)
		&& y2 < (double)nyin)
	    {
		fputc(buf[(int)y2*nyin + (int)x2], ofp);
	    } else {
		fputc(0, ofp);	/* XXX - settable color? */
	    }
	    /* "forward difference" our coordinates */
	    x2 += cosa;
	    y2 += sina;
	}
    }
}


int
main(int argc, char **argv)
{
    const size_t MAXBUFBYTES = 1280*1024;

    struct bwrot_args args = {NULL, 0, 0, 0, 0, 0, 512, 512, 1, 0.0, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    ssize_t x, y;
    size_t j;
    int ret = 0;
    size_t outbyte, outplace;
    FILE *ifp, *ofp;
    unsigned char *obuf;
    unsigned char *buffer;
    const char *out_file_name = NULL;
    ssize_t io;
    int introspection;

    args.self = &args;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    ifp = stdin;
    ofp = stdout;

    introspection = bu_arg_introspect(&msg, &bwrot_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    bu_vls_free(&msg);

    if (!get_args(argc, argv, &args, &ifp, &ofp, &out_file_name)) {
	char *help = bu_arg_schema_help(&bwrot_schema, "bwrot");
	bu_flog(stderr, "%s", help ? help : "");
	bu_free(help, "bwrot help");
	return 1;
    }
    if (args.help) {
	char *help = bu_arg_schema_help(&bwrot_schema, "bwrot");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bwrot help");
	return 0;
    }

    scanbytes = nxin * pixbytes;
    buflines = MAXBUFBYTES / scanbytes;
    if (buflines <= 0) {
	bu_exit(1, "%s", "bwrot: I'm not compiled to do a scanline that long!\n");
    }
    if (buflines > nyin) buflines = nyin;
    buffer = (unsigned char *)bu_malloc(buflines * scanbytes, "buffer");
    obuf = (unsigned char *)bu_malloc((nyin > nxin) ? nyin*pixbytes : nxin*pixbytes, "obuf");

    /*
     * Break out to added arbitrary angle routine
     */
    if (args.angle > 0.0) {
	arbrot(args.angle, ifp, ofp, buffer);
	goto done;
    }

    /*
     * Clear our "file pointer."  We need to maintain this
     * In order to tell if seeking is required.  bu_ftell() always
     * fails on pipes, so we can't use it.
     */
    outplace = 0;

    yin = 0;
    while (yin < nyin) {
	/* Fill buffer */
	fill_buffer(ifp, buffer);
	if (!buflines)
	    break;
	if (reverse)
	    reverse_buffer(buffer);
	if (plus90) {
	    for (x = 0; x < nxin; x++) {
		obp = obuf;
		bp = &buffer[ (lasty-firsty)*scanbytes + x*pixbytes ];
		for (y = lasty+1; y > yin; y--) {
		    /* firsty? */
		    for (j = 0; j < pixbytes; j++)
			*obp++ = *bp++;
		    bp -= scanbytes + pixbytes;
		}
		yout = x;
		xout = (nyin - 1) - lasty;
		outbyte = ((yout * nyin) + xout) * pixbytes;
		if (outplace != outbyte) {
		    if (bu_fseek(ofp, outbyte, SEEK_SET) < 0) {
			ret = 3;
			perror("fseek");
			bu_log("ERROR: %s can't seek on output (ofp=%p, outbyte=%zd)\n", bu_getprogname(), (void *)ofp, outbyte);
			goto done;
		    }
		    outplace = outbyte;
		}
		io = fwrite(obuf, pixbytes, buflines, ofp);
		if (io < buflines)
		    perror("fwrite");
		outplace += buflines*pixbytes;
	    }
	} else if (minus90) {
	    for (x = nxin; x > 0; x--) {
		obp = obuf;
		bp = &buffer[ (x-1)*pixbytes ];
		for (y = 0; y < buflines; y++) {
		    for (j = 0; j < pixbytes; j++)
			*obp++ = *bp++;
		    bp += scanbytes - pixbytes;
		}
		yout = (nxin - 1) - x + 1;
		xout = yin;
		outbyte = ((yout * nyin) + xout) * pixbytes;
		if (outplace != outbyte) {
		    if (bu_fseek(ofp, outbyte, SEEK_SET) < 0) {
			ret = 3;
			perror("fseek");
			bu_log("ERROR: %s can't seek on output (ofp=%p, outbyte=%zd)\n", bu_getprogname(), (void *)ofp, outbyte);
			goto done;
		    }
		    outplace = outbyte;
		}
		io = fwrite(obuf, pixbytes, buflines, ofp);
		if (io < buflines)
		    perror("fwrite");
		outplace += buflines*pixbytes;
	    }
	} else if (invert) {
	    for (y = lasty+1; (ssize_t)y > firsty; y--) {
		yout = (nyin - 1) - y + 1;
		outbyte = yout * scanbytes;
		if (outplace != outbyte) {
		    if (bu_fseek(ofp, outbyte, SEEK_SET) < 0) {
			ret = 3;
			perror("fseek");
			bu_log("ERROR: %s can't seek on output (ofp=%p, outbyte=%zd)\n", bu_getprogname(), (void *)ofp, outbyte);
			goto done;
		    }
		    outplace = outbyte;
		}
		io = fwrite(&buffer[(y-firsty-1)*scanbytes], 1, scanbytes, ofp);
		if (io < scanbytes)
		    perror("fwrite");
		outplace += scanbytes;
	    }
	} else {
	    /* Reverse only */
	    for (y = 0; y < buflines; y++) {
		io = fwrite(&buffer[y*scanbytes], 1, scanbytes, ofp);
		if (io < scanbytes)
		    perror("fwrite");
	    }
	}

	yin += buflines;
    }

done:
    if (ret == 0 && out_file_name != NULL && !isatty(fileno(stdout))) {
	if (!copy_output_to_stdout(ofp))
	    ret = 1;
    }

    if (ifp != stdin)
	fclose(ifp);
    if (ofp != stdout)
	fclose(ofp);
    bu_free(buffer, "buffer");
    bu_free(obuf, "obuf");

    return ret;
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
