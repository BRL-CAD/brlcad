/*
 *			P I X C L U M P . C
 *
 *	Quantize the color values in a PIX(5) stream to
 *	a set of specified values
 *
 *  Author -
 *	Paul J. Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1997 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"		/* For getopt(), etc. */
#include "bu.h"

#define	RED			0
#define	GRN			1
#define	BLU			2

/*
 *	Global variables
 */
unsigned char	(*color_tbl)[3];	/* Table of quantized colors */
int		color_tbl_size;		/* Capacity of table */
int		next_color;		/* Number of colors now in table */
static int	debug = 0;
#define	PC_DEBUG_TABLE		0x01
#define	PC_DEBUG_MATCH		0x02
#define	PC_DEBUG_OUTPUT		0x04

static char usage[] = "\
Usage: 'pixclump [-c R/G/B] [-f color_file] [-x debug_flags]\n\
		 [infile.pix [outfile.pix]]'\n";
#define OPT_STRING	"c:f:x:?"

static void print_usage ()
{
    (void) bu_log("%s", usage);
}

static void print_debug_usage (void)
{
    static char	*flag_denotation[] =
		{
		    "",
		    "color table",
		    "finding best pixel match",
		    "writing the pixels out",
		    0
		};
    int		i;

    bu_log("Debug bits and their meanings...\n");
    for (i = 1; (flag_denotation[i]) != 0; ++i)
	bu_log("0x%04x	%s\n", 1 << (i-1), flag_denotation[i]);
}

static void add_to_table (red, grn, blu)

int	red;
int	grn;
int	blu;

{
    /*
     *	Ensure that the color table can accomodate the new entry
     */
    if (next_color == color_tbl_size)
    {
	color_tbl_size *= 2;
	color_tbl = (unsigned char (*)[3])
	    bu_realloc((genptr_t) color_tbl,
			color_tbl_size * 3 * sizeof(unsigned char),
			"color table");
    }
    color_tbl[next_color][RED] = red;
    color_tbl[next_color][GRN] = grn;
    color_tbl[next_color][BLU] = blu;
    ++next_color;
}

static void fill_table (f_name)

char	*f_name;

{
    char		*bp;
    FILE		*fp;
    int			line_nm;
    int 		red;
    int			grn;
    int			blu;
    int			len;
    struct bu_vls	v;

    if ((fp = fopen(f_name, "r")) == NULL)
    {
	bu_log ("Cannot open color file '%s'\n", optarg);
	exit (1);
    }

    bu_vls_init(&v);
    for (line_nm = 1; bu_vls_gets(&v, fp) != -1;
	++line_nm, bu_vls_trunc(&v, 0))
    {
	for (bp = bu_vls_addr(&v); (*bp == ' ') || (*bp == '\t'); ++bp)
	    ;
	if ((*bp == '#') || (*bp == '\0'))
	    continue;
	if (sscanf(bp, "%d%d%d", &red, &grn, &blu) != 3)
	{
	    bu_log("Invalid color: '%s' on line %d of file '%s'\n",
		bp, line_nm, f_name);
	    exit (1);
	}
	if ((red <   0) || (grn <   0) || (blu <   0)
	 || (red > 255) || (grn > 255) || (blu > 255))
	{
	    bu_log("Illegal color: %d %d %d on line %d of file '%s'\n",
		red, grn, blu, line_nm, f_name);
	    exit (1);
	}
	add_to_table(red, grn, blu);
    }
}

static void print_table ()

{
    int	i;

    printf("-----------\nColor Table\n-----------\n");
    for (i = 0; i < next_color; ++i)
	printf("%3d %3d %3d\n",
	    color_tbl[i][RED], color_tbl[i][GRN], color_tbl[i][BLU]);
    printf("-----------\n");
}

/*
 *		C O L O R _ D I F F ( )
 *
 *	Returns the square of the Euclidean distance in RGB space
 *	between a specified pixel (R/G/B triple) and a specified
 *	entry in the color table.
 */
static int color_diff (pix, i)

unsigned char	*pix;
int		i;

{
    unsigned char	*cte;	/* The specified entry in the color table */

    cte = color_tbl[i];

    return (
	(pix[RED] - cte[RED]) * (pix[RED] - cte[RED]) +
	(pix[GRN] - cte[GRN]) * (pix[GRN] - cte[GRN]) +
	(pix[BLU] - cte[BLU]) * (pix[BLU] - cte[BLU])
    );
}

main (argc, argv)

int	argc;
char	*argv[];

{
    char		*cf_name = 0;	/* name of color file */
    char		*inf_name;	/* name of input stream */
    char		*outf_name;	/*  "   "  output   "   */
    unsigned char	pixbuf[3];	/* the current input pixel */
    FILE		*infp = NULL;	/* input stream */
    FILE		*outfp = NULL;	/* output   "   */
    int			ch;		/* current char in command line */
    int			i, j;		/* dummy loop indices */
    int			red, grn, blu;	/* Specified color */
    int			best_color;	/* index of best match to pixbuf */
    int			best_diff;	/* error in best match */
    int			this_diff;	/* pixel-color_tbl difference */

    extern int	optind;			/* index from getopt(3C) */
    extern char	*optarg;		/* argument from getopt(3C) */

    /*
     *	Initialize the color table
     */
    color_tbl_size = 8;
    color_tbl = (unsigned char (*)[3])
	bu_malloc(color_tbl_size * 3 * sizeof(unsigned char), "color table");
    next_color = 0;

    /*
     *	Process the command line
     */
    while ((ch = getopt(argc, argv, OPT_STRING)) != EOF)
	switch (ch)
	{
	    case 'c':
		if (sscanf(optarg, "%d/%d/%d", &red, &grn, &blu) != 3)
		{
		    bu_log("Invalid color: '%s'\n", optarg);
		    print_usage();
		    exit (1);
		}
		if ((red <   0) || (grn <   0) || (blu <   0)
		 || (red > 255) || (grn > 255) || (blu > 255))
		{
		    bu_log("Illegal color: '%s'\n", optarg);
		    print_usage();
		    exit (1);
		}
		add_to_table(red, grn, blu);
		cf_name = 0;
		break;
	    case 'f':
		cf_name = optarg;
		next_color = 0;
		break;
	    case 'x':
		if (sscanf(optarg, "%x", &debug) != 1)
		{
		    bu_log("Invalid debug-flag value: '%s'\n", optarg);
		    print_usage();
		    print_debug_usage();
		    exit (1);
		}
		break;
	    case '?':
	    default:
		print_usage();
		exit (ch != '?');
	}
    switch (argc - optind)
    {
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
	    exit (1);
    }

    /*
     *	Open input and output files, as necessary
     */
    if (infp == NULL)
    {
	inf_name = argv[optind];
	if ((infp = fopen(inf_name, "r")) == NULL)
	{
	    bu_log ("Cannot open input file '%s'\n", inf_name);
	    exit (1);
	}
	if (outfp == NULL)
	{
	    outf_name = argv[++optind];
	    if ((outfp = fopen(outf_name, "w")) == NULL)
	    {
		bu_log ("Cannot open output file '%s'\n", outf_name);
		exit (1);
	    }
	}
    }

    /*
     *	Ensure that infp is kosher,
     */
    if (infp == stdin)
    {
	if (isatty(fileno(stdin)))
	{
	    bu_log("FATAL: pixclump reads only from file or pipe\n");
	    print_usage();
	    exit (1);
	}
    }

    /*
     *	Ensure that the color table is nonempty
     */
    if (cf_name != 0)
	fill_table(cf_name);
    if (next_color == 0)
    {
	bu_log("pixclump: No colors specified\n");
	print_usage();
	exit (1);
    }
    if (debug & PC_DEBUG_TABLE)
	print_table();

    while (fread((void *) pixbuf, 3 * sizeof(unsigned char), 1, infp) == 1)
    {
	best_color = 0;
	best_diff = color_diff(pixbuf, 0);
	for (i = 1; i < next_color; ++i)
	{
	    if ((this_diff = color_diff(pixbuf, i)) < best_diff)
	    {
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
	if (fwrite((const void *) color_tbl[best_color],
		    3 * sizeof(unsigned char), 1, outfp) != 1)
	{
	    bu_log("pixclump:  Error writing pixel to file '%s'\n", outf_name);
	    exit (1);
	}
    }
}
