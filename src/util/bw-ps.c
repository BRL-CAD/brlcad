/*                         B W - P S . C
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
/** @file util/bw-ps.c
 *
 * Convert a black and white (bw) file to an 8-bit PostScript image.
 *
 */

#include "common.h"

#include <limits.h>
#include <math.h>
#include <time.h> /* for ctime() */
#include "bio.h"
#include "bu/app.h"
#include "bu/arg.h"
#include "bu/exit.h"


#define DEFAULT_SIZE 6.75	/* default output size in inches */
#define MAX_BYTES (64*128)	/* max bytes per image chunk */

static int encapsulated = 0;	/* encapsulated postscript */
static int center = 0;		/* center output on 8.5 x 11 page */
static int landscape = 0;	/* landscape mode */

static size_t width = 512;		/* input size in pixels */
static size_t height = 512;
static double outwidth = DEFAULT_SIZE;		/* output image size in inches */
static double outheight = DEFAULT_SIZE;
static size_t xpoints;		/* output image size in points */
static size_t ypoints;
static size_t pagewidth = 612;	/* page size in points - 8.5 inches */
static size_t pageheight = 792;	/* 11 inches */

static const char *file_name;
static FILE *infp;

struct bw_ps_args {
    struct bw_ps_args *self;
    int help;
    int encapsulated;
    int center;
    int landscape;
    size_t width;
    size_t height;
    double output_width;
    double output_height;
};

static struct bw_ps_args *
bw_ps_args_from_storage(void *storage)
{
    return storage ? *(struct bw_ps_args **)storage : NULL;
}

static int
bw_ps_square(struct bu_vls *msg, const char *arg, void *storage, int output)
{
    struct bw_ps_args *args = bw_ps_args_from_storage(storage);

    if (output) {
	double value;
	if (!bu_arg_double_from_str(msg, &value, arg))
	    return -1;
	if (value <= 0.0) {
	    if (msg)
		bu_vls_printf(msg, "value must be positive: %s", arg);
	    return -1;
	}
	if (args)
	    args->output_width = args->output_height = value;
    } else {
	size_t value;
	if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, SIZE_MAX))
	    return -1;
	if (args)
	    args->width = args->height = value;
    }
    return 0;
}

static int
bw_ps_dimension(struct bu_vls *msg, const char *arg, void *storage, int output,
	int height_only)
{
    struct bw_ps_args *args = bw_ps_args_from_storage(storage);

    if (output) {
	double value;
	if (!bu_arg_double_from_str(msg, &value, arg))
	    return -1;
	if (value <= 0.0) {
	    if (msg)
		bu_vls_printf(msg, "value must be positive: %s", arg);
	    return -1;
	}
	if (args) {
	    if (height_only)
		args->output_height = value;
	    else
		args->output_width = value;
	}
    } else {
	size_t value;
	if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, SIZE_MAX))
	    return -1;
	if (args) {
	    if (height_only)
		args->height = value;
	    else
		args->width = value;
	}
    }
    return 0;
}

#define BW_PS_DIMENSION_CALLBACK(_name, _output, _height) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return bw_ps_dimension(msg, arg, storage, _output, _height); \
    }

BW_PS_DIMENSION_CALLBACK(bw_ps_input_width, 0, 0)
BW_PS_DIMENSION_CALLBACK(bw_ps_input_height, 0, 1)
BW_PS_DIMENSION_CALLBACK(bw_ps_output_width, 1, 0)
BW_PS_DIMENSION_CALLBACK(bw_ps_output_height, 1, 1)

static int
bw_ps_input_square(struct bu_vls *msg, const char *arg, void *storage)
{
    return bw_ps_square(msg, arg, storage, 0);
}

static int
bw_ps_output_square(struct bu_vls *msg, const char *arg, void *storage)
{
    return bw_ps_square(msg, arg, storage, 1);
}

static const struct bu_arg_option bw_ps_options[] = {
    BU_ARG_FLAG("h", "help", struct bw_ps_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("e", "encapsulated", struct bw_ps_args, encapsulated,
	"Produce Encapsulated PostScript"),
    BU_ARG_FLAG("c", "center", struct bw_ps_args, center,
	"Center the image on the page"),
    BU_ARG_FLAG("L", "landscape", struct bw_ps_args, landscape,
	"Use landscape page orientation"),
    BU_ARG_CUSTOM("s", "input-size", struct bw_ps_args, self,
	bw_ps_input_square, "pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "input-width", struct bw_ps_args, self,
	bw_ps_input_width, "pixels", "Set the input width"),
    BU_ARG_CUSTOM("n", "input-height", struct bw_ps_args, self,
	bw_ps_input_height, "scanlines", "Set the input height"),
    BU_ARG_CUSTOM("S", "output-size", struct bw_ps_args, self,
	bw_ps_output_square, "inches", "Set both output dimensions"),
    BU_ARG_CUSTOM("W", "output-width", struct bw_ps_args, self,
	bw_ps_output_width, "inches", "Set the output width"),
    BU_ARG_CUSTOM("N", "output-height", struct bw_ps_args, self,
	bw_ps_output_height, "inches", "Set the output height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bw_ps_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"BW input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bw_ps_schema = BU_ARG_SCHEMA(
    "bw-ps", "Convert a BW image to PostScript", bw_ps_options,
    bw_ps_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bw_ps_grammar = {
    &bw_ps_schema, NULL, NULL
};

void
prolog(FILE *fp, const char *name, int w, int h)


/* in points */
{
    time_t ltime;

    ltime = time(0);

    if (encapsulated) {
	fputs("%!PS-Adobe-2.0 EPSF-1.2\n", fp);
	fprintf(fp, "%%%%Title: %s\n", name);
	fputs("%%Creator: BRL-CAD bw-ps\n", fp);
	fprintf(fp, "%%%%CreationDate: %s", ctime(&ltime));
	fputs("%%Pages: 0\n", fp);
    } else {
	fputs("%!PS-Adobe-1.0\n", fp);
	fputs("%begin(plot)\n", fp);
	fprintf(fp, "%%%%Title: %s\n", name);
	fputs("%%Creator: BRL-CAD bw-ps\n", fp);
	fprintf(fp, "%%%%CreationDate: %s", ctime(&ltime));
    }
    fprintf(fp, "%%%%BoundingBox: 0 0 %d %d\n", w, h);
    fputs("%%EndComments\n\n", fp);

    if (!encapsulated && landscape) {
	int tmp;
	tmp = pagewidth;
	pagewidth = pageheight;
	pageheight = tmp;
	fprintf(fp, "90 rotate\n");
	fprintf(fp, "0 -%lu translate\n", (unsigned long)pageheight);
    }
    if (!encapsulated && center) {
	int xtrans, ytrans;
	xtrans = (pagewidth - w)/2.0;
	ytrans = (pageheight - h)/2.0;
	fprintf(fp, "%d %d translate\n", xtrans, ytrans);
    }
    fprintf(fp, "%d %d scale\n\n", w, h);
}


void
postlog(FILE *fp)
{
    if (!encapsulated)
	fputs("%end(plot)\n", fp);

    fputs("\nshowpage\n", fp);
}


static int
get_args(int argc, char **argv, struct bw_ps_args *args)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int operand_index = bu_arg_schema_parse_complete(&bw_ps_schema, args,
	&msg, argc - 1, (const char **)(argv + 1));

    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 0;
    }
    bu_vls_free(&msg);
    if (args->help)
	return 1;

    encapsulated = args->encapsulated;
    center = args->center;
    landscape = args->landscape;
    width = args->width;
    height = args->height;
    outwidth = args->output_width;
    outheight = args->output_height;

    if (argc - operand_index - 1 == 0) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "stdin";
	infp = stdin;
    } else {
	file_name = argv[operand_index + 1];
	if ((infp = fopen(file_name, "rb")) == NULL) {
	    fprintf(stderr,
		    "bw-ps: cannot open \"%s\" for reading\n",
		    file_name);
	    return 0;
	}
    }

    return 1;
}


int
main(int argc, char **argv)
{
    struct bw_ps_args args = {NULL, 0, 0, 0, 0, 512, 512,
	DEFAULT_SIZE, DEFAULT_SIZE};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    size_t num = 0;
    size_t scans_per_patch, bytes_per_patch;
    size_t y;
    FILE *ofp = NULL;
    int introspection;

    args.self = &args;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    introspection = bu_arg_introspect(&msg, &bw_ps_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    bu_vls_free(&msg);

    if (!get_args(argc, argv, &args)) {
	char *help = bu_arg_schema_help(&bw_ps_schema, "bw-ps");
	bu_flog(stderr, "%s", help ? help : "");
	bu_free(help, "bw-ps help");
	return 1;
    }
    if (args.help) {
	char *help = bu_arg_schema_help(&bw_ps_schema, "bw-ps");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bw-ps help");
	return 0;
    }

    ofp = stdout;

    if (encapsulated) {
	xpoints = width;
	ypoints = height;
    } else {
	xpoints = outwidth * 72 + 0.5;
	ypoints = outheight * 72 + 0.5;
    }
    prolog(ofp, file_name, xpoints, ypoints);

    scans_per_patch = MAX_BYTES / width;
    if (scans_per_patch > height)
	scans_per_patch = height;
    bytes_per_patch = scans_per_patch * width;

    for (y = 0; y < height; y += scans_per_patch) {
	/* start a patch */
	fprintf(ofp, "save\n");
	fprintf(ofp, "%lu %lu 8 [%lu 0 0 %lu 0 %ld] {<\n ",
		(unsigned long)width, (unsigned long)scans_per_patch,		/* patch size */
		(unsigned long)width, (unsigned long)height,			/* total size = 1.0 */
		-(long)y);				/* patch y origin */

	/* data */
	num = 0;
	while (num < bytes_per_patch) {
	    fprintf(ofp, "%02x", getc(infp));
	    if ((++num % 32) == 0)
		fprintf(ofp, "\n ");
	}

	/* close this patch */
	fprintf(ofp, ">} image\n");
	fprintf(ofp, "restore\n");
    }

    postlog(ofp);
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
