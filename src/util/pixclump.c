/*                      P I X C L U M P . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2011 United States Government as represented by
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
#include "bu.h"


#define RED 0
#define GRN 1
#define BLU 2

#define PC_DEBUG_TABLE 0x01
#define PC_DEBUG_MATCH 0x02
#define PC_DEBUG_OUTPUT 0x04
#define OPT_STRING "c:f:x:?"


/*
 * Global variables
 */
unsigned char (*color_tbl)[3];	/* Table of quantized colors */
int color_tbl_size;		/* Capacity of table */
int next_color;		/* Number of colors now in table */
static int debug = 0;

static char usage[] = "\
Usage: 'pixclump [-c R/G/B] [-f color_file] [-x debug_flags]\n\
		 [infile.pix [outfile.pix]]'\n";


static void print_usage (void)
{
    bu_exit(1, "%s", usage);
}


static void print_debug_usage (void)
{
    static char *flag_denotation[] = {
	"",
	"color table",
	"finding best pixel match",
	"writing the pixels out",
	0
    };
    int i;

    bu_log("Debug bits and their meanings...\n");
    for (i = 1; (flag_denotation[i]) != 0; ++i)
	bu_log("0x%04x	%s\n", 1 << (i-1), flag_denotation[i]);

    print_usage();
}


static void add_to_table (unsigned char *rgb)
{
    /*
     * Ensure that the color table can accomodate the new entry
     */
    if (next_color == color_tbl_size) {
	color_tbl_size *= 2;
	color_tbl = (unsigned char (*)[3])
	    bu_realloc((genptr_t) color_tbl,
		       color_tbl_size * 3 * sizeof(unsigned char),
		       "color table");
    }
    VMOVE(color_tbl[next_color], rgb);
    ++next_color;
}


static void fill_table (char *f_name)
{
    char *bp;
    FILE *fp;
    int line_nm;
    unsigned char rgb[3];
    struct bu_vls v;

    if ((fp = fopen(f_name, "r")) == NULL)
	bu_exit(1, "Cannot open color file '%s'\n", bu_optarg);

    bu_vls_init(&v);
    for (line_nm = 1; bu_vls_gets(&v, fp) != -1;
	 ++line_nm, bu_vls_trunc(&v, 0))
    {
	for (bp = bu_vls_addr(&v); (*bp == ' ') || (*bp == '\t'); ++bp)
	    ;
	if ((*bp == '#') || (*bp == '\0'))
	    continue;
	if (! bu_str_to_rgb(bp, rgb))
	    bu_exit(1, "Illegal color: '%s' on line %d of file '%s'\n", bp, line_nm, f_name);
	add_to_table(rgb);
    }
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
 * C O L O R _ D I F F ()
 *
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


int
main (int argc, char **argv)
{
    char *cf_name = 0;	/* name of color file */
    char *inf_name;	/* name of input stream */
    char *outf_name = NULL;	/* "   " output "   */
    unsigned char pixbuf[3];	/* the current input pixel */
    FILE *infp = NULL;	/* input stream */
    FILE *outfp = NULL;	/* output "   */
    int ch;		/* current char in command line */
    int i;		/* dummy loop indices */
    unsigned char rgb[3];		/* Specified color */
    int best_color;	/* index of best match to pixbuf */
    int best_diff;	/* error in best match */
    int this_diff;	/* pixel-color_tbl difference */

    /*
     * Initialize the color table
     */
    color_tbl_size = 8;
    color_tbl = (unsigned char (*)[3])
	bu_malloc(color_tbl_size * 3 * sizeof(unsigned char), "color table");
    next_color = 0;

    /*
     * Process the command line
     */
    while ((ch = bu_getopt(argc, argv, OPT_STRING)) != -1)
	switch (ch) {
	    case 'c':
		if (! bu_str_to_rgb(bu_optarg, rgb)) {
		    bu_log("Illegal color: '%s'\n", bu_optarg);
		    print_usage();
		}
		add_to_table(rgb);
		cf_name = 0;
		break;
	    case 'f':
		cf_name = bu_optarg;
		next_color = 0;
		break;
	    case 'x':
		if (sscanf(bu_optarg, "%x", (unsigned int *) &debug) != 1) {
		    bu_log("Invalid debug-flag value: '%s'\n", bu_optarg);
		    print_debug_usage();
		}
		break;
	    case '?':
	    default:
		print_usage();
	}
    switch (argc - bu_optind) {
	case 0:
	    inf_name = "stdin";
	    infp = stdin;
	    /* Break intentionally missing */
	case 1:
	    outf_name = "stdout";
	    outfp = stdout;
	    /* Break intentionally missing */
	case 2:
	    break;
	default:
	    print_usage();
    }

    /*
     * Open input and output files, as necessary
     */
    if (infp == NULL) {
	inf_name = argv[bu_optind];
	if ((infp = fopen(inf_name, "r")) == NULL)
	    bu_exit(1, "Cannot open input file '%s'\n", inf_name);
	if (outfp == NULL) {
	    outf_name = argv[++bu_optind];
	    if ((outfp = fopen(outf_name, "w")) == NULL)
		bu_exit(1, "Cannot open output file '%s'\n", outf_name);
	}
    }

    /*
     * Ensure that infp is kosher,
     */
    if (infp == stdin) {
	if (isatty(fileno(stdin))) {
	    bu_log("FATAL: pixclump reads only from file or pipe\n");
	    print_usage();
	}
    }

    /*
     * Ensure that the color table is nonempty
     */
    if (cf_name != 0)
	fill_table(cf_name);
    if (next_color == 0) {
	bu_log("pixclump: No colors specified\n");
	print_usage();
    }
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
	if (fwrite((genptr_t) color_tbl[best_color],
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
