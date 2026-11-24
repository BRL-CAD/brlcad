/*                      P I X C L U M P . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2026 United States Government as represented by
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
/** @file util/pixclump.c
 *
 * Quantize the color values in a PIX(5) stream to a set of specified
 * values
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/vls.h"

#define RED 0
#define GRN 1
#define BLU 2

#define PC_DEBUG_TABLE 0x01
#define PC_DEBUG_MATCH 0x02


/*
 * Global variables
 */
unsigned char (*color_tbl)[3];	/* Table of quantized colors */
int color_tbl_size;		/* Capacity of table */
int next_color;		/* Number of colors now in table */
static int debug = 0;

static void add_to_table (unsigned char *rgb)
{
    /*
     * Ensure that the color table can accommodate the new entry
     */
    if (next_color == color_tbl_size) {
	color_tbl_size *= 2;
	color_tbl = (unsigned char (*)[3])
	    bu_realloc((void *) color_tbl,
		       color_tbl_size * 3 * sizeof(unsigned char),
		       "color table");
    }
    VMOVE(color_tbl[next_color], rgb);
    ++next_color;
}


static void fill_table (const char *f_name)
{
    char *bp;
    FILE *fp;
    int line_nm;
    unsigned char rgb[3];
    struct bu_vls v = BU_VLS_INIT_ZERO;

    if ((fp = fopen(f_name, "rb")) == NULL)
	bu_exit(1, "Cannot open color file '%s'\n", f_name);

    for (line_nm = 1; bu_vls_gets(&v, fp) != -1;
	 ++line_nm, bu_vls_trunc(&v, 0))
    {
	for (bp = bu_vls_addr(&v); (*bp == ' ') || (*bp == '\t'); ++bp)
	    ;
	if ((*bp == '#') || (*bp == '\0'))
	    continue;
	if (! bu_str_to_rgb(bp, rgb)) {
	    fclose(fp);
	    bu_exit(1, "Illegal color: '%s' on line %d of file '%s'\n", bp, line_nm, f_name);
	}
	add_to_table(rgb);
    }
    fclose(fp);
}


static void print_table (void)
{
    int i;

    bu_log("-----------\nColor Table\n-----------\n");
    for (i = 0; i < next_color; ++i)
	bu_log("%3d %3d %3d\n",
	       color_tbl[i][RED], color_tbl[i][GRN], color_tbl[i][BLU]);
    bu_log("-----------\n");
}


/*
 * Returns the square of the Euclidean distance in RGB space
 * between a specified pixel (R/G/B triple) and a specified
 * entry in the color table.
 */
static int color_diff (unsigned char *pix, int i)
{
    return (
	(pix[RED] - color_tbl[i][RED]) * (pix[RED] - color_tbl[i][RED]) +
	(pix[GRN] - color_tbl[i][GRN]) * (pix[GRN] - color_tbl[i][GRN]) +
	(pix[BLU] - color_tbl[i][BLU]) * (pix[BLU] - color_tbl[i][BLU])
	);
}


struct pixclump_args {
    struct pixclump_args *self;
    int help;
    const char *color_file;
    int debug_flags;
};

static int
pixclump_color(struct bu_vls *msg, const char *arg, void *storage)
{
    unsigned char rgb[3];

    if (!bu_str_to_rgb(arg, rgb)) {
	bu_vls_printf(msg, "invalid color: %s\n", arg);
	return -1;
    }
    if (storage) {
	struct pixclump_args *args = *(struct pixclump_args **)storage;
	if (args) {
	    add_to_table(rgb);
	    args->color_file = NULL;
	}
    }
    return 0;
}

static int
pixclump_color_file(struct bu_vls *UNUSED(msg), const char *arg, void *storage)
{
    if (storage) {
	struct pixclump_args *args = *(struct pixclump_args **)storage;
	if (args) {
	    args->color_file = arg;
	    next_color = 0;
	}
    }
    return 0;
}

static int
pixclump_debug(struct bu_vls *msg, const char *arg, void *storage)
{
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 16, 0, INT_MAX))
	return -1;
    if (storage) {
	struct pixclump_args *args = *(struct pixclump_args **)storage;
	if (args)
	    args->debug_flags = value;
    }
    return 0;
}

static const struct bu_arg_option pixclump_options[] = {
    BU_ARG_FLAG("h", "help", struct pixclump_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    {"c", "color", "color", "R/G/B", "Add a quantization color",
	BU_ARG_VALUE_CUSTOM, BU_ARG_STORAGE_OFFSET(struct pixclump_args, self),
	pixclump_color, NULL, NULL, NULL, 1, 0, NULL, BU_ARG_REQUIRED, NULL,
	NULL, NULL, BU_ARG_VALUE_RANGE_NONE, BU_ARG_SHAPE_SCALAR},
    BU_ARG_CUSTOM("f", "color-file", struct pixclump_args, self,
	pixclump_color_file, "file", "Read quantization colors from a file"),
    BU_ARG_CUSTOM("x", "debug", struct pixclump_args, self,
	pixclump_debug, "hex-mask", "Enable diagnostic output"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixclump_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PIX input file; standard input when omitted", NULL),
    BU_ARG_OPERAND("output", BU_ARG_VALUE_FILE, 0, 1,
	"PIX output file; standard output when omitted", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixclump_schema = BU_ARG_SCHEMA(
    "pixclump", "Quantize PIX colors to a specified palette",
    pixclump_options, pixclump_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixclump_grammar = {
    &pixclump_schema, NULL, NULL
};


int
main (int argc, char **argv)
{
    const char *outf_name = NULL;	/*  "   "  output  "    */
    unsigned char pixbuf[3];	/* the current input pixel */
    FILE *infp = NULL;	/* input stream */
    FILE *outfp = NULL;	/* output  "    */
    int i;		/* dummy loop indices */
    int best_color;	/* index of best match to pixbuf */
    int best_diff;	/* error in best match */
    int this_diff;	/* pixel-color_tbl difference */
    struct pixclump_args args = {NULL, 0, NULL, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    args.self = &args;
    bu_setprogname(argv[0]);

    /*
     * Initialize the color table
     */
    color_tbl_size = 8;
    color_tbl = (unsigned char (*)[3])
	bu_malloc(color_tbl_size * 3 * sizeof(unsigned char), "color table");
    next_color = 0;

    int introspection = bu_arg_introspect(&msg, &pixclump_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixclump_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixclump_schema, "pixclump");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixclump help");
	return 0;
	}

    debug = args.debug_flags;
    int operand_count = argc - operand_index - 1;
    const char **operands = (const char **)(argv + operand_index + 1);
    infp = stdin;
    outfp = stdout;
    outf_name = "stdout";

    if (operand_count > 0 && (infp = fopen(operands[0], "rb")) == NULL)
	bu_exit(1, "Cannot open input file '%s'\n", operands[0]);
    if (operand_count > 1) {
	outf_name = operands[1];
	if ((outfp = fopen(outf_name, "wb")) == NULL)
	    bu_exit(1, "Cannot open output file '%s'\n", outf_name);
    }

    if (infp == stdin && isatty(fileno(stdin)))
	bu_exit(1, "pixclump requires an input file or redirected input\n");

    setmode(fileno(infp), O_BINARY);
    setmode(fileno(outfp), O_BINARY);

    /*
     * Ensure that the color table is nonempty
     */
    if (args.color_file)
	fill_table(args.color_file);
    if (next_color == 0)
	bu_exit(1, "pixclump: no colors specified\n");
    if (debug & PC_DEBUG_TABLE)
	print_table();

    while (fread((void *) pixbuf, 3 * sizeof(unsigned char), 1, infp) == 1) {
	best_color = 0;
	best_diff = color_diff(pixbuf, 0);
	for (i = 1; i < next_color; ++i) {
	    if ((this_diff = color_diff(pixbuf, i)) < best_diff) {
		best_color = i;
		best_diff = this_diff;
	    }
	    if (debug & PC_DEBUG_MATCH)
		bu_log("p=%3d/%3d/%3d, t=%d %3d/%3d/%3d,  b=%d, %3d/%3d/%3d\n",
		       pixbuf[RED], pixbuf[GRN], pixbuf[BLU],
		       i,
		       color_tbl[i][RED],
		       color_tbl[i][GRN],
		       color_tbl[i][BLU],
		       best_color,
		       color_tbl[best_color][RED],
		       color_tbl[best_color][GRN],
		       color_tbl[best_color][BLU]);
	}
	if (fwrite((void *) color_tbl[best_color],
		   3 * sizeof(unsigned char), 1, outfp) != 1)
	    bu_exit(1, "pixclump:  Error writing pixel to file '%s'\n", outf_name);
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
